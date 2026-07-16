#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "fingerprint.hpp"
#include "talent_trees.hpp"

extern "C" {
void init_gen_rand(uint32_t seed);
uint32_t gen_rand32(void);
uint32_t rand_div(uint32_t m);
}

// Fisher-Yates shuffle matching the Lua table.shuffle exactly.
// Lua is 1-indexed: for i = n, 2, -1 do j = 1 + rand_div(i); swap t[i], t[j]
// C++ 0-indexed: swap t[i-1] with t[rand_div(i)]
template <typename T> static void fisher_yates_shuffle(std::vector<T> &t) {
  int n = static_cast<int>(t.size());
  for (int i = n; i >= 2; i--) {
    uint32_t j = rand_div(static_cast<uint32_t>(i));
    std::swap(t[i - 1], t[j]);
  }
}

// Pop the first tree from the list that the character doesn't already know.
// Returns the tree ID learned, or empty string if the list is exhausted.
// `pos` tracks the current front index to avoid O(n) erases.
static const std::string &pop_tree(const std::vector<std::string> &list,
                                   size_t &pos,
                                   std::unordered_set<std::string> &known) {
  static const std::string empty;
  while (pos < list.size()) {
    const std::string &tt = list[pos++];
    if (known.count(tt) == 0) {
      known.insert(tt);
      return tt;
    }
  }
  return empty;
}

// Integer-indexed pop: returns the tree index, or UINT16_MAX if exhausted.
// `seen` is a bitset (one bit per tree index) for O(1) dedup.
static uint16_t pop_tree_idx(const std::vector<uint16_t> &list, size_t &pos,
                             std::vector<bool> &seen) {
  while (pos < list.size()) {
    uint16_t idx = list[pos++];
    if (!seen[idx]) {
      seen[idx] = true;
      return idx;
    }
  }
  return UINT16_MAX;
}

static UnlockConfig load_config(const char *path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    std::fprintf(stderr, "Error: cannot open unlock file: %s\n", path);
    std::exit(1);
  }

  auto j = nlohmann::json::parse(f);
  UnlockConfig config;

  config.version = j.value("version", "tome-1.7.6");
  config.is_dwarf = j.value("is_dwarf", false);

  if (j.contains("subclasses")) {
    for (auto &[key, val] : j["subclasses"].items()) {
      config.subclasses[key] = val.get<bool>();
    }
  }

  if (j.contains("trees")) {
    for (auto &[key, val] : j["trees"].items()) {
      config.trees[key] = val.get<bool>();
    }
  }

  if (j.contains("addons")) {
    for (auto &[key, val] : j["addons"].items()) {
      config.addons[key] = val.get<std::string>();
    }
  }

  return config;
}

// Search rules loaded from rules.json.
struct LevelRangeRule {
  int min_level;
  int max_level;
  std::vector<std::string> talents;
};

struct SearchRules {
  std::vector<LevelRangeRule> level_ranges;
};

static SearchRules load_rules(const char *path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    std::fprintf(stderr, "Error: cannot open rules file: %s\n", path);
    std::exit(1);
  }

  auto j = nlohmann::json::parse(f);
  SearchRules rules;

  if (j.contains("level_ranges")) {
    for (auto &entry : j["level_ranges"]) {
      LevelRangeRule rule;
      rule.min_level = entry.value("min", 1);
      rule.max_level = entry.value("max", 50);
      for (auto &v : entry["talents"]) {
        rule.talents.push_back(v.get<std::string>());
      }
      rules.level_ranges.push_back(std::move(rule));
    }
  }

  return rules;
}

static void run_generate(int argc, char *argv[]) {
  if (argc < 4) {
    std::fprintf(stderr, "Usage: %s generate <seed> <unlocks.json>\n", argv[0]);
    std::exit(1);
  }

  uint32_t seed = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10));
  auto config = load_config(argv[3]);

  // Step 1: Build the talent tree lists from unlock config
  auto lists = build_talent_trees(config);

  // Step 2: Sort both lists lexicographically
  std::sort(lists.class_trees.begin(), lists.class_trees.end());
  std::sort(lists.generic_trees.begin(), lists.generic_trees.end());

  // Step 3: Seed the PRNG
  init_gen_rand(seed);

  // Step 4: Fisher-Yates shuffle (class first, then generic - shared PRNG)
  fisher_yates_shuffle(lists.class_trees);
  fisher_yates_shuffle(lists.generic_trees);

  // Step 5: Pop trees on the unlock schedule.
  std::unordered_set<std::string> known;
  known.insert("technique/combat-training");

  struct Unlock {
    int level;
    std::string type;
    std::string tree;
  };
  std::vector<Unlock> schedule;
  size_t cpos = 0, gpos = 0;

  auto learn = [&](int level, const char *type) {
    bool is_class = (std::strcmp(type, "class") == 0);
    const auto &list = is_class ? lists.class_trees : lists.generic_trees;
    size_t &pos = is_class ? cpos : gpos;
    const std::string &tree = pop_tree(list, pos, known);
    if (!tree.empty()) {
      schedule.push_back({level, type, std::string(tree)});
    }
  };

  // Level 1: 3 class + 1 generic
  learn(1, "class");
  learn(1, "class");
  learn(1, "class");
  learn(1, "generic");

  // Levels 2-50
  for (int level = 2; level <= 50; level++) {
    if (level % 5 == 0)
      learn(level, "class");
    if (level % 10 == 2)
      learn(level, "generic");
  }

  // Compute addons fingerprint from auto-detected sources
  std::string fingerprint =
      compute_fingerprint(config.version, lists.addon_sources, config.addons);
  std::string full_seed = std::to_string(seed) + "-" + fingerprint;

  // Output
  std::printf("Wanderer Seed: %s\n", full_seed.c_str());
  std::printf("================================================\n\n");

  std::printf("Talent Tree Unlock Order:\n\n");
  std::printf("%-7s  %-8s  %s\n", "Level", "Type", "Tree");
  std::printf("-------  --------  ----------------------------------------\n");
  for (const auto &u : schedule) {
    std::printf("%-7d  %-8s  %s\n", u.level, u.type.c_str(), u.tree.c_str());
  }

  std::printf(
      "\nTotal: %zu class, %zu generic\n",
      std::count_if(schedule.begin(), schedule.end(),
                    [](const Unlock &u) { return u.type == "class"; }),
      std::count_if(schedule.begin(), schedule.end(),
                    [](const Unlock &u) { return u.type == "generic"; }));
}

static void run_search(int argc, char *argv[]) {
  if (argc < 4) {
    std::fprintf(stderr, "Usage: %s search <unlocks.json> <rules.json>\n",
                 argv[0]);
    std::exit(1);
  }

  auto config = load_config(argv[2]);
  auto rules = load_rules(argv[3]);

  // Build and sort the base talent tree lists once.
  auto base = build_talent_trees(config);
  std::sort(base.class_trees.begin(), base.class_trees.end());
  std::sort(base.generic_trees.begin(), base.generic_trees.end());

  // Build a combined string→index mapping for all unique tree IDs.
  std::unordered_map<std::string, uint16_t> tree_to_idx;
  auto intern = [&](const std::string &s) -> uint16_t {
    auto it = tree_to_idx.find(s);
    if (it != tree_to_idx.end())
      return it->second;
    uint16_t idx = static_cast<uint16_t>(tree_to_idx.size());
    tree_to_idx[s] = idx;
    return idx;
  };

  // Convert base lists to index arrays.
  std::vector<uint16_t> base_class_idx, base_generic_idx;
  for (const auto &s : base.class_trees)
    base_class_idx.push_back(intern(s));
  for (const auto &s : base.generic_trees)
    base_generic_idx.push_back(intern(s));

  uint16_t combat_training_idx = intern("technique/combat-training");

  // Convert level_range rules to indexed form.
  struct IndexedLevelRange {
    int min_level;
    int max_level;
    std::vector<uint16_t> talents;
  };
  std::vector<IndexedLevelRange> indexed_rules;
  for (const auto &rule : rules.level_ranges) {
    IndexedLevelRange ir;
    ir.min_level = rule.min_level;
    ir.max_level = rule.max_level;
    for (const auto &s : rule.talents)
      ir.talents.push_back(intern(s));
    indexed_rules.push_back(std::move(ir));
  }

  // Pre-compute the unlock schedule (level for each pop).
  // Each entry: {level, is_class}
  struct ScheduleStep {
    int level;
    bool is_class;
  };
  std::vector<ScheduleStep> schedule_template;
  // Level 1: 3 class + 1 generic
  schedule_template.push_back({1, true});
  schedule_template.push_back({1, true});
  schedule_template.push_back({1, true});
  schedule_template.push_back({1, false});
  // Levels 2-50
  for (int level = 2; level <= 50; level++) {
    if (level % 5 == 0)
      schedule_template.push_back({level, true});
    if (level % 10 == 2)
      schedule_template.push_back({level, false});
  }

  // Find the highest max_level across all rules so we can stop early.
  int max_needed_level = 0;
  for (const auto &r : indexed_rules)
    if (r.max_level > max_needed_level)
      max_needed_level = r.max_level;

  // Trim schedule_template to only include steps up to max_needed_level.
  while (!schedule_template.empty() &&
         schedule_template.back().level > max_needed_level)
    schedule_template.pop_back();

  int schedule_len = static_cast<int>(schedule_template.size());

  uint16_t num_trees = static_cast<uint16_t>(tree_to_idx.size());

  std::string fingerprint =
      compute_fingerprint(config.version, base.addon_sources, config.addons);

  constexpr uint32_t MAX_SEED = 2147483647;
  constexpr uint32_t TOTAL_SEEDS = MAX_SEED; // seeds 1..MAX_SEED
  constexpr uint64_t MAX_RESULTS = 100;

  unsigned nthreads = std::max(1u, std::thread::hardware_concurrency() / 2);
  std::fprintf(stderr, "Searching seeds 1..%u with %u threads ...\n", MAX_SEED,
               nthreads);

  std::atomic<uint64_t> found{0};
  std::atomic<bool> stop_flag{false};

  // Preallocate one results vector per thread.
  std::vector<std::vector<uint32_t>> results(nthreads);
  for (auto &v : results)
    v.reserve(MAX_RESULTS / nthreads + 1);

  uint32_t chunk = TOTAL_SEEDS / nthreads;

  std::vector<std::thread> threads;
  threads.reserve(nthreads);

  for (unsigned tid = 0; tid < nthreads; tid++) {
    uint32_t start = tid * chunk + 1;
    uint32_t end = (tid == nthreads - 1) ? MAX_SEED : (tid + 1) * chunk;

    threads.emplace_back([&, tid, start, end]() {
      // Thread-local working buffers.
      std::vector<uint16_t> class_idx(base_class_idx.size());
      std::vector<uint16_t> generic_idx(base_generic_idx.size());
      std::vector<bool> seen(num_trees, false);
      std::vector<int> rule_matched(indexed_rules.size());

      for (uint32_t seed = start; seed <= end; seed++) {
        if (stop_flag.load(std::memory_order_relaxed))
          return;

        std::memcpy(class_idx.data(), base_class_idx.data(),
                    base_class_idx.size() * sizeof(uint16_t));
        std::memcpy(generic_idx.data(), base_generic_idx.data(),
                    base_generic_idx.size() * sizeof(uint16_t));

        init_gen_rand(seed);
        fisher_yates_shuffle(class_idx);
        fisher_yates_shuffle(generic_idx);

        seen.assign(num_trees, false);
        seen[combat_training_idx] = true;
        std::fill(rule_matched.begin(), rule_matched.end(), 0);

        size_t cpos = 0, gpos = 0;
        bool failed = false;

        for (int si = 0; si < schedule_len; si++) {
          int level = schedule_template[si].level;
          auto &list = schedule_template[si].is_class ? class_idx : generic_idx;
          size_t &pos = schedule_template[si].is_class ? cpos : gpos;
          uint16_t tree = pop_tree_idx(list, pos, seen);
          if (tree == UINT16_MAX)
            continue;

          for (size_t ri = 0; ri < indexed_rules.size(); ri++) {
            const auto &rule = indexed_rules[ri];
            if (level < rule.min_level || level > rule.max_level)
              continue;
            for (uint16_t req : rule.talents) {
              if (tree == req) {
                rule_matched[ri]++;
                break;
              }
            }
          }
        }

        if (!failed) {
          for (size_t ri = 0; ri < indexed_rules.size(); ri++) {
            if (rule_matched[ri] <
                static_cast<int>(indexed_rules[ri].talents.size())) {
              failed = true;
              break;
            }
          }
        }
        if (failed)
          continue;

        uint64_t count = found.fetch_add(1, std::memory_order_relaxed);
        if (count >= MAX_RESULTS) {
          stop_flag.store(true, std::memory_order_relaxed);
          return;
        }
        results[tid].push_back(seed);
      }
    });
  }

  for (auto &t : threads)
    t.join();

  // Output results in seed order across all thread vectors.
  // Collect and sort since threads cover non-overlapping ascending ranges
  // but we want globally sorted output.
  std::vector<uint32_t> all_results;
  for (const auto &v : results)
    all_results.insert(all_results.end(), v.begin(), v.end());
  std::sort(all_results.begin(), all_results.end());

  for (uint32_t seed : all_results)
    std::printf("%u-%s\n", seed, fingerprint.c_str());

  std::fprintf(stderr, "Done. Found %zu matching seeds.\n", all_results.size());
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "Usage: %s generate <seed> <unlocks.json>\n"
                 "       %s search <unlocks.json> <rules.json>\n",
                 argv[0], argv[0]);
    return 1;
  }

  if (std::strcmp(argv[1], "search") == 0) {
    run_search(argc, argv);
  } else if (std::strcmp(argv[1], "generate") == 0) {
    run_generate(argc, argv);
  } else {
    // Legacy: treat first arg as seed, second as unlocks.json
    // Shift args to match generate's expected layout.
    std::fprintf(stderr,
                 "Usage: %s generate <seed> <unlocks.json>\n"
                 "       %s search <unlocks.json> <rules.json>\n",
                 argv[0], argv[0]);
    return 1;
  }

  return 0;
}

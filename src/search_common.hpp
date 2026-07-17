#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

// Shared work-queue and result sink that lets the CPU worker threads and the
// GPU driver cooperate on a single seed range. Every worker (CPU thread or GPU)
// pulls fixed-size seed chunks from one atomic cursor, so a faster device simply
// claims more chunks - the split self-balances with no fixed CPU/GPU ratio. The
// hit counter, early-stop flag, and print lock are shared so the max_results cap
// is global across both devices and output never interleaves.
struct SearchCoord {
  std::atomic<uint32_t> cursor;      // next unclaimed seed
  uint32_t seed_end;                 // inclusive
  uint32_t chunk;                    // seeds handed out per claim
  std::atomic<uint64_t> found{0};    // total hits seen (may exceed max_results)
  std::atomic<bool> stop{false};     // set once the cap is reached
  std::mutex print_mutex;
  uint64_t max_results;
  const std::string *fingerprint;

  SearchCoord(uint32_t start, uint32_t end, uint32_t chunk_, uint64_t maxr,
              const std::string *fp)
      : cursor(start), seed_end(end), chunk(chunk_), max_results(maxr),
        fingerprint(fp) {}

  // Claim the next [start, start+count) block. Returns false when the range is
  // exhausted or another worker has requested a stop.
  bool claim(uint32_t &start, uint32_t &count) {
    if (stop.load(std::memory_order_relaxed))
      return false;
    uint32_t s = cursor.fetch_add(chunk, std::memory_order_relaxed);
    if (s > seed_end)
      return false;
    uint64_t last = std::min<uint64_t>(static_cast<uint64_t>(s) + chunk - 1,
                                       seed_end);
    start = s;
    count = static_cast<uint32_t>(last - s + 1);
    return true;
  }

  // Record a matching seed, printing it (up to the cap) as it is found. Prints
  // are serialized and flushed so they stream cleanly when piped.
  void report(uint32_t seed) {
    uint64_t c = found.fetch_add(1, std::memory_order_relaxed);
    if (c >= max_results) {
      stop.store(true, std::memory_order_relaxed);
      return;
    }
    std::lock_guard<std::mutex> lock(print_mutex);
    std::printf("%u-%s\n", seed, fingerprint->c_str());
    std::fflush(stdout);
  }
};

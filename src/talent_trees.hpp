#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Talent tree IDs classified as generic (generic=true in newTalentType).
// All other talent trees referenced by subclasses are class trees.
//
// Derived from newTalentType definitions in game/modules/tome/data/talents/*/
inline bool is_generic_tree(const std::string &id) {
  static const std::unordered_set<std::string> generics = {
      "celestial/chants",
      "celestial/hymns",
      "celestial/light",
      "chronomancy/chronomancy",
      "chronomancy/energy",
      "chronomancy/fate-weaving",
      "chronomancy/spacetime-weaving",
      "corruption/curses",
      "corruption/hexes",
      "corruption/torment",
      "corruption/black-magic",
      "corruption/demonic-strength",
      "corruption/oppression",
      "corruption/vile-life",
      "cunning/lethality",
      "cunning/scoundrel",
      "cunning/survival",
      "cursed/cursed-form",
      "cursed/dark-sustenance",
      "cursed/gestures",
      "demented/beyond-sanity",
      "demented/calamity",
      "demented/doom",
      "psionic/augmented-mobility",
      "psionic/dream-forge",
      "psionic/dreaming",
      "psionic/feedback",
      "psionic/finer-energy-manipulations",
      "psionic/mentalism",
      "spell/aegis",
      "spell/conveyance",
      "spell/divination",
      "spell/necrosis",
      "spell/spectre",
      "spell/staff-combat",
      "spell/stone-alchemy",
      "steamtech/blacksmith",
      "steamtech/chemistry",
      "steamtech/engineering",
      "steamtech/physics",
      "technique/combat-training",
      "technique/conditioning",
      "technique/mobility",
      "technique/thuggery",
      "technique/unarmed-training",
      "wild-gift/antimagic",
      "wild-gift/call",
      "wild-gift/fungus",
      "wild-gift/harmony",
      "wild-gift/mindstar-mastery",
  };
  return generics.count(id) > 0;
}

// Returns the addon source for a talent tree ID, matching the game's
// tt_def.source field.  Trees defined by the base game return "@vanilla@".
// Trees defined by DLC addons return the addon short_name.
inline const char *tree_source(const std::string &id) {
  static const std::unordered_map<std::string, const char *> dlc_trees = {
      // Ashes of Urh'Rok (newTalentType definitions in data-ashes-urhrok)
      {"corruption/black-magic", "ashes-urhrok"},
      {"corruption/brutality", "ashes-urhrok"},
      {"corruption/demonic-pact", "ashes-urhrok"},
      {"corruption/demonic-strength", "ashes-urhrok"},
      {"corruption/doom-covenant", "ashes-urhrok"},
      {"corruption/doom-shield", "ashes-urhrok"},
      {"corruption/fearfire", "ashes-urhrok"},
      {"corruption/heart-of-fire", "ashes-urhrok"},
      {"corruption/infernal-combat", "ashes-urhrok"},
      {"corruption/oppression", "ashes-urhrok"},
      {"corruption/spellblaze", "ashes-urhrok"},
      {"corruption/torture", "ashes-urhrok"},
      {"corruption/wrath", "ashes-urhrok"},
      // Forbidden Cults (newTalentType definitions in data-cults)
      {"demented/beyond-sanity", "cults"},
      {"demented/calamity", "cults"},
      {"demented/chronophage", "cults"},
      {"demented/controlled-horrors", "cults"},
      {"demented/disfigured-face", "cults"},
      {"demented/doom", "cults"},
      {"demented/entropy", "cults"},
      {"demented/friend-of-the-worm", "cults"},
      {"demented/horrific-body", "cults"},
      {"demented/madness", "cults"},
      {"demented/nether", "cults"},
      {"demented/oblivion", "cults"},
      {"demented/path-of-horror", "cults"},
      {"demented/rift", "cults"},
      {"demented/scourge-drake", "cults"},
      {"demented/slow-death", "cults"},
      {"demented/tentacles", "cults"},
      {"demented/timethief", "cults"},
      {"demented/void", "cults"},
      // Embers of Rage (newTalentType definitions in data-orcs)
      {"psionic/action-at-a-distance", "orcs"},
      {"psionic/gestalt", "orcs"},
      {"psionic/psionic-fog", "orcs"},
      {"spell/undead-drake", "orcs"},
      {"steamtech/artillery", "orcs"},
      {"steamtech/automated-butchery", "orcs"},
      {"steamtech/automation", "orcs"},
      {"steamtech/avoidance", "orcs"},
      {"steamtech/battle-machinery", "orcs"},
      {"steamtech/battlefield-management", "orcs"},
      {"steamtech/blacksmith", "orcs"},
      {"steamtech/bullets-mastery", "orcs"},
      {"steamtech/butchery", "orcs"},
      {"steamtech/chemical-warfare", "orcs"},
      {"steamtech/chemistry", "orcs"},
      {"steamtech/demolition", "orcs"},
      {"steamtech/dread", "orcs"},
      {"steamtech/elusiveness", "orcs"},
      {"steamtech/engineering", "orcs"},
      {"steamtech/furnace", "orcs"},
      {"steamtech/gadgets", "orcs"},
      {"steamtech/gunner-training", "orcs"},
      {"steamtech/gunslinging", "orcs"},
      {"steamtech/heavy-weapons", "orcs"},
      {"steamtech/magnetism", "orcs"},
      {"steamtech/mecharachnid", "orcs"},
      {"steamtech/mechstar", "orcs"},
      {"steamtech/physics", "orcs"},
      {"steamtech/psytech-gunnery", "orcs"},
      {"steamtech/sawmaiming", "orcs"},
      {"steamtech/thoughts-of-iron", "orcs"},
      {"steamtech/turrets", "orcs"},
  };
  auto it = dlc_trees.find(id);
  if (it != dlc_trees.end())
    return it->second;
  return "@vanilla@";
}

// Unlock configuration loaded from JSON.
struct UnlockConfig {
  std::string version; // game version string (e.g. "1.7.6")
  bool is_dwarf = false;
  std::unordered_map<std::string, bool> subclasses; // subclass name -> unlocked
  std::unordered_map<std::string, bool> trees;      // unlock key -> unlocked
  // addon name -> version string (for non-vanilla addons that contribute trees)
  std::unordered_map<std::string, std::string> addons;
};

struct TalentTreeLists {
  std::vector<std::string> class_trees;
  std::vector<std::string> generic_trees;
  std::unordered_set<std::string> addon_sources;
};

// Per-subclass static data.
struct SubclassInfo {
  const char *name;
  bool not_on_random_boss;
  // nullptr-terminated list of talent tree IDs from talents_types
  const char *const *trees;
  // nullptr-terminated list of {tree_id, unlock_key} pairs from
  // unlockable_talents_types (flattened: tree, key, tree, key, ..., nullptr)
  const char *const *unlockable_trees;
};

// clang-format off

// Talent tree lists for each subclass.
static const char* const trees_berserker[] = {
    "technique/2hweapon-assault", "technique/strength-of-the-berserker",
    "technique/combat-techniques-active", "technique/combat-techniques-passive",
    "technique/combat-training", "technique/conditioning",
    "technique/superiority", "technique/warcries", "technique/bloodthirst",
    "cunning/survival", "cunning/dirty", nullptr
};
static const char* const trees_bulwark[] = {
    "technique/shield-offense", "technique/shield-defense",
    "technique/combat-techniques-active", "technique/combat-techniques-passive",
    "technique/combat-training", "technique/conditioning",
    "technique/superiority", "technique/warcries", "technique/battle-tactics",
    "cunning/survival", "cunning/dirty", nullptr
};
static const char* const trees_archer[] = {
    "technique/archery-training", "technique/archery-utility",
    "technique/marksmanship", "technique/reflexes",
    "technique/combat-techniques-active", "technique/combat-techniques-passive",
    "technique/sniper", "technique/agility",
    "technique/combat-training", "cunning/trapping",
    "cunning/survival", "technique/mobility", "technique/conditioning", nullptr
};
static const char* const unlock_archer[] = {
    "cunning/poisons", "rogue_poisons", nullptr
};
static const char* const trees_arcane_blade[] = {
    "spell/fire", "spell/air", "spell/earth", "spell/stone",
    "spell/conveyance", "spell/aegis", "spell/enhancement",
    "technique/superiority", "technique/combat-techniques-active",
    "technique/combat-techniques-passive", "technique/combat-training",
    "technique/magical-combat", "technique/shield-offense",
    "technique/2hweapon-assault", "technique/dualweapon-attack",
    "cunning/survival", "cunning/dirty", nullptr
};
static const char* const trees_brawler[] = {
    "cunning/dirty", "cunning/tactical", "cunning/survival",
    "technique/combat-training", "technique/combat-techniques-active",
    "technique/combat-techniques-passive",
    "technique/pugilism", "technique/finishing-moves",
    "technique/grappling", "technique/unarmed-discipline",
    "technique/unarmed-training", "technique/conditioning",
    "technique/mobility", nullptr
};
static const char* const trees_rogue[] = {
    "technique/dualweapon-attack", "technique/duelist",
    "technique/combat-techniques-active", "technique/combat-training",
    "technique/mobility", "technique/throwing-knives",
    "technique/assassination",
    "cunning/stealth", "cunning/trapping", "cunning/lethality",
    "cunning/survival", "cunning/scoundrel", "cunning/dirty",
    "cunning/artifice", nullptr
};
static const char* const unlock_rogue[] = {
    "cunning/poisons", "rogue_poisons", nullptr
};
static const char* const trees_shadowblade[] = {
    "spell/phantasm", "spell/temporal", "spell/divination",
    "spell/conveyance",
    "technique/dualweapon-attack", "technique/duelist",
    "technique/combat-techniques-active", "technique/combat-techniques-passive",
    "technique/combat-training", "technique/mobility",
    "cunning/stealth", "cunning/survival", "cunning/lethality",
    "cunning/dirty", "cunning/shadow-magic", "cunning/ambush", nullptr
};
static const char* const trees_marauder[] = {
    "technique/dualweapon-attack", "technique/duelist",
    "technique/combat-techniques-active", "technique/combat-techniques-passive",
    "technique/combat-training", "technique/battle-tactics",
    "technique/mobility", "technique/thuggery", "technique/conditioning",
    "technique/bloodthirst", "technique/throwing-knives",
    "cunning/dirty", "cunning/tactical", "cunning/survival", nullptr
};
static const char* const unlock_marauder[] = {
    "cunning/poisons", "rogue_poisons", nullptr
};
static const char* const trees_skirmisher[] = {
    "technique/skirmisher-slings", "technique/buckler-training",
    "cunning/called-shots", "technique/tireless-combatant",
    "cunning/trapping",
    "technique/mobility", "cunning/survival",
    "technique/combat-training", "cunning/scoundrel", nullptr
};
static const char* const unlock_skirmisher[] = {
    "cunning/poisons", "rogue_poisons", nullptr
};
static const char* const trees_alchemist[] = {
    "spell/explosives", "spell/golemancy", "spell/advanced-golemancy",
    "spell/stone-alchemy", "spell/fire-alchemy", "spell/acid-alchemy",
    "spell/frost-alchemy", "spell/energy-alchemy",
    "spell/staff-combat", "cunning/survival", nullptr
};
static const char* const trees_archmage[] = {
    "spell/arcane", "spell/aether", "spell/fire", "spell/wildfire",
    "spell/earth", "spell/stone", "spell/water", "spell/ice",
    "spell/air", "spell/storm", "spell/phantasm", "spell/temporal",
    "spell/meta", "spell/divination", "spell/conveyance", "spell/aegis",
    "cunning/survival", nullptr
};
static const char* const trees_necromancer[] = {
    "spell/master-of-bones", "spell/master-of-flesh",
    "spell/master-necromancer", "spell/grave", "spell/glacial-waste",
    "spell/rime-wraith", "spell/nightfall", "spell/dreadmaster",
    "spell/age-of-dusk", "spell/death", "spell/animus",
    "spell/eradication", "spell/spectre", "spell/necrosis",
    "cunning/survival", nullptr
};
static const char* const trees_cursed[] = {
    "cursed/gloom", "cursed/slaughter", "cursed/endless-hunt",
    "cursed/strife", "cursed/cursed-form",
    "technique/combat-training", "cunning/survival",
    "cursed/rampage", "cursed/predator", "cursed/fears", nullptr
};
static const char* const trees_doomed[] = {
    "cursed/dark-sustenance", "cursed/force-of-will", "cursed/gestures",
    "cursed/punishments", "cursed/shadows", "cursed/darkness",
    "cursed/cursed-form", "cunning/survival", "cursed/fears",
    "cursed/one-with-shadows", "cursed/advanced-shadowmancy", nullptr
};
static const char* const trees_sun_paladin[] = {
    "technique/shield-offense", "technique/2hweapon-assault",
    "technique/combat-techniques-active", "technique/combat-training",
    "cunning/survival", "technique/combat-techniques-passive",
    "celestial/sun", "celestial/chants", "celestial/combat",
    "celestial/light", "celestial/guardian", "celestial/radiance",
    "celestial/crusader", nullptr
};
static const char* const trees_anorithil[] = {
    "cunning/survival", "celestial/sunlight", "celestial/chants",
    "celestial/glyphs", "celestial/circles", "celestial/eclipse",
    "celestial/light", "celestial/twilight", "celestial/hymns",
    "celestial/star-fury", nullptr
};
static const char* const trees_reaver[] = {
    "technique/combat-training", "cunning/survival",
    "corruption/sanguisuge", "corruption/reaving-combat",
    "corruption/scourge", "corruption/plague", "corruption/hexes",
    "corruption/curses", "corruption/bone", "corruption/torment",
    "corruption/vim", "corruption/rot", "corruption/vile-life", nullptr
};
static const char* const trees_corruptor[] = {
    "cunning/survival", "corruption/sanguisuge", "corruption/hexes",
    "corruption/curses", "corruption/bone", "corruption/plague",
    "corruption/shadowflame", "corruption/blood", "corruption/vim",
    "corruption/blight", "corruption/torment", nullptr
};
static const char* const trees_paradox_mage[] = {
    "chronomancy/gravity", "chronomancy/matter",
    "chronomancy/spacetime-folding", "chronomancy/speed-control",
    "chronomancy/timetravel",
    "chronomancy/flux", "chronomancy/spellbinding",
    "chronomancy/stasis", "chronomancy/timeline-threading",
    "chronomancy/chronomancy", "chronomancy/fate-weaving",
    "chronomancy/spacetime-weaving",
    "chronomancy/energy", "cunning/survival", nullptr
};
static const char* const trees_temporal_warden[] = {
    "chronomancy/blade-threading", "chronomancy/bow-threading",
    "chronomancy/guardian", "chronomancy/spacetime-folding",
    "chronomancy/speed-control", "chronomancy/temporal-combat",
    "chronomancy/stasis", "chronomancy/threaded-combat",
    "chronomancy/temporal-hounds",
    "technique/combat-training", "chronomancy/chronomancy",
    "chronomancy/spacetime-weaving",
    "chronomancy/fate-weaving", "cunning/survival", nullptr
};
static const char* const trees_mindslayer[] = {
    "psionic/absorption", "psionic/projection", "psionic/psi-fighting",
    "psionic/focus", "psionic/voracity",
    "psionic/augmented-mobility", "psionic/augmented-striking",
    "psionic/finer-energy-manipulations",
    "psionic/kinetic-mastery", "psionic/thermal-mastery",
    "psionic/charged-mastery",
    "cunning/survival", "technique/combat-training", nullptr
};
static const char* const trees_solipsist[] = {
    "psionic/distortion", "psionic/dream-smith",
    "psionic/psychic-assault", "psionic/slumber",
    "psionic/solipsism", "psionic/thought-forms",
    "psionic/dreaming", "psionic/feedback", "psionic/mentalism",
    "cunning/survival",
    "psionic/discharge", "psionic/dream-forge",
    "psionic/nightmare", nullptr
};
static const char* const trees_summoner[] = {
    "wild-gift/call", "wild-gift/harmony",
    "wild-gift/summon-melee", "wild-gift/summon-distance",
    "wild-gift/summon-utility", "wild-gift/summon-augmentation",
    "wild-gift/summon-advanced", "wild-gift/mindstar-mastery",
    "technique/combat-techniques-active",
    "cunning/survival", "technique/combat-training", nullptr
};
static const char* const trees_wyrmic[] = {
    "wild-gift/call", "wild-gift/harmony",
    "wild-gift/sand-drake", "wild-gift/fire-drake",
    "wild-gift/cold-drake", "wild-gift/storm-drake",
    "wild-gift/venom-drake", "wild-gift/higher-draconic",
    "wild-gift/fungus", "cunning/survival",
    "technique/shield-offense", "technique/2hweapon-assault",
    "technique/combat-techniques-active", "technique/combat-training", nullptr
};
static const char* const trees_oozemancer[] = {
    "cunning/survival", "wild-gift/call", "wild-gift/antimagic",
    "wild-gift/mindstar-mastery", "wild-gift/mucus", "wild-gift/ooze",
    "wild-gift/fungus", "wild-gift/oozing-blades",
    "wild-gift/corrosive-blades", "wild-gift/moss",
    "wild-gift/eyals-fury", "wild-gift/slime", nullptr
};
static const char* const trees_stone_warden[] = {
    "wild-gift/call", "wild-gift/earthen-power",
    "wild-gift/earthen-vines", "wild-gift/dwarven-nature",
    "spell/stone-alchemy", "spell/eldritch-stone",
    "spell/eldritch-shield", "spell/deeprock",
    "spell/earth", "spell/stone",
    "cunning/survival", "technique/combat-training", nullptr
};
// Wyrmic unlockable trees from DLCs
static const char* const unlock_wyrmic[] = {
    "spell/undead-drake", "wyrmic_undead",
    "demented/scourge-drake", "wyrmic_scourge",
    nullptr
};
// DLC: Ashes of Urh'Rok
static const char* const trees_doombringer[] = {
    "cunning/survival",
    "corruption/shadowflame", "corruption/hexes",
    "technique/combat-techniques-active", "technique/combat-techniques-passive",
    "technique/combat-training",
    "corruption/demonic-strength", "corruption/fearfire",
    "corruption/oppression", "corruption/heart-of-fire",
    "corruption/brutality", "corruption/wrath", "corruption/torture",
    nullptr
};
static const char* const trees_demonologist[] = {
    "corruption/spellblaze", "corruption/doom-covenant",
    "corruption/torment", "corruption/shadowflame",
    "corruption/infernal-combat", "corruption/demonic-pact",
    "corruption/doom-shield", "corruption/black-magic",
    "cunning/survival", "technique/combat-training",
    "technique/combat-techniques-active", "technique/combat-techniques-passive",
    "technique/shield-offense",
    nullptr
};
// DLC: Forbidden Cults
static const char* const trees_writhing_one[] = {
    "demented/tentacles", "demented/path-of-horror",
    "demented/slow-death", "demented/horrific-body",
    "demented/disfigured-face", "demented/friend-of-the-worm",
    "demented/controlled-horrors", "demented/beyond-sanity",
    "technique/combat-training", "cunning/survival",
    nullptr
};
static const char* const trees_cultist_of_entropy[] = {
    "demented/nether", "demented/madness",
    "demented/void", "demented/entropy",
    "demented/timethief", "demented/oblivion",
    "demented/chronophage", "demented/rift",
    "demented/doom", "demented/calamity",
    "demented/beyond-sanity",
    "technique/combat-training", "cunning/survival",
    nullptr
};
// DLC: Embers of Rage
static const char* const trees_sawbutcher[] = {
    "steamtech/butchery", "steamtech/sawmaiming",
    "steamtech/battlefield-management", "steamtech/battle-machinery",
    "steamtech/automated-butchery", "steamtech/furnace",
    "steamtech/chemistry", "steamtech/physics",
    "steamtech/blacksmith", "steamtech/engineering",
    "cunning/survival", "technique/combat-training",
    nullptr
};
static const char* const trees_gunslinger[] = {
    "steamtech/gunner-training", "steamtech/gunslinging",
    "steamtech/bullets-mastery", "steamtech/avoidance",
    "steamtech/automation", "steamtech/elusiveness",
    "steamtech/chemistry", "steamtech/physics",
    "steamtech/blacksmith", "steamtech/engineering",
    "cunning/survival", "technique/combat-training",
    nullptr
};
static const char* const trees_psyshot[] = {
    "steamtech/avoidance",
    "steamtech/chemistry", "steamtech/physics",
    "steamtech/blacksmith", "steamtech/engineering",
    "psionic/nightmare",
    "cunning/survival", "technique/combat-training",
    "psionic/gestalt", "steamtech/psytech-gunnery",
    "steamtech/thoughts-of-iron", "steamtech/mechstar",
    "psionic/action-at-a-distance", "psionic/psionic-fog",
    "steamtech/dread",
    nullptr
};
static const char* const trees_annihilator[] = {
    "steamtech/magnetism", "steamtech/heavy-weapons",
    "steamtech/gadgets", "steamtech/demolition",
    "steamtech/turrets", "steamtech/mecharachnid",
    "steamtech/artillery", "steamtech/chemical-warfare",
    "steamtech/chemistry", "steamtech/physics",
    "steamtech/blacksmith", "steamtech/engineering",
    "cunning/survival", "technique/combat-training",
    nullptr
};

static const char* const no_unlockables[] = { nullptr };

// clang-format on

// All subclasses with their metadata.
// not_on_random_boss: if true, excluded unless special exception applies.
// Stone Warden has not_on_random_boss=true but is included when race is Dwarf.
static const SubclassInfo all_subclasses[] = {
    // Always-available subclasses (no locked function)
    {"Berserker", false, trees_berserker, no_unlockables},
    {"Bulwark", false, trees_bulwark, no_unlockables},
    {"Archer", false, trees_archer, unlock_archer},
    {"Arcane Blade", false, trees_arcane_blade, no_unlockables},
    {"Brawler", false, trees_brawler, no_unlockables},
    {"Rogue", false, trees_rogue, unlock_rogue},
    {"Shadowblade", false, trees_shadowblade, no_unlockables},
    {"Marauder", false, trees_marauder, unlock_marauder},
    {"Skirmisher", false, trees_skirmisher, unlock_skirmisher},
    {"Alchemist", false, trees_alchemist, no_unlockables},
    // Locked subclasses (require profile unlock)
    {"Archmage", false, trees_archmage, no_unlockables},
    {"Necromancer", false, trees_necromancer, no_unlockables},
    {"Cursed", false, trees_cursed, no_unlockables},
    {"Doomed", false, trees_doomed, no_unlockables},
    {"Sun Paladin", false, trees_sun_paladin, no_unlockables},
    {"Anorithil", false, trees_anorithil, no_unlockables},
    {"Reaver", false, trees_reaver, no_unlockables},
    {"Corruptor", false, trees_corruptor, no_unlockables},
    {"Paradox Mage", false, trees_paradox_mage, no_unlockables},
    {"Temporal Warden", false, trees_temporal_warden, no_unlockables},
    {"Mindslayer", false, trees_mindslayer, no_unlockables},
    {"Solipsist", false, trees_solipsist, no_unlockables},
    {"Summoner", false, trees_summoner, no_unlockables},
    {"Wyrmic", false, trees_wyrmic, unlock_wyrmic},
    {"Oozemancer", false, trees_oozemancer, no_unlockables},
    {"Stone Warden", true, trees_stone_warden, no_unlockables},
    // DLC: Ashes of Urh'Rok
    {"Doombringer", false, trees_doombringer, no_unlockables},
    {"Demonologist", false, trees_demonologist, no_unlockables},
    // DLC: Forbidden Cults
    {"Writhing One", false, trees_writhing_one, no_unlockables},
    {"Cultist of Entropy", false, trees_cultist_of_entropy, no_unlockables},
    // DLC: Embers of Rage
    {"Sawbutcher", false, trees_sawbutcher, no_unlockables},
    {"Gunslinger", false, trees_gunslinger, no_unlockables},
    {"Psyshot", false, trees_psyshot, no_unlockables},
    {"Annihilator", false, trees_annihilator, no_unlockables},
};

static constexpr int NUM_SUBCLASSES =
    sizeof(all_subclasses) / sizeof(all_subclasses[0]);

// Build the class and generic talent tree lists based on unlock config.
inline TalentTreeLists build_talent_trees(const UnlockConfig &config) {
  TalentTreeLists result;

  for (int i = 0; i < NUM_SUBCLASSES; i++) {
    const auto &sc = all_subclasses[i];

    // Check if this subclass is unlocked
    auto it = config.subclasses.find(sc.name);
    if (it == config.subclasses.end() || !it->second) {
      continue; // locked - no .def, skipped entirely
    }

    // Check not_on_random_boss filter
    // Stone Warden is the only one with this flag; included only for Dwarf
    if (sc.not_on_random_boss) {
      if (std::string(sc.name) == "Stone Warden" && config.is_dwarf) {
        // allowed
      } else {
        continue;
      }
    }

    // Add base talent trees
    for (int j = 0; sc.trees[j] != nullptr; j++) {
      std::string id = sc.trees[j];
      result.addon_sources.insert(tree_source(id));
      if (is_generic_tree(id)) {
        result.generic_trees.push_back(id);
      } else {
        result.class_trees.push_back(id);
      }
    }

    // Add unlockable talent trees (individually gated)
    for (int j = 0; sc.unlockable_trees[j] != nullptr; j += 2) {
      const char *tree_id = sc.unlockable_trees[j];
      const char *unlock_key = sc.unlockable_trees[j + 1];

      auto uit = config.trees.find(unlock_key);
      if (uit != config.trees.end() && uit->second) {
        std::string id = tree_id;
        result.addon_sources.insert(tree_source(id));
        if (is_generic_tree(id)) {
          result.generic_trees.push_back(id);
        } else {
          result.class_trees.push_back(id);
        }
      }
    }
  }

  return result;
}

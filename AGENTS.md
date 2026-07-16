# Wanderer Seed Finder

A C++17 CLI tool that brute-force searches for Tales of Maj'Eyal (ToME) Wanderer class game seeds
whose talent tree unlock order matches user-defined criteria. It reimplements the game's Lua-based
SFMT-19937 PRNG and Fisher-Yates shuffle to deterministically predict talent trees for any seed.

## Build & Run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/wanderer_seed generate <seed> unlocks.json        # show unlock order for one seed
./build/wanderer_seed search unlocks.json rules.json      # brute-force search seeds 1..2147483647
```

Or use `./run.sh` (builds then runs generate) / `./run.sh search` (builds then runs search). There
are no tests. nlohmann/json is fetched via CMake FetchContent (needs network on first configure).
The build does **not** depend on the game engine — the `t-engine4` submodule is reference-only and
need not be checked out.

## Architecture

- `src/main.cpp` — CLI entry point, search loop (multithreaded), generate mode, JSON config loading
- `src/talent_trees.hpp` — static subclass/talent-tree data and `build_talent_trees()` which filters by unlock config
- `src/fingerprint.hpp` — MD5-based addon fingerprint matching the game's seed string format
- `src/sfmt/` — vendored SFMT-19937 PRNG (C); thread-local state via `__thread`
- `src/md5.{c,h}` — vendored MD5 implementation used by fingerprint
- `unlocks.json` — which subclasses/trees the player has unlocked (input config)
- `rules.json` — search criteria: required talents within level ranges
- `t-engine4/` — ToME game engine as a git submodule, kept purely as a reference for the reimplemented algorithms

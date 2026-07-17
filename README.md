# Wanderer Seed Finder

A C++17 command-line tool that brute-force searches for [Tales of Maj'Eyal](https://te4.org) (ToME)
**Wanderer** class game seeds whose talent-tree unlock order matches user-defined criteria. It
reimplements the game's Lua-based SFMT-19937 PRNG and Fisher–Yates shuffle to deterministically
predict the talent trees a Wanderer will unlock for any given seed, without launching the game.

Last updated for game version 1.7.6.

## Build

Requires CMake ≥ 3.16 and a C++17 compiler. [nlohmann/json](https://github.com/nlohmann/json) is
downloaded automatically at configure time (needs network on the first build).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or use the helper: `./run.sh` (build + search) / `./run.sh generate <seed>` (build + generate).

## Configuration

- **`unlocks.json`** - your account's unlock profile: which subclasses and trees are unlocked,
  whether the character is a Dwarf, the ToME version, and installed addon versions. Set an entry to
  `false` to simulate a profile that hasn't unlocked it yet. Addons/DLC affect both the seed
  fingerprint and the pool of possible trees, so this must match your game for predictions to be
  correct.
- **`rules.json`** - search criteria: a list of `level_ranges`, each requiring
  that one of the listed `talents` be unlocked within the `[min, max]` level window. A seed matches
  only if every range is satisfied.

## Usage

```sh
# Brute-force search all seeds for ones matching rules.json.
# Search is capped to 100 results.
./build/wanderer_seed search unlocks.json rules.json

# Show the talent-tree unlock order for one seed:
./build/wanderer_seed generate <seed> unlocks.json
# Example:
./build/wanderer_seed generate 61742831 unlocks.json
```

## Using your GPU to speedup search

```sh
# Configure and build with OpenCL support
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWANDERER_ENABLE_OPENCL=ON
cmake --build build

# Run the search on GPU only
./build/wanderer_seed search unlocks.json rules.json --gpu

# Run the search on both CPU and GPU simultaneously
./build/wanderer_seed search unlocks.json rules.json --gpu --cpu
```

## How it works

The Wanderer class in ToME derives its randomized talent trees from the game seed. This tool
reimplements that derivation exactly:

- **SFMT-19937** - the same PRNG ToME uses (vendored, C).
- **Fisher–Yates shuffle** - matches Lua's `table.shuffle` step for step.
- **Seed fingerprint** - an MD5-based addon fingerprint reproduces the game's `<seed>-<hash>`
  seed-string format, so predictions line up with what the game actually rolls for your installed
  addon/DLC set.

Given your unlock profile (which subclasses / trees your account has unlocked), it can either print
the full unlock order for a single seed or search the entire `1..2147483647` seed space
(multithreaded) for seeds matching your rules.

## Project layout

```
src/main.cpp             CLI entry point, multithreaded search + generate, JSON config
src/talent_trees.hpp     static subclass / talent-tree data + unlock filtering
src/fingerprint.hpp      MD5-based addon fingerprint (seed-string format)
src/sfmt/                vendored SFMT-19937 PRNG (C)
src/md5.{c,h}            vendored MD5
unlocks.json             edit this with your personal unlock status
rules.json               example search rules
t-engine4/               game engine, as a git submodule (reference only)
```

## The t-engine4 submodule

`t-engine4/` points at the open-source T-Engine4 / ToME engine
(`https://git.net-core.org/tome/t-engine4.git`), pinned as a submodule purely as a reference for the
reimplemented algorithms - it is **not** required to build or run this tool. To fetch the engine
source locally:

```sh
git submodule update --init t-engine4
```

## License & attribution

Released under **GPL-3.0** (see [LICENSE](LICENSE)). ToME and the T-Engine4 are GPL-3.0, and the
talent-tree data in `src/talent_trees.hpp` is derived from the game's data files, so this tool is
distributed under the same terms.

Vendored third-party code retains its original licenses:

- **SFMT** (`src/sfmt/`) - © 2006–2007 Mutsuo Saito, Makoto Matsumoto and Hiroshima University.
  New BSD License.
- **MD5** (`src/md5.{c,h}`) - from the Lua `md5` library by Marcela Ozorio Suarez /
  Roberto Ierusalimschy et al. (MIT License).

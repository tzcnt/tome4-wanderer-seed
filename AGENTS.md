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
The build does **not** depend on the game engine - the `t-engine4` submodule is reference-only and
need not be checked out.

Modify unlocks.json to configure for the user's game-unlock status. Modify rules.json to specify the search criteria.

### CPU / GPU search backends

`search` can run on the CPU, on an optional OpenCL GPU backend, or on **both at once**. Select with
`--cpu` and/or `--gpu`; with no flag it defaults to `--cpu` alone. The GPU backend is **off at build
time** by default (no OpenCL SDK needed for a normal build) - enable it with `WANDERER_ENABLE_OPENCL`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWANDERER_ENABLE_OPENCL=ON && cmake --build build
./build/wanderer_seed search unlocks.json rules.json            # CPU (default)
./build/wanderer_seed search unlocks.json rules.json --gpu      # GPU only (prefers a GPU device)
./build/wanderer_seed search unlocks.json rules.json --cpu --gpu  # both, self-balancing
```

When both are used, CPU threads and the GPU driver pull seed chunks from one shared atomic cursor
(`src/search_common.hpp`), so the faster device simply claims more work - no fixed split, and the
`max_results` cap / output are shared. `WANDERER_CHUNK` (default ~1M) sets the chunk size (smaller =
better tail balance, larger = less overhead).

`--gpu` in a build compiled without `WANDERER_ENABLE_OPENCL` prints a rebuild hint and exits. The GPU
kernel (`src/search_gpu.hpp`) is a bit-exact port of the CPU algorithm; every mode was validated by
comparing the full match set against the CPU path over bounded seed ranges (identical down to millions
of matches).

The kernel is bound by its per-thread local-memory footprint (the ~2.5 KB SFMT state), not by
registers, so throughput is very sensitive to that footprint and to work-group size. Optional
NVIDIA/tuning env knobs: `WANDERER_GPU_LOCAL` (work-group size, default 32 - small groups maximize
occupancy here), `WANDERER_GPU_BATCH` (seeds per launch), `WANDERER_GPU_MAXREG`
(`-cl-nv-maxrregcount`), and `WANDERER_GPU_VERBOSE` (prints device, kernel resource usage, and the
build log).

`search` (both CPU and GPU) honors three optional env vars, useful for partial scans and for
cross-checking the two paths: `WANDERER_SEED_START` / `WANDERER_SEED_END` (default `1` /
`2147483647`) bound the seed range, and `WANDERER_MAX_RESULTS` (default `100`) caps how many hits are
printed before stopping.

## Architecture

- `src/main.cpp` - CLI entry point, search loop (multithreaded), generate mode, JSON config loading
- `src/search_common.hpp` - `SearchCoord`: the shared atomic seed-chunk queue + result sink used by the CPU threads and the GPU driver
- `src/search_gpu.hpp` - optional OpenCL backend (kernel source + host driver); compiled only when `WANDERER_ENABLE_OPENCL=ON`
- `src/talent_trees.hpp` - static subclass/talent-tree data and `build_talent_trees()` which filters by unlock config
- `src/fingerprint.hpp` - MD5-based addon fingerprint matching the game's seed string format
- `src/sfmt/` - vendored SFMT-19937 PRNG (C); thread-local state via `__thread`
- `src/md5.{c,h}` - vendored MD5 implementation used by fingerprint
- `unlocks.json` - which subclasses/trees the player has unlocked (input config)
- `rules.json` - search criteria: required talents within level ranges
- `t-engine4/` - ToME game engine as a git submodule, kept purely as a reference for the reimplemented algorithms

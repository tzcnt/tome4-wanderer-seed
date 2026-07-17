#pragma once

// Optional OpenCL GPU search path. Only compiled when WANDERER_ENABLE_OPENCL is
// defined (see CMake option WANDERER_ENABLE_OPENCL). The kernel reproduces the
// exact CPU algorithm bit-for-bit:
//   - SFMT-19937 init_gen_rand + period_certification + gen_rand_all
//   - rand_div rejection sampling
//   - Fisher-Yates shuffle of the class list then the generic list (shared PRNG)
//   - the level-based unlock schedule pop with per-tree dedup
//   - level_range rule matching
// Each work-item evaluates one seed. Matching seeds are appended to an output
// buffer via an atomic counter.

#ifdef WANDERER_ENABLE_OPENCL

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

#include "search_common.hpp"

// ---------------------------------------------------------------------------
// Kernel source (bit-exact port of the CPU algorithm).
// Config-dependent sizes are supplied as -D build options:
//   CLASS_N, GENERIC_N, CLASS_CAP, GENERIC_CAP, NUM_TREES, SEEN_WORDS,
//   SCHED_LEN, NUM_RULES, NUM_RULES_CAP
// ---------------------------------------------------------------------------
static const char *WANDERER_KERNEL_SRC = R"CLC(
// ---- SFMT-19937 parameters (SFMT-params19937.h) ----
#define SFMT_N    156
#define SFMT_N32  624
#define POS1      122
#define SL1       18
#define SL2       1
#define SR1       11
#define SR2       1
#define MSK1      0xdfffffefU
#define MSK2      0xddfecb7fU
#define MSK3      0xbffaffffU
#define MSK4      0xbffffff6U

// The 624-word state lives in a scalar private array (dynamically indexed, so
// it spills to local memory). Words consumed by the shuffle are cheap scalar
// reads. Two pure-ALU wins over the naive port: the recursion is modulo-free
// (rolling block indices), and period_certification is a popcount instead of a
// 128-iteration bit loop.

// Regenerate the state in place. The recursion needs blocks a=state[i],
// b=state[i+POS1], c=new[i-2], d=new[i-1]. c and d were just produced, so they
// are carried in registers instead of re-read from local memory each step -
// halving the per-block local reads (only a and b hit local memory now).
#define WSC_REC_STEP(BIDX)                                                     \
  do {                                                                         \
    int a = 4 * i, b = 4 * (BIDX);                                             \
    uint a0 = s[a], a1 = s[a+1], a2 = s[a+2], a3 = s[a+3];                     \
    ulong th = ((ulong)a3 << 32) | (ulong)a2;                                 \
    ulong tl = ((ulong)a1 << 32) | (ulong)a0;                                 \
    ulong oh = th << 8, ol = tl << 8; oh |= tl >> 56;                         \
    uint x0=(uint)ol, x1=(uint)(ol>>32), x2=(uint)oh, x3=(uint)(oh>>32);       \
    ulong th2 = ((ulong)c3 << 32) | (ulong)c2;                                \
    ulong tl2 = ((ulong)c1 << 32) | (ulong)c0;                                \
    ulong oh2 = th2 >> 8, ol2 = tl2 >> 8; ol2 |= th2 << 56;                    \
    uint y0=(uint)ol2, y1=(uint)(ol2>>32), y2=(uint)oh2, y3=(uint)(oh2>>32);   \
    uint n0 = a0 ^ x0 ^ ((s[b]   >> SR1) & MSK1) ^ y0 ^ (d0 << SL1);          \
    uint n1 = a1 ^ x1 ^ ((s[b+1] >> SR1) & MSK2) ^ y1 ^ (d1 << SL1);          \
    uint n2 = a2 ^ x2 ^ ((s[b+2] >> SR1) & MSK3) ^ y2 ^ (d2 << SL1);          \
    uint n3 = a3 ^ x3 ^ ((s[b+3] >> SR1) & MSK4) ^ y3 ^ (d3 << SL1);          \
    s[a]=n0; s[a+1]=n1; s[a+2]=n2; s[a+3]=n3;                                  \
    c0=d0; c1=d1; c2=d2; c3=d3;                                               \
    d0=n0; d1=n1; d2=n2; d3=n3;                                               \
  } while (0)

inline void gen_rand_all(__private uint *s) {
  int r1 = 4 * (SFMT_N - 2), r2 = 4 * (SFMT_N - 1);
  uint c0 = s[r1], c1 = s[r1+1], c2 = s[r1+2], c3 = s[r1+3];
  uint d0 = s[r2], d1 = s[r2+1], d2 = s[r2+2], d3 = s[r2+3];
  int i = 0;
  for (; i < SFMT_N - POS1; i++)
    WSC_REC_STEP(i + POS1);
  for (; i < SFMT_N; i++)
    WSC_REC_STEP(i + POS1 - SFMT_N);
}

inline uint gen_rand32(__private uint *s, __private int *idx) {
  if (*idx >= SFMT_N32) {
    gen_rand_all(s);
    *idx = 0;
  }
  return s[(*idx)++];
}

inline uint rand_div(__private uint *s, __private int *idx, uint m) {
  if (m <= 1) return 0;
  uint used = m;
  used |= used >> 1;
  used |= used >> 2;
  used |= used >> 4;
  used |= used >> 8;
  used |= used >> 16;
  uint r;
  do {
    r = gen_rand32(s, idx) & used;
  } while (r >= m);
  return r;
}

inline void sfmt_init(__private uint *s, uint seed) {
  s[0] = seed;
  for (int i = 1; i < SFMT_N32; i++) {
    s[i] = 1812433253U * (s[i-1] ^ (s[i-1] >> 30)) + (uint)i;
  }
  // parity = {1, 0, 0, 0x13c9e684}; inner is the parity of the set bits of
  // (word0 & 1) and (word3 & 0x13c9e684). The fixup, when needed, always flips
  // bit 0 of word 0.
  int inner = (int)(((s[0] & 1U) + popcount(s[3] & 0x13c9e684U)) & 1U);
  if (inner != 1) s[0] ^= 1U;
}

__kernel void wanderer_search(
    const uint seed_base,
    const uint seed_count,
    __global const ushort *base_class,
    __global const ushort *base_generic,
    const ushort combat_idx,
    __global const int *sched_level,
    __global const uchar *sched_is_class,
    __global const int *rule_min,
    __global const int *rule_max,
    __global const int *rule_toff,
    __global const int *rule_tcount,
    __global const ushort *rule_talents,
    __global uint *out_seeds,
    __global volatile uint *out_count) {

  uint gid = get_global_id(0);
  if (gid >= seed_count) return;
  uint seed = seed_base + gid;

  uint state[SFMT_N32];
  sfmt_init(state, seed);
  int idx = SFMT_N32;

  // Local mutable copies of the base lists for shuffling. LIST_T is uchar when
  // all tree indices fit in a byte (num_trees <= 256), else ushort - keeping
  // this per-thread scratch as small as possible eases the local-memory wall.
  LIST_T cls[CLASS_CAP];
  LIST_T gen[GENERIC_CAP];
  for (int k = 0; k < CLASS_N; k++)   cls[k] = (LIST_T)base_class[k];
  for (int k = 0; k < GENERIC_N; k++) gen[k] = (LIST_T)base_generic[k];

  // Fisher-Yates: class list first, then generic (shared PRNG stream).
  for (int i = CLASS_N; i >= 2; i--) {
    uint j = rand_div(state, &idx, (uint)i);
    LIST_T tmp = cls[i-1]; cls[i-1] = cls[j]; cls[j] = tmp;
  }
  for (int i = GENERIC_N; i >= 2; i--) {
    uint j = rand_div(state, &idx, (uint)i);
    LIST_T tmp = gen[i-1]; gen[i-1] = gen[j]; gen[j] = tmp;
  }

  // seen bitset (one bit per tree index), combat-training pre-marked.
  uint seen[SEEN_WORDS];
  for (int w = 0; w < SEEN_WORDS; w++) seen[w] = 0U;
  seen[combat_idx >> 5] |= (1U << (combat_idx & 31));

  int rmatch[NUM_RULES_CAP];
  for (int r = 0; r < NUM_RULES; r++) rmatch[r] = 0;

  uint cpos = 0, gpos = 0;

  for (int si = 0; si < SCHED_LEN; si++) {
    int level = sched_level[si];
    uchar is_class = sched_is_class[si];

    // pop_tree_idx: first not-yet-seen entry from the front of the list.
    uint tree = 0xFFFFU;
    if (is_class) {
      while (cpos < (uint)CLASS_N) {
        uint t = cls[cpos++];
        uint bit = 1U << (t & 31);
        if (!(seen[t >> 5] & bit)) { seen[t >> 5] |= bit; tree = t; break; }
      }
    } else {
      while (gpos < (uint)GENERIC_N) {
        uint t = gen[gpos++];
        uint bit = 1U << (t & 31);
        if (!(seen[t >> 5] & bit)) { seen[t >> 5] |= bit; tree = t; break; }
      }
    }
    if (tree == 0xFFFFU) continue;

    for (int ri = 0; ri < NUM_RULES; ri++) {
      if (level < rule_min[ri] || level > rule_max[ri]) continue;
      int off = rule_toff[ri];
      int cnt = rule_tcount[ri];
      for (int t = 0; t < cnt; t++) {
        if (tree == rule_talents[off + t]) { rmatch[ri]++; break; }
      }
    }
  }

  for (int ri = 0; ri < NUM_RULES; ri++) {
    if (rmatch[ri] < rule_tcount[ri]) return; // failed
  }

  uint pos = atomic_inc(out_count);
  if (pos < CAP) out_seeds[pos] = seed;
}
)CLC";

// ---------------------------------------------------------------------------
// Host driver
// ---------------------------------------------------------------------------

#define WSC_CHECK(err, what)                                                   \
  do {                                                                         \
    cl_int _e = (err);                                                         \
    if (_e != CL_SUCCESS) {                                                    \
      std::fprintf(stderr, "OpenCL error %d at %s\n", (int)_e, (what));        \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

// Pick a device: prefer a GPU on any platform, else fall back to any device.
inline cl_device_id wsc_pick_device() {
  cl_uint nplat = 0;
  clGetPlatformIDs(0, nullptr, &nplat);
  if (nplat == 0) {
    std::fprintf(stderr, "OpenCL: no platforms found\n");
    std::exit(1);
  }
  std::vector<cl_platform_id> plats(nplat);
  clGetPlatformIDs(nplat, plats.data(), nullptr);

  cl_device_id fallback = nullptr;
  for (cl_uint p = 0; p < nplat; p++) {
    cl_uint nd = 0;
    if (clGetDeviceIDs(plats[p], CL_DEVICE_TYPE_GPU, 0, nullptr, &nd) ==
            CL_SUCCESS &&
        nd > 0) {
      std::vector<cl_device_id> devs(nd);
      clGetDeviceIDs(plats[p], CL_DEVICE_TYPE_GPU, nd, devs.data(), nullptr);
      return devs[0];
    }
    if (!fallback) {
      cl_uint na = 0;
      if (clGetDeviceIDs(plats[p], CL_DEVICE_TYPE_ALL, 0, nullptr, &na) ==
              CL_SUCCESS &&
          na > 0) {
        std::vector<cl_device_id> devs(na);
        clGetDeviceIDs(plats[p], CL_DEVICE_TYPE_ALL, na, devs.data(), nullptr);
        fallback = devs[0];
      }
    }
  }
  if (!fallback) {
    std::fprintf(stderr, "OpenCL: no devices found\n");
    std::exit(1);
  }
  return fallback;
}

// Rules in the flattened form the kernel consumes.
struct GpuRules {
  std::vector<int> min_level;
  std::vector<int> max_level;
  std::vector<int> talent_offset;
  std::vector<int> talent_count;
  std::vector<uint16_t> talents; // flat
};

// Drives the GPU over seed chunks pulled from the shared coordinator. Runs
// alongside any CPU worker threads; both self-balance via coord.claim().
inline void run_search_gpu(const std::vector<uint16_t> &base_class_idx,
                           const std::vector<uint16_t> &base_generic_idx,
                           uint16_t combat_training_idx, uint16_t num_trees,
                           const std::vector<int> &sched_level,
                           const std::vector<uint8_t> &sched_is_class,
                           const GpuRules &rules, SearchCoord &coord) {
  const uint32_t class_n = static_cast<uint32_t>(base_class_idx.size());
  const uint32_t generic_n = static_cast<uint32_t>(base_generic_idx.size());
  const uint32_t sched_len = static_cast<uint32_t>(sched_level.size());
  const uint32_t num_rules = static_cast<uint32_t>(rules.min_level.size());
  const uint32_t seen_words = (num_trees + 31u) / 32u;

  // One chunk yields at most `chunk` matches, so the output buffer is sized to
  // the coordinator's chunk size.
  uint32_t cap = coord.chunk;
  if (cap == 0) cap = 1;

  cl_int err;
  cl_device_id dev = wsc_pick_device();
  char dname[256] = {0}, pname[256] = {0};
  clGetDeviceInfo(dev, CL_DEVICE_NAME, sizeof(dname), dname, nullptr);
  cl_platform_id plat;
  clGetDeviceInfo(dev, CL_DEVICE_PLATFORM, sizeof(plat), &plat, nullptr);
  clGetPlatformInfo(plat, CL_PLATFORM_NAME, sizeof(pname), pname, nullptr);

  cl_context ctx = clCreateContext(nullptr, 1, &dev, nullptr, nullptr, &err);
  WSC_CHECK(err, "clCreateContext");
  cl_command_queue queue = clCreateCommandQueue(ctx, dev, 0, &err);
  WSC_CHECK(err, "clCreateCommandQueue");

  cl_program prog =
      clCreateProgramWithSource(ctx, 1, &WANDERER_KERNEL_SRC, nullptr, &err);
  WSC_CHECK(err, "clCreateProgramWithSource");

  char opts[640];
  int optlen = std::snprintf(
      opts, sizeof(opts),
      "-cl-std=CL1.2 -D CLASS_N=%u -D GENERIC_N=%u -D CLASS_CAP=%u "
      "-D GENERIC_CAP=%u -D NUM_TREES=%u -D SEEN_WORDS=%u -D SCHED_LEN=%u "
      "-D NUM_RULES=%u -D NUM_RULES_CAP=%u -D CAP=%u -D LIST_T=%s",
      class_n, generic_n, class_n < 1 ? 1u : class_n,
      generic_n < 1 ? 1u : generic_n, (uint32_t)num_trees, seen_words,
      sched_len, num_rules, num_rules < 1 ? 1u : num_rules, cap,
      num_trees <= 256 ? "uchar" : "ushort");
  // NVIDIA-specific occupancy tuning (harmless/ignored on other platforms).
  if (const char *mr = std::getenv("WANDERER_GPU_MAXREG"))
    optlen += std::snprintf(opts + optlen, sizeof(opts) - optlen,
                            " -cl-nv-maxrregcount=%d", atoi(mr));
  if (std::getenv("WANDERER_GPU_VERBOSE"))
    optlen += std::snprintf(opts + optlen, sizeof(opts) - optlen,
                            " -cl-nv-verbose");

  err = clBuildProgram(prog, 1, &dev, opts, nullptr, nullptr);
  if (err != CL_SUCCESS) {
    size_t logsz = 0;
    clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logsz);
    std::vector<char> log(logsz + 1, 0);
    clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, logsz, log.data(),
                          nullptr);
    std::fprintf(stderr, "OpenCL build failed:\n%s\n", log.data());
    std::exit(1);
  }
  if (std::getenv("WANDERER_GPU_VERBOSE")) {
    size_t logsz = 0;
    clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logsz);
    std::vector<char> log(logsz + 1, 0);
    clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, logsz, log.data(),
                          nullptr);
    std::fprintf(stderr, "OpenCL build log:\n%s\n", log.data());
  }
  cl_kernel kern = clCreateKernel(prog, "wanderer_search", &err);
  WSC_CHECK(err, "clCreateKernel");

  if (std::getenv("WANDERER_GPU_VERBOSE")) {
    cl_ulong privmem = 0, locmem = 0;
    size_t maxwg = 0, wgmult = 0;
    clGetKernelWorkGroupInfo(kern, dev, CL_KERNEL_PRIVATE_MEM_SIZE,
                             sizeof(privmem), &privmem, nullptr);
    clGetKernelWorkGroupInfo(kern, dev, CL_KERNEL_LOCAL_MEM_SIZE,
                             sizeof(locmem), &locmem, nullptr);
    clGetKernelWorkGroupInfo(kern, dev, CL_KERNEL_WORK_GROUP_SIZE,
                             sizeof(maxwg), &maxwg, nullptr);
    clGetKernelWorkGroupInfo(kern, dev,
                             CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                             sizeof(wgmult), &wgmult, nullptr);
    std::fprintf(stderr,
                 "kernel resources: private/spill=%llu B, local=%llu B, "
                 "max_wg=%zu, wg_mult=%zu\n",
                 (unsigned long long)privmem, (unsigned long long)locmem, maxwg,
                 wgmult);
  }

  // Helper to create a read-only device buffer from host data.
  auto mkbuf = [&](const void *data, size_t bytes) -> cl_mem {
    if (bytes == 0) bytes = 1; // avoid zero-size allocation
    cl_mem b = clCreateBuffer(ctx,
                              CL_MEM_READ_ONLY | (data ? CL_MEM_COPY_HOST_PTR : 0),
                              bytes, const_cast<void *>(data), &err);
    WSC_CHECK(err, "clCreateBuffer");
    return b;
  };

  cl_mem bClass = mkbuf(base_class_idx.data(),
                        base_class_idx.size() * sizeof(uint16_t));
  cl_mem bGeneric = mkbuf(base_generic_idx.data(),
                          base_generic_idx.size() * sizeof(uint16_t));
  cl_mem bSchedLvl = mkbuf(sched_level.data(), sched_level.size() * sizeof(int));
  cl_mem bSchedCls =
      mkbuf(sched_is_class.data(), sched_is_class.size() * sizeof(uint8_t));
  cl_mem bRMin = mkbuf(rules.min_level.data(),
                       rules.min_level.size() * sizeof(int));
  cl_mem bRMax = mkbuf(rules.max_level.data(),
                       rules.max_level.size() * sizeof(int));
  cl_mem bROff = mkbuf(rules.talent_offset.data(),
                       rules.talent_offset.size() * sizeof(int));
  cl_mem bRCnt = mkbuf(rules.talent_count.data(),
                       rules.talent_count.size() * sizeof(int));
  cl_mem bRTal =
      mkbuf(rules.talents.data(), rules.talents.size() * sizeof(uint16_t));

  cl_mem bOutSeeds = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY,
                                    cap * sizeof(uint32_t), nullptr, &err);
  WSC_CHECK(err, "clCreateBuffer(out_seeds)");
  cl_mem bOutCount = clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof(uint32_t),
                                    nullptr, &err);
  WSC_CHECK(err, "clCreateBuffer(out_count)");

  uint32_t zero = 0;
  WSC_CHECK(clEnqueueWriteBuffer(queue, bOutCount, CL_TRUE, 0, sizeof(uint32_t),
                                 &zero, 0, nullptr, nullptr),
            "init out_count");

  // Static kernel args (positions 2.. are fixed across batches).
  cl_ushort combat = combat_training_idx;
  clSetKernelArg(kern, 2, sizeof(cl_mem), &bClass);
  clSetKernelArg(kern, 3, sizeof(cl_mem), &bGeneric);
  clSetKernelArg(kern, 4, sizeof(cl_ushort), &combat);
  clSetKernelArg(kern, 5, sizeof(cl_mem), &bSchedLvl);
  clSetKernelArg(kern, 6, sizeof(cl_mem), &bSchedCls);
  clSetKernelArg(kern, 7, sizeof(cl_mem), &bRMin);
  clSetKernelArg(kern, 8, sizeof(cl_mem), &bRMax);
  clSetKernelArg(kern, 9, sizeof(cl_mem), &bROff);
  clSetKernelArg(kern, 10, sizeof(cl_mem), &bRCnt);
  clSetKernelArg(kern, 11, sizeof(cl_mem), &bRTal);
  clSetKernelArg(kern, 12, sizeof(cl_mem), &bOutSeeds);
  clSetKernelArg(kern, 13, sizeof(cl_mem), &bOutCount);

  std::fprintf(stderr, "  GPU device: %s / %s\n", pname, dname);
  if (std::getenv("WANDERER_GPU_VERBOSE"))
    std::fprintf(
        stderr,
        "  config: class_n=%u generic_n=%u num_trees=%u sched_len=%u "
        "(base shuffle draws ~= %u)\n",
        class_n, generic_n, (uint32_t)num_trees, sched_len,
        (class_n ? class_n - 1 : 0) + (generic_n ? generic_n - 1 : 0));

  // Small work-groups win big here: the per-thread local-memory footprint means
  // occupancy is maximized with ~1 warp per group. 32 measured fastest on
  // Ampere; override with WANDERER_GPU_LOCAL to retune for another device.
  size_t local = 32;
  if (const char *e = std::getenv("WANDERER_GPU_LOCAL"))
    local = static_cast<size_t>(std::strtoul(e, nullptr, 10));
  if (local == 0) local = 32;

  // Pull seed chunks from the shared coordinator until the range is exhausted
  // or the result cap is hit. One kernel launch per chunk.
  std::vector<uint32_t> matches;
  uint32_t seed_base, count;
  while (coord.claim(seed_base, count)) {
    uint32_t zero2 = 0;
    WSC_CHECK(clEnqueueWriteBuffer(queue, bOutCount, CL_FALSE, 0,
                                   sizeof(uint32_t), &zero2, 0, nullptr,
                                   nullptr),
              "reset out_count");
    clSetKernelArg(kern, 0, sizeof(uint32_t), &seed_base);
    clSetKernelArg(kern, 1, sizeof(uint32_t), &count);

    size_t global = ((count + local - 1) / local) * local;
    WSC_CHECK(clEnqueueNDRangeKernel(queue, kern, 1, nullptr, &global, &local,
                                     0, nullptr, nullptr),
              "clEnqueueNDRangeKernel");

    uint32_t total = 0;
    WSC_CHECK(clEnqueueReadBuffer(queue, bOutCount, CL_TRUE, 0,
                                  sizeof(uint32_t), &total, 0, nullptr, nullptr),
              "read out_count");

    uint32_t have = std::min(total, cap);
    if (have > 0) {
      matches.resize(have);
      WSC_CHECK(clEnqueueReadBuffer(queue, bOutSeeds, CL_TRUE, 0,
                                    have * sizeof(uint32_t), matches.data(), 0,
                                    nullptr, nullptr),
                "read out_seeds");
      std::sort(matches.begin(), matches.end());
      for (uint32_t s : matches)
        coord.report(s);
    }
  }
  clFinish(queue);

  clReleaseMemObject(bClass);
  clReleaseMemObject(bGeneric);
  clReleaseMemObject(bSchedLvl);
  clReleaseMemObject(bSchedCls);
  clReleaseMemObject(bRMin);
  clReleaseMemObject(bRMax);
  clReleaseMemObject(bROff);
  clReleaseMemObject(bRCnt);
  clReleaseMemObject(bRTal);
  clReleaseMemObject(bOutSeeds);
  clReleaseMemObject(bOutCount);
  clReleaseKernel(kern);
  clReleaseProgram(prog);
  clReleaseCommandQueue(queue);
  clReleaseContext(ctx);
}

#endif // WANDERER_ENABLE_OPENCL

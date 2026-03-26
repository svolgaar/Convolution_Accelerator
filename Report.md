Full Report


The CNN processes 256x256 images through 5 sequential (conv → maxpool) pairs, followed by 2 dense layers.

Each pixel in the 256x256 pixel image has 3 input channels (RGB). Each filter is of size 3x3 and is applied across all input channels of the layer. For layer 0 that means 3 channels, but for deeper layers the number of input channels equals the number of filters from the previous layer (e.g. layer 1 has 32 input channels because layer 0 outputs 32 filters).
The data is laid out as CHW (Channel x Height x Width).

Base line software version:

```cpp
void Conv2D(TFXP *input, TFXP * output, TFXP * filters,
      uint32_t numFilters, uint32_t numChannels,
      uint32_t inputWidth, uint32_t inputHeight,
      uint32_t convWidth, uint32_t convHeight)
{
  for (uint32_t iFilter = 0; iFilter < numFilters; ++ iFilter) {
    for (uint32_t y = 0; y < (inputHeight-2); ++y) {
      for (uint32_t x = 0; x < (inputWidth-2); ++ x) {
        TFXP acc;
        acc = 0;
        for (uint32_t iChannel = 0; iChannel < numChannels; ++ iChannel) {
          for (uint32_t cy = 0; cy < convHeight; ++ cy) {
            for (uint32_t cx = 0; cx < convWidth; ++cx) {
              //acc += filters[iFilter][iChannel][cy][cx] * input[iChannel][y+cy][x+cx];
              TFXP v, f;
              f = *(filters + iFilter*numChannels*convHeight*convWidth + iChannel*convHeight*convWidth + cy*convWidth + cx);
              v = *(input + iChannel*inputWidth*inputHeight + (y+cy)*inputWidth + (x+cx));
              acc += FXP_Mult(f, v, DECIMALS);
            }
          }
        }
        //output[iFilter][y][x] = acc;
        *(output + iFilter * (inputHeight-2)*(inputWidth-2) + y*(inputWidth-2) + x) = acc;
      }
    }
  }
}
```

The iteration is as follows:

Per filter at the layer of the convolution we go through a slightly reduced version of the input (due to no padding, output is inputSize - 2 in each dimension) where for each channel we convolve around said pixel by convHeight*convWidth (filter size) then doing a MAC operation. The number of MACs per output pixel is numChannels x convHeight x convWidth (= numChannels x 9). For layer 0 this is 3 x 9 = 27, but for deeper layers it grows: layer 1 = 288, layer 2 = 576, layer 3 = 1152, layer 4 = 2304.



Implementing HW version

Fundamentally the minimal version can be written exactly like the SW version with the caveat that we need to indicate PRAGMAS
 
 the following were used for the simplest minimal implementation:

 ```cpp
#pragma HLS INTERFACE m_axi port=input bundle=master1 offset=slave depth=196608 max_read_burst_length=256 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=output bundle=master1 offset=slave depth=2064512 max_write_burst_length=256 num_write_outstanding=16
#pragma HLS INTERFACE m_axi port=filters bundle=master2 offset=slave depth=27648 max_read_burst_length=256 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=biases bundle=master2 offset=slave depth=256 max_read_burst_length=256 num_read_outstanding=16
#pragma HLS INTERFACE s_axilite port=input
#pragma HLS INTERFACE s_axilite port=output
#pragma HLS INTERFACE s_axilite port=filters
#pragma HLS INTERFACE s_axilite port=biases
#pragma HLS INTERFACE s_axilite port=numChannels
#pragma HLS INTERFACE s_axilite port=numFilters
#pragma HLS INTERFACE s_axilite port=inputWidth
#pragma HLS INTERFACE s_axilite port=inputHeight
#pragma HLS INTERFACE s_axilite port=convWidth
#pragma HLS INTERFACE s_axilite port=convHeight
#pragma HLS INTERFACE s_axilite port=relu
#pragma HLS INTERFACE s_axilite port=return
 ```
HLS INTERFACE pragmas tell the synthesizer how each function argument is communicated between the ARM CPU (PS) and the FPGA accelerator (PL).

- **m_axi** (Master AXI): a high-performance port through which the accelerator reads/writes bulk data from/to DDR. However, the accelerator doesn't inherently know *which* DDR addresses to access.
- **s_axilite** (Slave AXI-Lite): a lightweight control bus through which the CPU writes configuration values (base addresses, scalar parameters) into registers on the accelerator. The `offset=slave` parameter on the m_axi pragma links the two: it tells HLS to read the base address for that port from the corresponding s_axilite register at runtime, rather than hardcoding it at synthesis time.

Pointer arguments (`input`, `output`, `filters`, `biases`) appear on both interfaces: s_axilite provides the base address, m_axi performs the actual data transfers. Scalar arguments (`numChannels`, `numFilters`, etc.) only need s_axilite since they are single values read directly from a register. `port=return` exposes the ap_start/ap_done/ap_idle control signals that the CPU uses to start the accelerator and poll for completion.

The `bundle` parameter groups ports onto physical AXI master interfaces. Here, `input` and `output` share `master1` (routed to HP0) while `filters` and `biases` share `master2` (routed to HP1). The Zynq has 4 HP ports, but only 2 are needed because the data accesses don't actually conflict: filter loading, row loading, computation (from BRAM), and output writing happen in separate phases. Splitting `input` and `filters` onto different bundles was necessary to achieve II=1 (one MAC per cycle) in the initial design, since the inner loop reads both simultaneously.


Relu and biases:


in the software implementation, both the Relu and biases were implemented in separate functions. as seen below:

```cpp
void ReLU(TFXP * input, uint32_t channels, uint32_t width, uint32_t height)
{
  for (uint32_t ii = 0; ii < channels*width*height; ++ ii) {
    if ( Fxp2Float(input[ii], DECIMALS) < 0.0 )
      input[ii] = 0;
  }
}

void AddBiases(TFXP * input, TFXP * biases, uint32_t channels, uint32_t width, uint32_t height)
{
  for (uint32_t iChannel = 0; iChannel < channels; ++ iChannel) {
    for (uint32_t iPixel = 0; iPixel < width * height; ++ iPixel) {
      *input = *input + *biases;
      ++ input;
    }
    ++ biases;
  }
}
```
ReLU simply clamps negative values to zero, introducing non-linearity into the network. Without it, stacking multiple convolution layers would be equivalent to a single linear transformation, limiting the network to only learning linear relationships.

The bias is a single scalar per filter that is added to (shifts) the convolution result for every output pixel of that filter. It gives each filter a learned baseline activation. ReLU is applied after the bias addition to ensure the final clamping.

In the SW version, these are separate functions that require an extra pass over the full output tensor in DDR. In the HW version, they are folded directly into the convolution loop, avoiding the extra DDR read/write pass:

```cpp
        // Add bias for this filter
        acc += bias;
        // Apply ReLU if enabled
        if (relu) {
          acc = (acc < 0) ? 0 : acc;
        }
```
The `biases` and `relu` arguments are exposed via s_axilite registers and must be declared in the ConvProxy with their register offsets (generated by Vitis HLS synthesis) just like the other arguments.

Filter cacheing and axilite port interfacing

In the minimal implementation, filter values are re-fetched from DDR on every MAC operation. For each output pixel, the same filter coefficients are re-read — this means `outputHeight × outputWidth` redundant reads of the entire filter per output filter. This prevents burst transfers and wastes DDR bandwidth.

The first improvements are:

1. **Splitting into two m_axi ports**: `input`/`output` on `master1` (HP0) and `filters`/`biases` on `master2` (HP1). This allows simultaneous reads of input and filter data, achieving II=1 in the inner loop.

2. **AXI interface tuning**:
```cpp
#pragma HLS INTERFACE  max_read_burst_length=256 num_read_outstanding=16
#pragma HLS INTERFACE  max_write_burst_length=256 num_write_outstanding=16
```
`max_read_burst_length=256` raises the maximum burst from the default 16 to 256 data beats (32-bit words). With 16, only 64 bytes are moved per burst; with 256, 1024 bytes are moved, and the address setup overhead is paid only once per burst. `num_read_outstanding=16` allows up to 16 burst requests to be in flight simultaneously — the accelerator can issue up to 16 bursts before waiting for a response, hiding DDR latency by keeping the memory controller's pipeline full.

3. **Filter caching into BRAM**: Each filter consists of `numChannels × 3 × 3` independent coefficients. The largest filter across all layers is 256 × 3 × 3 = 2304 values (layer 4), so we allocate a local BRAM array of that size. Before iterating over the spatial positions, the entire filter is copied into BRAM in a single sequential pass. The array is reused for each filter — only one filter's coefficients are stored at a time.

```cpp
  for (uint32_t iFilter = 0; iFilter < numFilters; ++ iFilter) {
    // Cache filter coefficients for this filter into local BRAM
    TFXP localFilters[256 * 3 * 3]; // Max: 256 channels x 3x3 kernel
    for (uint32_t i = 0; i < numChannels * convHeight * convWidth; ++ i) {
#pragma HLS PIPELINE II=1
      localFilters[i] = *(filters + iFilter*numChannels*convHeight*convWidth + i);
    }
  }
```

`#pragma HLS PIPELINE II=1` tells the synthesizer to pipeline this loop with an Initiation Interval of 1. The **Initiation Interval (II)** is the number of clock cycles between starting consecutive loop iterations. II=1 means a new iteration starts every single cycle — the loop body is split into pipeline stages so that while iteration N is in stage 2, iteration N+1 is already in stage 1. Without pipelining, each iteration would have to fully complete before the next begins, wasting cycles. II=1 is the theoretical maximum throughput for a loop.

By loading filter coefficients into BRAM once and then reading from BRAM (single-cycle access) during the convolution, we reduce DDR filter reads from `numFilters × outputH × outputW × numChannels × 9` to just `numFilters × numChannels × 9`. The sequential copy also enables HLS to infer a burst transfer, making the `max_read_burst_length=256` pragma effective for filter reads.


Input Caching and Loop-Flatten off to avoid negative slack

Similarly to the filter caching argument, the input values are being re-fetched from DDR every time, wasting bandwidth. We apply a similar solution of caching inputs into BRAM.

The key insight is that the convolution slides a 3×3 spatial window across the input: each output pixel at position (y, x) is computed from the 3×3 patch of input pixels at (y..y+2, x..x+2), across all channels. This means computing one full row of output (all x positions at a given y) requires exactly 3 input rows. When the y-loop advances by 1, the window shifts down — output row y+1 needs input rows y+1, y+2, y+3. Rows y+1 and y+2 are already in the buffer from the previous iteration, so only 1 new row needs to be loaded from DDR.

We therefore allocate 3 row buffers, each of size `numChannels × inputWidth` (max 32×128 = 4096), forming a circular buffer. After the initial 3-row load, each new output row only fetches 1 new input row, overwriting the slot that held the oldest (no longer needed) row:

```cpp
  for (uint32_t iFilter = 0; iFilter < numFilters; ++ iFilter) {
    // Cache filter coefficients for this filter into local BRAM
        TFXP rowBuffer[3][4096];        // 3 row buffers, max numChannels * inputWidth = 32*128 = 4096

    for (uint32_t row = 0; row < 3; ++ row) {
#pragma HLS LOOP_FLATTEN off
      for (uint32_t ch = 0; ch < numChannels; ++ ch) {
#pragma HLS LOOP_FLATTEN off
        uint32_t srcBase = ch * inputWidth * inputHeight + row * inputWidth;
        uint32_t dstBase = ch * inputWidth;
        for (uint32_t px = 0; px < inputWidth; ++ px) {
#pragma HLS PIPELINE II=1
          rowBuffer[row][dstBase + px] = *(input + srcBase + px);
        }
      }
    }
  }
```

Following the implementation, the buffering of 3 rows caused the auto flattening HLS to have negative slack as it lengthened the critical path. Therefore we turned off the Loop flattening to avoid negative timing slack. While it costs a few cycles at each boundary, its necessary for the slack.


Filter Parallelism

While the buffering has made data access considerably more efficient, we are still handling only one filter at a time. For each input pixel, computing it against multiple filters is completely independent work — every filter reads the same input pixel but multiplies by different coefficients. This means we can replicate the MAC hardware to process N filters simultaneously.

### Outer loop and active filter tracking

The outer filter loop now steps by `N_PARALLEL` instead of 1:

```cpp
const uint32_t N_PARALLEL = 4;

for (uint32_t iFilter = 0; iFilter < numFilters; iFilter += N_PARALLEL) {
    uint32_t nActive = ((iFilter + N_PARALLEL) <= numFilters) ? N_PARALLEL : (numFilters - iFilter);
```

`nActive` handles the edge case where `numFilters` isn't divisible by N. For example, with N=4 and 6 filters: the first group processes filters 0-3 (nActive=4), the second group processes filters 4-5 (nActive=2). Without this, the last group would read out-of-bounds filter data and write to invalid output addresses.

### Parallel filter storage

```cpp
TFXP localFilters[N][256 * 3 * 3];
#pragma HLS ARRAY_PARTITION variable=localFilters complete dim=1
```

We now need N separate copies of the filter BRAM — one per parallel filter. `complete dim=1` tells HLS to split the first dimension (the N) into N physically independent BRAM blocks, so all N filters can be read in the same clock cycle. Each block stores up to 256×3×3 coefficients (the largest single filter). Using `dim=2` would instead create 256×3×3 = 2304 individual registers per filter, massively exceeding the FPGA's flip-flop resources for no benefit.

The filter loading now loads N filters per group:
```cpp
for (uint32_t p = 0; p < N_PARALLEL; ++p) {
    if (p < nActive) {
        for (uint32_t i = 0; i < filterLen; ++i) {
#pragma HLS PIPELINE II=1
            localFilters[p][i] = *(filters + (iFilter + p)*filterLen + i);
        }
    }
}
```

Each filter is loaded sequentially from DDR into its own BRAM. The `if (p < nActive)` guard prevents out-of-bounds DDR reads in the last group.

### Parallel biases

```cpp
TFXP bias[4];
#pragma HLS ARRAY_PARTITION variable=bias complete
for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
    bias[p] = (p < nActive) ? biases[iFilter + p] : (TFXP)0;
}
```

Each filter has its own bias, so we need N biases loaded in parallel. `complete` partitioning (no dim needed for a 1D array) gives each bias its own register for simultaneous access. `#pragma HLS UNROLL` tells HLS to execute all N iterations in a single cycle by replicating the load hardware, rather than looping sequentially. Inactive filters get bias=0 to avoid garbage values affecting any downstream logic.

### Parallel MAC in the inner loop

```cpp
TFXP acc[4];
#pragma HLS ARRAY_PARTITION variable=acc complete

// Read pixel ONCE, multiply against all N filters
TFXP pixelValue = rowBuffer[(y + cy) % 3][iChannel * inputWidth + x + cx];
for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
    TFXP filterValue = localFilters[p][iChannel*convHeight*convWidth + cy*convWidth + cx];
    acc[p] += FXP_Mult(filterValue, pixelValue, DECIMALS);
}
```

This is the core of the parallelization. One input pixel is read from `rowBuffer`, then N independent multiply-accumulates happen simultaneously — each reading from its own `localFilters[p]` BRAM and accumulating into its own `acc[p]`. The `#pragma HLS UNROLL` creates N copies of the multiplier hardware in parallel. This is why `ARRAY_PARTITION complete dim=1` on `localFilters` is essential: without it, the N filter reads would compete for the same BRAM ports and become sequential, defeating the purpose.

### Parallel bias addition, ReLU, and output write

```cpp
for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
    acc[p] += bias[p];
    if (relu) {
        acc[p] = (acc[p] < 0) ? (TFXP)0 : acc[p];
    }
    if ((iFilter + p) < numFilters) {
        *(output + (iFilter + p) * (inputHeight-2)*(inputWidth-2) + y*(inputWidth-2) + x) = acc[p];
    }
}
```

After the MAC loop, all N accumulators are finalized in parallel: bias added, ReLU applied, and results written to DDR. The `if ((iFilter + p) < numFilters)` guard prevents writing past the valid output range for the last group. Each filter's output goes to a different location in DDR (different filter plane), so the N writes are to N scattered addresses.

### Scaling

The approach was tested at N=4 (12.9s → 4.9s, ~2.6× speedup) and N=8 (→ 3.4s). Resource usage scales roughly linearly with N — each doubling adds another set of filter BRAMs and multiplier DSPs. N=16 was later achieved, and N=32 was attempted but exceeded the xc7z020's 220 DSPs.


## 4-Buffer Prefetch (Overlapping Data Transfers with Computation)

### The problem with 3 buffers

With 3 row buffers and circular indexing `% 3`, a collision occurs: the buffer slot for the next row to prefetch (`(y+3) % 3`) equals the slot currently being read (`y % 3`). For example at y=0: `(0+3) % 3 = 0`, which is the same slot as `y % 3 = 0`. This forces the row loading and computation to be strictly sequential — the accelerator must wait for row loading to finish before computing, adding idle time.

### The 4-buffer solution

```cpp
// 3 buffers: (y+3) % 3 == y % 3 → CONFLICT (same slot)
// 4 buffers: (y+3) % 4 != y%4, (y+1)%4, (y+2)%4 → NO CONFLICT
TFXP rowBuffer[4][4096];
```

With 4 buffers, the prefetch of row `y+3` writes to buffer `(y+3) % 4` while the computation reads from `y%4`, `(y+1)%4`, `(y+2)%4` — all different slots. The initial load now fills 4 rows (0,1,2,3) instead of 3. This is handled with a guard for small inputs:

```cpp
uint32_t initRows = (inputHeight < 4) ? inputHeight : 4;
for (uint32_t row = 0; row < initRows; ++ row) { ... }
```

The prefetch during the y-loop only executes when there's actually a next row to fetch:

```cpp
if (y > 0 && (y + 3) < inputHeight) {
    uint32_t prefetchRow = y + 3;
    uint32_t bufIdx = prefetchRow % 4;
    // Load row into BRAM while computation uses the other 3 slots
}
```

### Hardware benefit of `% 4` vs `% 3`

`% 4` is a simple 2-bit mask in hardware (just take the lowest 2 bits of the address), whereas `% 3` requires actual division logic — a multi-cycle operation that lengthens the critical path. This reduced the compute pipeline iteration latency from 43 cycles to 11 cycles (II=1 maintained in both cases).

### Impact

- **Conv time**: 2.718s → 2.562s (5.7% improvement)
- **Total time**: 3.374s → 3.190s (5.5% improvement)
- **LUTs**: 19224 → 17117 — reduced due to simpler modulo logic
- **FFs**: 15686 → 12607 — fewer pipeline stages needed

### Separate output HP port (attempted and reverted)

In this same phase, output was moved to a dedicated 3rd HP port (`master3` → HP2) to test if read/write contention on `master1` was a bottleneck. Result: **identical runtime** (3.190s → 3.190s), confirming the design is compute-bound, not memory-bound. The extra AXI adapter logic only wasted LUTs, so it was reverted back to 2 ports.


## N=16 Filter Parallelism and Output Row Buffering

### Scaling to N=16

With N=8 using only 40% DSPs and 34% BRAM, there was headroom to double parallelism to 16. The changes are mechanical — increase `N_PARALLEL` from 8 to 16 and double all parallel array sizes:

```cpp
const uint32_t N_PARALLEL = 16;
TFXP localFilters[16][256 * 3 * 3];
TFXP bias[16];
TFXP acc[16];
```

Resources scaled to: BRAM 57%, DSP 62%, LUTs 41%. N=32 was attempted but exceeded the FPGA's 220 DSPs.

### The output write bottleneck

After going to N=16, profiling revealed a surprise: **output writes were the dominant bottleneck**, not the MAC computation. For Conv 0 (3 input channels), the compute loop takes only 27 MACs per pixel — but writing 16 output values to 16 different DDR addresses (one per filter's output plane) created 16 individual scattered AXI write transactions per pixel. Each transaction has setup overhead. For Conv 0, the output writes took roughly 12× longer than the actual computation.

### Output row buffering

Instead of writing each pixel directly to DDR, buffer a full output row per parallel filter in local BRAM:

```cpp
TFXP outRowBuf[16][256];    // 16 filters × max 254 output pixels
#pragma HLS ARRAY_PARTITION variable=outRowBuf complete dim=1
```

`complete dim=1` gives each of the 16 filters its own BRAM, so all 16 can be written in parallel from the unrolled `p` loop.

The computation loop now stores to the local buffer instead of DDR:

```cpp
// Inside the x-loop, after bias + ReLU:
outRowBuf[p][x] = acc[p];   // BRAM write, not DDR write
```

After the entire x-loop finishes for a given output row y, the buffered results are burst-written to DDR:

```cpp
for (uint32_t p = 0; p < nActive; ++p) {
    uint32_t dstBase = (iFilter + p) * outHeight * outWidth + y * outWidth;
    for (uint32_t x = 0; x < outWidth; ++x) {
#pragma HLS PIPELINE II=1
        *(output + dstBase + x) = outRowBuf[p][x];
    }
}
```

Each filter's output row is now a sequential burst of `outWidth` consecutive addresses — HLS can infer an efficient AXI burst transfer. This converts thousands of scattered single-beat writes into 16 sequential burst writes per output row.

Note that the loop uses `nActive` instead of `N_PARALLEL` with an `if` guard — this simplifies the loop structure and removes a conditional branch that was preventing HLS from optimizing the burst pattern.

### Removing `latency=20`

The `latency=20` parameter was also removed from all AXI pragmas in this commit. This hint was telling HLS to pessimistically assume 20 cycles of AXI round-trip latency, causing the scheduler to insert unnecessary pipeline bubbles. Removing it let HLS use its default (more aggressive) latency estimate and overlap AXI transactions more efficiently.

### Impact

| Layer | Before (ms) | After (ms) | Speedup |
|-------|-------------|------------|---------|
| Conv 0 (3ch→32f) | 716 | 100 | 7.2× |
| Conv 1 (32ch→64f) | 535 | 238 | 2.2× |
| Conv 2 (64ch→64f) | 350 | 215 | 1.6× |
| Conv 3 (64ch→128f) | 256 | 199 | 1.3× |
| Conv 4 (128ch→256f) | 25 | 23 | 1.1× |
| **Total Conv** | **1882** | **775** | **2.43×** |
| **Total** | **2509** | **1402** | **1.79×** |

Conv 0 saw the largest speedup (7.2×) because with only 3 input channels, its compute loop was very short and the scattered output writes had dominated execution time. Deeper layers with more channels already had compute-dominant profiles, so the output write optimization mattered less.

Resources actually **decreased** despite adding the output buffer BRAM, because removing `latency=20` let HLS share DSPs more efficiently: DSP 62% → 41%, LUTs 41% → 32%.


## Loop Flattening and Interleaved Filter Loading

### Overhead analysis

Cycle analysis showed a 2-5× overhead ratio between estimated compute cycles and actual runtime. Three sources were identified:

1. **Per-channel burst restart in row loading**: The `ch` and `px` loops were kept separate by `LOOP_FLATTEN off`, causing HLS to restart the AXI read pipeline at each channel boundary.
2. **Sequential filter loading**: Filters were loaded one at a time (outer `p` loop, inner `i` loop), causing 16 separate burst restarts per filter group.
3. **Conditional branch in output writes**: The `if ((iFilter+p) < numFilters)` guard prevented HLS from optimizing the burst.

### Changes

**1. Removing `LOOP_FLATTEN off` from row loading:**

Previously:
```cpp
for (uint32_t ch = 0; ch < numChannels; ++ch) {
#pragma HLS LOOP_FLATTEN off          // ← removed
    for (uint32_t px = 0; px < inputWidth; ++px) {
#pragma HLS PIPELINE II=1
        rowBuffer[row][ch * inputWidth + px] = *(input + ...);
    }
}
```

After removal, HLS merges the `ch` and `px` loops into a single flattened pipeline. The address computation still has per-channel strides (non-contiguous in CHW layout), but the loop control overhead at each channel boundary is eliminated.

**2. Interleaved filter loading:**

Previously filters were loaded sequentially — one complete filter at a time:
```cpp
// BEFORE: outer p, inner i — 16 separate bursts
for (uint32_t p = 0; p < N_PARALLEL; ++p) {
    for (uint32_t i = 0; i < filterLen; ++i) {
#pragma HLS PIPELINE II=1
        localFilters[p][i] = *(filters + (iFilter + p)*filterLen + i);
    }
}
```

Restructured to interleaved — all 16 filters loaded simultaneously from a single sequential read stream:
```cpp
// AFTER: outer i, inner p — single burst, all 16 BRAMs written simultaneously
for (uint32_t i = 0; i < filterLen; ++i) {
#pragma HLS PIPELINE II=1
    for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
        if (p < nActive)
            localFilters[p][i] = *(filters + (iFilter + p)*filterLen + i);
    }
}
```

The outer loop iterates over coefficient index `i`, and the inner unrolled `p` loop writes the same index position across all 16 filter BRAMs simultaneously. This eliminates 16 separate burst restarts — the DDR read stream is a single continuous sequential access pattern. Since `localFilters` is partitioned `complete dim=1`, all 16 BRAM writes happen in the same cycle.

**3. Simplified output write loop:**

Changed from `for (p < N_PARALLEL) { if (valid) { write } }` to `for (p < nActive) { write }`, removing the conditional branch.

### Impact

- **Total time**: 1402ms → 1397ms (0.4% improvement)

The changes produced a negligible improvement, confirming that the overhead was inherent AXI transaction setup and pipeline drain costs, not loop restart overhead. However, the interleaved filter loading is architecturally cleaner and was kept.

Note: `LOOP_FLATTEN off` was **re-added** in the next commit to fix negative timing slack caused by the flattened loop control logic lengthening the critical path. It was selectively applied: kept on row loading loops and the output burst write loop where it was needed for timing, but not on the filter loading (which uses the interleaved approach instead).


## 2× X-Position Parallelism

### The key insight

After all previous optimizations, DSP usage sat at only 92/220 (41%). The insight is that for two adjacent output x-positions, the **filter coefficient is identical** — the filter value depends on `(iChannel, cy, cx)`, not on the output x-position. So two adjacent output pixels can share the same filter read:

- Output pixel at (y, x): `filter[ch][cy][cx] * input[ch][y+cy][x+cx]`
- Output pixel at (y, x+1): `filter[ch][cy][cx] * input[ch][y+cy][x+1+cx]`

Same filter value, different input pixel. This means x-parallelism doubles DSP usage without duplicating filter BRAM storage.

### Changes

**X-loop steps by 2 with dual accumulators:**

```cpp
for (uint32_t x = 0; x < outWidth; x += 2) {
    TFXP acc0[16], acc1[16];    // Two sets of accumulators
#pragma HLS ARRAY_PARTITION variable=acc0 complete
#pragma HLS ARRAY_PARTITION variable=acc1 complete
```

**Inner MAC reads two adjacent pixels, multiplies both by the same filter value:**

```cpp
uint32_t rowIdx = (y + cy) % 4;
uint32_t baseAddr = iChannel * inputWidth + x + cx;
TFXP pix0 = rowBuffer[rowIdx][baseAddr];       // pixel for x
TFXP pix1 = rowBuffer[rowIdx][baseAddr + 1];   // pixel for x+1
for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
    TFXP filterValue = localFilters[p][...];   // read ONCE
    acc0[p] += FXP_Mult(filterValue, pix0, DECIMALS);  // output x
    acc1[p] += FXP_Mult(filterValue, pix1, DECIMALS);  // output x+1
}
```

One filter read, two input reads, two multiplications per filter — 32 MACs per cycle across all 16 parallel filters.

**`rowBuffer` partitioned for dual reads:**

```cpp
TFXP rowBuffer[4][4096];
#pragma HLS ARRAY_PARTITION variable=rowBuffer cyclic factor=2 dim=2
```

`cyclic factor=2` places even-indexed elements in bank 0 and odd-indexed in bank 1. Since `pix0` and `pix1` are at adjacent addresses (one even, one odd), they are guaranteed to be in different banks — enabling conflict-free parallel reads in the same cycle.

**`outRowBuf` also partitioned for dual writes:**

```cpp
TFXP outRowBuf[16][256];
#pragma HLS ARRAY_PARTITION variable=outRowBuf complete dim=1
#pragma HLS ARRAY_PARTITION variable=outRowBuf cyclic factor=2 dim=2
```

Both output pixels `outRowBuf[p][x]` and `outRowBuf[p][x+1]` must be written simultaneously. Again, `cyclic factor=2` ensures adjacent indices map to different BRAM banks.

**Bias and ReLU applied to both accumulators:**

```cpp
for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
    acc0[p] += bias[p];
    acc1[p] += bias[p];
    if (relu) {
        acc0[p] = (acc0[p] < 0) ? (TFXP)0 : acc0[p];
        acc1[p] = (acc1[p] < 0) ? (TFXP)0 : acc1[p];
    }
    outRowBuf[p][x] = acc0[p];
    outRowBuf[p][x + 1] = acc1[p];
}
```

### Impact

| Layer | Before (ms) | After (ms) | Speedup |
|-------|-------------|------------|---------|
| Conv 0 (3ch→32f) | 99 | 73 | 1.36× |
| Conv 1 (32ch→64f) | 236 | 142 | 1.66× |
| Conv 2 (64ch→64f) | 211 | 127 | 1.66× |
| Conv 3 (64ch→128f) | 198 | 125 | 1.58× |
| Conv 4 (128ch→256f) | 26 | 19 | 1.37× |
| **Total Conv** | **770** | **485** | **1.59×** |
| **Total** | **1397** | **1116** | **1.25×** |

The 1.59× Conv speedup is close to the theoretical 2× from halving the x-loop iterations. The gap is due to non-compute phases (filter loading, row prefetch, output burst writes) which are unchanged.

Resources: DSPs 92 → 140 (41% → 63%), BRAM 176 → 192 (62% → 68%). The FPGA is now approaching its resource limits — further x-parallelism (x=3 or x=4) would exceed the 220 DSP budget.

### Why this is more efficient than kernel parallelism

Parallelizing across kernel positions (multiple cy/cx at once) would also double DSPs but would additionally require:
- Multiple filter coefficient reads per cycle (more BRAM partitioning)
- Multiple input pixel reads from non-adjacent addresses (complex partitioning)
- An adder tree to merge partial accumulations (extra logic)

X-parallelism avoids all of this: one filter read is shared, and the two input pixels are always adjacent (trivially handled by `cyclic factor=2`).


## Fixing Negative Slack

After the loop flattening changes, the design had negative timing slack — the critical path exceeded the 10ns clock period. The `LOOP_FLATTEN off` pragmas were re-added to the row loading loops and the output burst write loop:

```cpp
// Initial row loading
for (uint32_t row = 0; row < initRows; ++row) {
#pragma HLS LOOP_FLATTEN off
    for (uint32_t ch = 0; ch < numChannels; ++ch) {
#pragma HLS LOOP_FLATTEN off
        for (uint32_t px = 0; px < inputWidth; ++px) {
#pragma HLS PIPELINE II=1
            rowBuffer[row][dstBase + px] = *(input + srcBase + px);

// Prefetch during y-loop
for (uint32_t ch = 0; ch < numChannels; ++ch) {
#pragma HLS LOOP_FLATTEN off
    for (uint32_t px = 0; px < inputWidth; ++px) {
#pragma HLS PIPELINE II=1
        rowBuffer[bufIdx][dstBase + px] = *(input + srcBase + px);

// Output burst write
for (uint32_t p = 0; p < nActive; ++p) {
#pragma HLS LOOP_FLATTEN off
    for (uint32_t x = 0; x < outWidth; ++x) {
#pragma HLS PIPELINE II=1
        *(output + dstBase + x) = outRowBuf[p][x];
```

When HLS flattens nested loops, it merges the loop control logic (counters, comparisons, address calculations for both loops) into a single pipeline. This adds more combinational logic between pipeline registers, potentially lengthening the critical path beyond the clock period. By keeping `LOOP_FLATTEN off`, the outer loop's overhead (incrementing `ch` or `p`, restarting the inner pipeline) costs a few extra clock cycles but keeps the combinational paths short enough to meet timing.

The filter loading was kept interleaved (without `LOOP_FLATTEN off`) since its structure — outer sequential `i`, inner unrolled `p` — doesn't create the same critical path issue.


## MaxPool HW Accelerator

### Motivation

After all Conv2D optimizations, the bottleneck breakdown was: Conv 43.5%, MaxPool 43.1%, Dense 20%. The ARM CPU spent 0.481s on software MaxPool while the FPGA sat idle. MaxPool is a simple 2×2 max comparison with stride 2 — no multiplications needed (zero DSPs), making it ideal for a lightweight hardware accelerator.

### What MaxPool does

MaxPool takes a 2×2 block of pixels and outputs the maximum value. With stride 2, non-overlapping 2×2 blocks tile the input, halving both width and height:

```
Input (4×4):          Output (2×2):
[3  1  4  1]          [3  4]
[5  9  2  6]    →     [9  8]
[5  3  5  8]
[9  7  9  3]
```

Each output pixel = max(top-left, top-right, bottom-left, bottom-right) of the corresponding 2×2 block.

### HLS design

```cpp
void MaxPool_HW(TFXP_MP *input, TFXP_MP *output,
    uint32_t channels, uint32_t width, uint32_t height)
```

**Interface pragmas** follow the same pattern as Conv2D: separate AXI master ports for input (`gmem0`) and output (`gmem1`) to avoid read/write contention, s_axilite for all parameters and return.

**Row-pair buffering**: Two consecutive input rows are burst-read into a single local buffer (`rowBuf[512]`). Since the data is CHW layout, consecutive rows within a channel are contiguous in memory, enabling a single burst of `2 × width` elements:

```cpp
TFXP_MP rowBuf[512];    // 2 rows × max 254 width
for (uint32_t i = 0; i < 2 * width; ++i) {
#pragma HLS PIPELINE II=1
    rowBuf[i] = *(input + srcBase + i);
}
```

Row 0 occupies `rowBuf[0..width-1]`, row 1 occupies `rowBuf[width..2*width-1]`.

**2×2 max computation** at II=1:

```cpp
for (uint32_t col = 0; col < outW2; ++col) {
#pragma HLS PIPELINE II=1
    uint32_t c2 = col * 2;
    TFXP_MP a = rowBuf[c2];           // top-left
    TFXP_MP b = rowBuf[c2 + 1];       // top-right
    TFXP_MP c = rowBuf[width + c2];   // bottom-left
    TFXP_MP d = rowBuf[width + c2 + 1]; // bottom-right

    TFXP_MP max01 = (a > b) ? a : b;
    TFXP_MP max23 = (c > d) ? c : d;
    TFXP_MP maxVal = (max01 > max23) ? max01 : max23;
    outBuf[col] = maxVal;
}
```

This uses a comparison tree: two parallel comparisons followed by one final comparison. Pure combinational logic — no DSPs consumed.

**Output row buffering**: Results are accumulated in `outBuf[128]` and then burst-written to DDR, same principle as the Conv2D output buffering.

**Odd dimension handling**: `outWidth` and `outHeight` are rounded down to even, matching the software MaxPool behavior.

### Vivado integration

The MaxPool accelerator required two additional HP ports:
- **HP2**: MaxPool `gmem0` (input reads) via `axi_mem_intercon_2`
- **HP3**: MaxPool `gmem1` (output writes) via `axi_mem_intercon_3`

The AXI peripheral bus was expanded to 2 master ports — M00 for Conv2D control registers (at `0x40000000`), M01 for MaxPool control registers (at `0x40010000`).

### Software integration

A `CMaxPoolProxy` class follows the same pattern as the Conv2D proxy:
- Opens the accelerator at `0x40010000`
- Register offsets from HLS-generated header: control at 0x00, input_r at 0x10, output_r at 0x18, channels at 0x20, width at 0x28, height at 0x30
- Translates virtual→physical DDR addresses and writes them to the s_axilite registers
- Uses `ShareDMAMappings()` to reuse the Conv2D proxy's DMA address translation table without copying (avoids double-free on cleanup)

In `model.cpp`, all 5 software `MaxPool()` calls were replaced with `maxpooler.MaxPool_HW()`.

### Impact

| Layer | SW MaxPool (ms) | HW MaxPool (ms) | Speedup |
|-------|----------------|-----------------|---------|
| MaxPool 0 (32ch, 254×254) | 266 | 40.3 | 6.6× |
| MaxPool 1 (64ch, 125×125) | 126 | 21.4 | 5.9× |
| MaxPool 2 (128ch, 60×60) | 59 | 12.1 | 4.9× |
| MaxPool 3 (256ch, 28×28) | 26 | 7.3 | 3.6× |
| MaxPool 4 (64ch, 12×12) | 1.2 | 0.6 | 2.0× |
| **Total MaxPool** | **478** | **82** | **5.8×** |
| **Total inference** | **1116** | **744** | **1.50×** |

A further optimization merged the two separate row reads into a single contiguous burst (since rows are adjacent in CHW layout), saving burst setup overhead. This brought total MaxPool down to 77ms and total inference to 739ms.

### Final bottleneck breakdown

- **Conv (HW): 69.4%** (0.512s)
- **Dense (SW): 20.2%** (0.149s)
- **MaxPool (HW): 10.4%** (0.077s)
- Flatten + Sigmoid: ~0.1%

### Overall performance summary

| Milestone | Total time | Speedup vs SW |
|-----------|-----------|---------------|
| Software only | 28.0s | 1× |
| Initial HW (no optimization) | 86.4s | 0.3× (slower!) |
| + Filter caching + dual AXI | 14.1s | 2.0× |
| + Input row caching | 12.9s | 2.2× |
| + N=4 filter parallelism | 4.9s | 5.7× |
| + N=8 filter parallelism | 3.4s | 8.2× |
| + 4-buffer prefetch | 3.2s | 8.8× |
| + N=16 + output row buffering | 1.4s | 20.0× |
| + X-position parallelism | 1.1s | 25.5× |
| + MaxPool HW accelerator | 0.74s | **37.9×** |
# Convolution_Accelerator
Midterm project for the Class HW-SW Codesign on using an PYNQ FPGA board to accelerate a convolution algorithm using HLS.

## Execution Time and Resource Usage

| Description       | Width | Height | Channels | Filters | Time (ms) | LUTs       | FFs        | BRAMs | DSPs     |
|-------------------|-------|--------|----------|---------|-----------|------------|------------|-------|----------|
| SW                | 256   | 256    | 16       | 32      | 28000     | n/a        | n/a        | n/a   | n/a      |
| Initial solution  | 256   | 256    | 16       | 32      | 86357     | 5316 (9%)  | 5838 (5%)  | -     | 39 (17%) |
| + Dual AXI + Interface tuning + Filter caching | 256 | 256 | 16 | 32 | 14124 | 9180 (17%) | 5977 (5%) | 8 (2%) | 30 (13%) |
| + Input row caching | 256 | 256 | 16 | 32 | 12870 | 10369 (19%) | 6494 (6%) | 40 (14%) | 15 (6%) |
| + memcpy burst transfers (reverted) | 256 | 256 | 16 | 32 | 12856 | 13167 (24%) | 10744 (10%) | 48 (17%) | 45 (20%) |
| + 4x filter parallelism | 256 | 256 | 16 | 32 | 4853 | 15986 (30%) | 12978 (12%) | 64 (22%) | 66 (30%) |
| + 8x filter parallelism | 256 | 256 | 16 | 32 | 3374 | 19224 (36%) | 15686 (14%) | 96 (34%) | 88 (40%) |
| + 4-buffer prefetch overlap | 256 | 256 | 16 | 32 | 3190 | 17117 (32%) | 12607 (11%) | 96 (34%) | 88 (40%) |
| + Separate output HP port (reverted) | 256 | 256 | 16 | 32 | 3190 | 18435 (34%) | 13339 (12%) | 96 (34%) | 88 (40%) |
| + 16x filter parallelism | 256 | 256 | 16 | 32 | 2509 | 22196 (41%) | 18290 (17%) | 160 (57%) | 137 (62%) |
| + Output row buffering + remove latency hint | 256 | 256 | 16 | 32 | 1402 | 17099 (32%) | 13367 (12%) | 176 (62%) | 91 (41%) |
| + Loop flattening + interleaved filter load | 256 | 256 | 16 | 32 | 1397 | 16976 (31%) | 20575 (19%) | 176 (62%) | 92 (41%) |
| + 2× x-position parallelism | 256 | 256 | 16 | 32 | 1116 | 24920 (46%) | 20913 (19%) | 192 (68%) | 140 (63%) |
| + MaxPool HW accelerator | 256 | 256 | 16 | 32 | 744 | — | — | — | — |
| + MaxPool merged burst reads | 256 | 256 | 16 | 32 | 739 | — | — | — | — |

## 1. Basic Accelerator Design and Integration

The hardware accelerator (`Conv2D_HW`) implements the full convolution computation **including bias addition and ReLU activation** directly in hardware. This can be seen in [conv2d.cpp](HLS/HLS_Conv/conv2d.cpp):

- **Convolution**: The 6-nested-loop structure computes the 2D convolution across all filters and channels using fixed-point multiply-accumulate operations (`FXP_Mult`)
- **Bias**: After the convolution accumulation for each output pixel, the bias for the current filter is added (`acc += bias`, line 59). The bias is read once per filter from DDR (`biases[iFilter]`, line 42) and reused across all spatial positions
- **ReLU**: Applied conditionally based on the `relu` parameter (`acc = (acc < 0) ? 0 : acc`, lines 61-63). This allows the same accelerator to be used for layers that need ReLU (conv layers, `relu=1`) and those that don't

The SW side ([model.cpp](Solution/src/model.cpp)) calls `convolver.Conv2D_HW(...)` with `relu=1` for all 5 convolution layers. The operations that remain in software are:
- **MaxPool**: Runs on the ARM CPU after each conv layer
- **Flatten**: Transposes the feature maps for the dense layers
- **Dense layers**: Fully-connected layers run on the ARM CPU
- **Sigmoid**: Final activation for the binary classifier

This partitioning makes sense because the convolution is by far the most compute-intensive operation (O(filters x channels x height x width x 3 x 3)), while the other operations are either simpler or have different access patterns less suited to the accelerator.

### Bug Fix: Swapped numChannels/numFilters in model.cpp

The original Lab 3 code had a bug in [model.cpp](Solution/src/model.cpp) where `numChannels` and `numFilters` were swapped in every `Conv2D_HW` call. The HLS function signature is:

```cpp
Conv2D_HW(..., uint32_t numChannels, uint32_t numFilters, ...)
```

But the Inference function passed them as:
```cpp
convolver.Conv2D_HW(..., LayerShapes[iLayer][1], LayerShapes[iLayer][0], ...)
//                       ↑ output_filters (32)   ↑ input_channels (3)
```

This sent `numChannels=32` and `numFilters=3` for layer 0, causing the HW to only compute 3 output feature maps (instead of 32) while reading 32 input channels (only 3 exist). The out-of-bounds input reads caused a **read transaction overflow** visible in the System ILA on `master1`, and the network produced incorrect classifications.

The fix swaps the arguments to `LayerShapes[iLayer][0], LayerShapes[iLayer][1]` so that `numChannels` correctly receives the number of input channels and `numFilters` receives the number of output filters.

---

Current Synthesis Report:

========================================
== HLS Synthesis Report
========================================


================================================================
== Synthesis Summary Report of 'Conv2D_HW'
================================================================
+ General Information: 
    * Date:           Sun Mar 22 10:48:42 2026
    * Version:        2022.2 (Build 3670227 on Oct 13 2022)
    * Project:        Conv2D_HW_HLS
    * Solution:       solution1 (Vivado IP Flow Target)
    * Product family: zynq
    * Target device:  xc7z020-clg400-1
    

+ Performance & Resource Estimates: 
    
    PS: '+' for module; 'o' for loop; '*' for dataflow
    +-------------------------------------------------------------------------+------+------+---------+--------+----------+---------+------+----------+------+----------+-----------+-----------+-----+
    |                                 Modules                                 | Issue|      | Latency | Latency| Iteration|         | Trip |          |      |          |           |           |     |
    |                                 & Loops                                 | Type | Slack| (cycles)|  (ns)  |  Latency | Interval| Count| Pipelined| BRAM |    DSP   |     FF    |    LUT    | URAM|
    +-------------------------------------------------------------------------+------+------+---------+--------+----------+---------+------+----------+------+----------+-----------+-----------+-----+
    |+ Conv2D_HW                                                              |     -|  0.00|        -|       -|         -|        -|     -|        no|     -|  39 (17%)|  5838 (5%)|  5316 (9%)|    -|
    | o VITIS_LOOP_41_1_VITIS_LOOP_43_2                                       |     -|  7.30|        -|       -|         -|        -|     -|        no|     -|         -|          -|          -|    -|
    |  o VITIS_LOOP_44_3                                                      |     -|  7.30|        -|       -|         -|        -|     -|        no|     -|         -|          -|          -|    -|
    |   + Conv2D_HW_Pipeline_VITIS_LOOP_47_4_VITIS_LOOP_48_5_VITIS_LOOP_49_6  |     -|  0.00|        -|       -|         -|        -|     -|        no|     -|   18 (8%)|  1972 (1%)|  1733 (3%)|    -|
    |    o VITIS_LOOP_47_4_VITIS_LOOP_48_5_VITIS_LOOP_49_6                    |    II|  7.30|        -|       -|        19|        2|     -|       yes|     -|         -|          -|          -|    -|
    +-------------------------------------------------------------------------+------+------+---------+--------+----------+---------+------+----------+------+----------+-----------+-----------+-----+


================================================================
== HW Interfaces
================================================================
* M_AXI
+---------------+------------+---------------+---------+--------+----------+-----------+--------------+--------------+-------------+-------------+
| Interface     | Data Width | Address Width | Latency | Offset | Register | Max Widen | Max Read     | Max Write    | Num Read    | Num Write   |
|               | (SW->HW)   |               |         |        |          | Bitwidth  | Burst Length | Burst Length | Outstanding | Outstanding |
+---------------+------------+---------------+---------+--------+----------+-----------+--------------+--------------+-------------+-------------+
| m_axi_master1 | 32 -> 32   | 32            | 0       | slave  | 0        | 0         | 16           | 16           | 16          | 16          |
+---------------+------------+---------------+---------+--------+----------+-----------+--------------+--------------+-------------+-------------+

* S_AXILITE Interfaces
+---------------+------------+---------------+--------+----------+
| Interface     | Data Width | Address Width | Offset | Register |
+---------------+------------+---------------+--------+----------+
| s_axi_control | 32         | 7             | 16     | 0        |
+---------------+------------+---------------+--------+----------+

* S_AXILITE Registers
+---------------+-------------+--------+-------+--------+----------------------------------+----------------------------------------------------------------------+
| Interface     | Register    | Offset | Width | Access | Description                      | Bit Fields                                                           |
+---------------+-------------+--------+-------+--------+----------------------------------+----------------------------------------------------------------------+
| s_axi_control | CTRL        | 0x00   | 32    | RW     | Control signals                  | 0=AP_START 1=AP_DONE 2=AP_IDLE 3=AP_READY 7=AUTO_RESTART 9=INTERRUPT |
| s_axi_control | GIER        | 0x04   | 32    | RW     | Global Interrupt Enable Register | 0=Enable                                                             |
| s_axi_control | IP_IER      | 0x08   | 32    | RW     | IP Interrupt Enable Register     | 0=CHAN0_INT_EN 1=CHAN1_INT_EN                                        |
| s_axi_control | IP_ISR      | 0x0c   | 32    | RW     | IP Interrupt Status Register     | 0=CHAN0_INT_ST 1=CHAN1_INT_ST                                        |
| s_axi_control | input_r     | 0x10   | 32    | W      | Data signal of input_r           |                                                                      |
| s_axi_control | output_r    | 0x18   | 32    | W      | Data signal of output_r          |                                                                      |
| s_axi_control | filters     | 0x20   | 32    | W      | Data signal of filters           |                                                                      |
| s_axi_control | biases      | 0x28   | 32    | W      | Data signal of biases            |                                                                      |
| s_axi_control | numChannels | 0x30   | 32    | W      | Data signal of numChannels       |                                                                      |
| s_axi_control | numFilters  | 0x38   | 32    | W      | Data signal of numFilters        |                                                                      |
| s_axi_control | inputWidth  | 0x40   | 32    | W      | Data signal of inputWidth        |                                                                      |
| s_axi_control | inputHeight | 0x48   | 32    | W      | Data signal of inputHeight       |                                                                      |
| s_axi_control | convWidth   | 0x50   | 32    | W      | Data signal of convWidth         |                                                                      |
| s_axi_control | convHeight  | 0x58   | 32    | W      | Data signal of convHeight        |                                                                      |
| s_axi_control | relu        | 0x60   | 32    | W      | Data signal of relu              |                                                                      |
+---------------+-------------+--------+-------+--------+----------------------------------+----------------------------------------------------------------------+

* TOP LEVEL CONTROL
+-----------+------------+-----------+
| Interface | Type       | Ports     |
+-----------+------------+-----------+
| ap_clk    | clock      | ap_clk    |
| ap_rst_n  | reset      | ap_rst_n  |
| interrupt | interrupt  | interrupt |
| ap_ctrl   | ap_ctrl_hs |           |
+-----------+------------+-----------+

================================================================
== M_AXI Burst Information
================================================================
 Note: All burst requests might be further partitioned into multiple requests during RTL generation based on max_read_burst_length or max_write_burst_length settings.

* Inferred Burst Summary
+---------------+-----------------+-----------+----------+-------+-------------------------------+
| HW Interface  | Loop            | Direction | Length   | Width | Location                      |
+---------------+-----------------+-----------+----------+-------+-------------------------------+
| m_axi_master1 | VITIS_LOOP_44_3 | write     | variable | 32    | HLS/HLS_Conv/conv2d.cpp:44:24 |
+---------------+-----------------+-----------+----------+-------+-------------------------------+

* Inferred Bursts and Widening Missed
+---------------+----------+-----------------+-------------------------------------------------------------------------------------------------------+------------+-------------------------------+
| HW Interface  | Variable | Loop            | Problem                                                                                               | Resolution | Location                      |
+---------------+----------+-----------------+-------------------------------------------------------------------------------------------------------+------------+-------------------------------+
| m_axi_master1 | filters  | VITIS_LOOP_48_5 | Stride is incompatible                                                                                | 214-230    | HLS/HLS_Conv/conv2d.cpp:48:28 |
| m_axi_master1 | input    | VITIS_LOOP_48_5 | Stride is incompatible                                                                                | 214-230    | HLS/HLS_Conv/conv2d.cpp:48:28 |
| m_axi_master1 | output   | VITIS_LOOP_43_2 | Stride is incompatible                                                                                | 214-230    | HLS/HLS_Conv/conv2d.cpp:43:22 |
| m_axi_master1 | output   | VITIS_LOOP_44_3 | Could not widen since type i32 size is greater than or equal to the max_widen_bitwidth threshold of 0 | 214-353    | HLS/HLS_Conv/conv2d.cpp:44:24 |
| m_axi_master1 | filters  | VITIS_LOOP_49_6 | Could not widen since type i32 size is greater than or equal to the max_widen_bitwidth threshold of 0 | 214-353    | HLS/HLS_Conv/conv2d.cpp:49:30 |
| m_axi_master1 | input    | VITIS_LOOP_49_6 | Could not widen since type i32 size is greater than or equal to the max_widen_bitwidth threshold of 0 | 214-353    | HLS/HLS_Conv/conv2d.cpp:49:30 |
| m_axi_master1 | biases   | VITIS_LOOP_41_1 | Could not widen since type i32 size is greater than or equal to the max_widen_bitwidth threshold of 0 | 214-353    | HLS/HLS_Conv/conv2d.cpp:41:19 |
| m_axi_master1 |          |                 | Could not burst due to multiple potential reads to the same bundle in the same region.                | 214-224    | HLS/HLS_Conv/conv2d.cpp:41:19 |
| m_axi_master1 |          |                 | Could not burst due to multiple potential reads to the same bundle in the same region.                | 214-224    | HLS/HLS_Conv/conv2d.cpp:49:30 |
+---------------+----------+-----------------+-------------------------------------------------------------------------------------------------------+------------+-------------------------------+


Type II issue means the pipeline is only able to accept an input every 2nd clock cycle which is becaue all four arrays (Input/Output/filters/biases) share a single AXI port (master1). (reading input and filters) in the same cycle.

Solution: Split into separate AXI bundles (master1/master2) so reads can happen in parallel.

## Changes for Optimization 1: Dual AXI Master Ports

### conv2d.cpp (HLS pragmas)
Split `filters` and `biases` to a separate AXI bundle `master2`, keeping `input` and `output` on `master1`:
```
- #pragma HLS INTERFACE m_axi port=filters bundle=master1 offset=slave
- #pragma HLS INTERFACE m_axi port=biases bundle=master1 offset=slave
+ #pragma HLS INTERFACE m_axi port=filters bundle=master2 offset=slave
+ #pragma HLS INTERFACE m_axi port=biases bundle=master2 offset=slave
```

### Conv2D_HW_Vivado.tcl (Vivado block design)
1. Enabled HP1 port on the PS:
```
- CONFIG.PCW_USE_S_AXI_HP1 {0}
+ CONFIG.PCW_USE_S_AXI_HP1 {1}
```

2. Added a second AXI interconnect (`axi_mem_intercon_1`) for the new master port.

3. Wired `master2` through the new interconnect to HP1:
```
Conv2D_HW_0/m_axi_master2 --> axi_mem_intercon_1/S00_AXI
axi_mem_intercon_1/M00_AXI --> processing_system7_0/S_AXI_HP1
```

4. Added clock and reset connections for `axi_mem_intercon_1` and `S_AXI_HP1_ACLK`.

5. Added address mapping for `master2` through HP1:
```
Conv2D_HW_0/Data_m_axi_master2 --> S_AXI_HP1/HP1_DDR_LOWOCM (0x00000000, 512MB)
```

## Changes for Optimization 2: AXI Interface Tuning

### conv2d.cpp (HLS pragmas)
Added `depth`, `max_read/write_burst_length`, `num_read/write_outstanding`, and `latency` to all m_axi pragmas:

- **`depth`**: Tells HLS the max number of elements accessed (based on largest layer — layer 0: 256x256 input, 32 filters, 3 channels)
  - `input`: 256x256x3 = 196,608
  - `output`: 254x254x32 = 2,064,512
  - `filters`: 32x3x3x3 = 864 (set to 27,648 to cover layer 2: 128x64x3x3)
  - `biases`: 256 (max filters across all layers)
- **`max_read/write_burst_length=256`**: Max AXI4 burst length (up from default 16), moves more data per transaction
- **`num_read/write_outstanding=16`**: Allows 16 burst requests in flight simultaneously, enabling memory request pipelining
- **`latency=20`**: Hints DDR access latency (~200ns at 100MHz = 20 cycles), helps HLS schedule around memory stalls

## 2. Caching of Filter Coefficients

### Problem
In the initial implementation, filter values are read from DDR (via AXI) on every multiply-accumulate in the innermost loop. For each output pixel, the same filter is re-read from DDR — this means `(inputHeight-2) * (inputWidth-2)` redundant reads of the entire filter per output filter. DDR reads have high latency and limited bandwidth, making this the main bottleneck.

### Solution
Before the spatial loops (`y`, `x`), copy the filter coefficients for the current filter into a local BRAM array (`localFilters`). HLS synthesizes local arrays as BRAM, which has single-cycle access and no AXI overhead.

```cpp
TFXP localFilters[256 * 3 * 3]; // Max: 256 channels x 3x3 kernel
for (uint32_t i = 0; i < numChannels * convHeight * convWidth; ++ i) {
    localFilters[i] = *(filters + iFilter*numChannels*convHeight*convWidth + i);
}
```

The copy loop accesses DDR sequentially, which is burst-friendly — HLS can infer a single burst transfer to fill the local buffer. The inner convolution loop then reads from `localFilters` (BRAM) instead of `filters` (DDR).

### Impact
- **BRAM usage**: The local array is 256 x 3 x 3 = 2,304 x 32-bit = 9,216 bytes = ~7 BRAM18K blocks (the xc7z020 has 280)
- **DDR reads for filters**: Reduced from `numFilters * outputHeight * outputWidth * numChannels * 9` to `numFilters * numChannels * 9` (one bulk read per filter instead of per output pixel)
- The sequential copy enables burst inference, so the `max_read_burst_length=256` pragma from Optimization 2 now takes effect for filter reads

### Synthesis Results After Optimizations 1+2+3 (Dual AXI + Interface Tuning + Filter Caching)

Key improvements vs initial solution:
- **II=1 achieved** on both the filter copy loop (`VITIS_LOOP_46_2`) and the inner compute loop (`VITIS_LOOP_55_5_VITIS_LOOP_56_6_VITIS_LOOP_57_7`) — no more II violations
- **3 burst transfers inferred** (up from 1):
  - Filter read burst on `master2` (the copy loop)
  - Input read burst on `master1` (inner compute loop)
  - Output write burst on `master1`
- **BRAM**: 0 → 8 (2%) — used for the local filter cache
- **DSPs**: 39 (17%) → 30 (13%) — HLS shares multipliers more efficiently
- **LUTs**: 5316 (9%) → 9180 (17%) — increased due to dual AXI adapter logic
- **FFs**: 5838 (5%) → 5977 (5%) — roughly unchanged

Remaining issues:
- `input` stride incompatible across channels — would need input tile caching
- `output` stride incompatible across y rows — would need output buffering
- Widening still blocked (`max_widen_bitwidth=0`)


Runtime HW minimal implementation:

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...





[HW] OUTPUT: 1.00000000 --> DOG
Conv 0 (HW) --> 6451485652 ns (6.451 s)
Conv 1 (HW) --> 28595199274 ns (28.595 s)
Conv 2 (HW) --> 26019028789 ns (26.019 s)
Conv 3 (HW) --> 22481711192 ns (22.482 s)
Conv 4 (HW) --> 2061981388 ns (2.062 s)
MaxPool 0 --> 266283501 ns (0.266 s)
MaxPool 1 --> 125602621 ns (0.126 s)
MaxPool 2 --> 58888221 ns (0.059 s)
MaxPool 3 --> 25613461 ns (0.026 s)
MaxPool 4 --> 1187723 ns (0.001 s)
Dense 5 (SW) --> 269742129 ns (0.270 s)
Dense 6 (SW) --> 114846 ns (0.000 s)
Total Conv time (HW): 85609406295 ns (85.609 s) 99.1 %
Total MaxPool time:   477575527 ns (0.478 s) 0.6 %
Total Dense time (SW):269856975 ns (0.270 s) 0.3 %
Total Flatten time:   433710 ns (0.000 s) 0.0 %
Total Sigmoid time:   167345 ns (0.000 s) 0.0 %
Total time:           86357439852 ns (86.357 s) 100.0 %


Runtime HW with filter caching and improved AXI port interfacing:

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...
[HW] OUTPUT: 0.93905449 --> DOG
Conv 0 (HW) --> 1621222367 ns (1.621 s)
Conv 1 (HW) --> 4432703446 ns (4.433 s)
Conv 2 (HW) --> 3864081509 ns (3.864 s)
Conv 3 (HW) --> 3265174196 ns (3.265 s)
Conv 4 (HW) --> 283474200 ns (0.283 s)
MaxPool 0 --> 267879206 ns (0.268 s)
MaxPool 1 --> 127048018 ns (0.127 s)
MaxPool 2 --> 81948759 ns (0.082 s)
MaxPool 3 --> 25598735 ns (0.026 s)
MaxPool 4 --> 1203087 ns (0.001 s)
Dense 5 (SW) --> 148917806 ns (0.149 s)
Dense 6 (SW) --> 66123 ns (0.000 s)
Total Conv time (HW): 13466655718 ns (13.467 s) 95.3 %
Total MaxPool time:   503677805 ns (0.504 s) 3.6 %
Total Dense time (SW):148983929 ns (0.149 s) 1.1 %
Total Flatten time:   453419 ns (0.000 s) 0.0 %
Total Sigmoid time:   4170674 ns (0.004 s) 0.0 %
Total time:           14123941545 ns (14.124 s) 100.0 %


## 3. Input Row Caching

### Problem
In the previous implementation, input pixels are read directly from DDR for every multiply-accumulate operation. With a 3x3 convolution kernel, each input row is read 3 times (once for each kernel row) across consecutive output rows. This results in `3 * numChannels * inputWidth` redundant DDR reads per output row.

### Solution
Cache 3 rows of input data (across all channels) into local BRAM using a circular buffer scheme. Since a 3x3 convolution only needs 3 consecutive rows at any time, we maintain `rowBuffer[3][4096]` — 3 row slots, each holding up to `numChannels * inputWidth` pixels (max 32 * 128 = 4096 for the model's layers).

```cpp
TFXP rowBuffer[3][4096];

// Load initial 3 rows into BRAM
for (uint32_t row = 0; row < 3; ++ row) {
    for (uint32_t ch = 0; ch < numChannels; ++ ch) {
        uint32_t srcBase = ch * inputWidth * inputHeight + row * inputWidth;
        uint32_t dstBase = ch * inputWidth;
        for (uint32_t px = 0; px < inputWidth; ++ px)
            rowBuffer[row][dstBase + px] = *(input + srcBase + px);
    }
}

// Sliding window: for each new output row y, replace the expired row with row y+2
for (uint32_t y = 0; y < (inputHeight-2); ++y) {
    if (y > 0) {
        uint32_t newRow = y + 2;
        uint32_t bufIdx = newRow % 3;  // circular index
        // Load new row into the slot that held the expired row
        ...
    }
    // Convolution reads from rowBuffer[(y+cy) % 3] instead of DDR
}
```

The circular indexing `(y + cy) % 3` maps each of the 3 kernel rows to the correct buffer slot. When moving to the next output row, only 1 new row is loaded from DDR (replacing the row that's no longer needed), instead of re-reading all 3 rows.

### Impact
- **DDR input reads**: Reduced from `3 * numChannels * inputWidth * outputHeight` per filter to `(outputHeight + 2) * numChannels * inputWidth` — each row is read exactly once from DDR
- **BRAM usage**: 8 (2%) → 40 (14%) — 32 additional BRAM18K for the 3 row buffers (3 x 4096 x 32-bit = 48KB)
- **DSPs**: 30 (13%) → 15 (6%) — reduced because filter reads now come from local BRAM, allowing HLS to simplify address computation
- **Runtime**: 14.1s → 12.9s (~9% improvement) — modest gain because the DDR access pattern (non-sequential across channels) still limits burst efficiency

Runtime HW with input row caching:

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...
[HW] OUTPUT: 0.93905449 --> DOG
Conv 0 (HW) --> 1603014382 ns (1.603 s)
Conv 1 (HW) --> 3785198973 ns (3.785 s)
Conv 2 (HW) --> 3389254887 ns (3.389 s)
Conv 3 (HW) --> 3101344060 ns (3.101 s)
Conv 4 (HW) --> 337042289 ns (0.337 s)
MaxPool 0 --> 292605120 ns (0.293 s)
MaxPool 1 --> 125775966 ns (0.126 s)
MaxPool 2 --> 58864283 ns (0.059 s)
MaxPool 3 --> 25611006 ns (0.026 s)
MaxPool 4 --> 1187483 ns (0.001 s)
Dense 5 (SW) --> 148974616 ns (0.149 s)
Dense 6 (SW) --> 67631 ns (0.000 s)
Total Conv time (HW): 12215854591 ns (12.216 s) 94.9 %
Total MaxPool time:   504043858 ns (0.504 s) 3.9 %
Total Dense time (SW):149042247 ns (0.149 s) 1.2 %
Total Flatten time:   452444 ns (0.000 s) 0.0 %
Total Sigmoid time:   116160 ns (0.000 s) 0.0 %
Total time:           12869509300 ns (12.870 s) 100.0 %


## 4. Burst Transfer Optimization via memcpy (Attempted — Reverted)

### Motivation
HLS reported "Stride is incompatible" and "Could not widen" warnings on all DDR data transfers, indicating that manual pointer-arithmetic loops were being compiled as single-element AXI transactions rather than bursts. The idea was to replace these loops with `memcpy`, which HLS has built-in recognition for and directly maps to AXI burst transactions.

### Changes Attempted
- **Filter loading**: replaced manual copy loop with `memcpy(localFilters, filters + offset, size)`
- **Row loading**: replaced inner pixel loop with `memcpy(rowBuffer[row] + dst, input + src, inputWidth * sizeof(TFXP))`
- **Output writing**: buffered output row in BRAM (`outputRow[x] = acc`), then burst-wrote with `memcpy(output + offset, outputRow, outputWidth * sizeof(TFXP))`

### Results
The `memcpy` approach resolved the "Could not widen" warnings but the "Stride is incompatible" warnings persisted on the outer loops (unavoidable with CHW data layout — channels are non-contiguous in memory).

More importantly, **runtime was unchanged**: 12.870s → 12.856s. The bottleneck is the convolution compute loop (multiply-accumulate across channels/kernel), not the data transfer overhead. The transfers (row loading, filter loading, output writing) represent a negligible fraction of the total execution time.

### Resource Cost
The burst logic came at significant resource cost for zero performance benefit:
- **BRAM**: 40 (14%) → 48 (17%)
- **DSPs**: 15 (6%) → 45 (20%)
- **LUTs**: 10369 (19%) → 13167 (24%)
- **FFs**: 6494 (6%) → 10744 (10%)

### Decision
Reverted to the manual loop version to preserve resources for potential compute-side optimizations (loop unrolling, pipelining) that would target the actual bottleneck.


## 5. Parallelizing Output Filter Computation (N_PARALLEL=4)

### Problem
After the memcpy experiment proved the design is compute-bound, the bottleneck is clear: the convolution MAC loop iterates over all `numChannels * 3 * 3` multiply-accumulates for every output pixel, and this is repeated for every output filter sequentially. For layer 1 (64 filters, 32 channels), that's 64 full spatial passes over the input.

### Solution
Compute multiple output filters simultaneously by replicating the filter storage, multipliers, and accumulators. All parallel filters share the same input pixel (read once from `rowBuffer`), so input caching is unchanged.

Key changes:
- **`localFilters[4][256*9]`** — 4 separate BRAM arrays, one per parallel filter, fully partitioned on the first dimension so all 4 can be read in the same cycle:
```cpp
TFXP localFilters[4][256 * 3 * 3];
#pragma HLS ARRAY_PARTITION variable=localFilters complete dim=1
```

- **Outer loop** steps by `N_PARALLEL=4`:
```cpp
for (uint32_t iFilter = 0; iFilter < numFilters; iFilter += N_PARALLEL) {
```

- **Inner MAC** reads one pixel, multiplies against all 4 filters in parallel:
```cpp
TFXP pixelValue = rowBuffer[(y + cy) % 3][iChannel * inputWidth + x + cx];
for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
    TFXP filterValue = localFilters[p][iChannel*convHeight*convWidth + cy*convWidth + cx];
    acc[p] += FXP_Mult(filterValue, pixelValue, DECIMALS);
}
```

- **4 independent accumulators** with separate bias addition, ReLU, and output writes, all unrolled
- **Edge case handling**: `nActive` tracks valid filters when `numFilters % 4 != 0`; bounds-checked before output writes

### Impact with N_PARALLEL=4
- **Conv time**: 12.2s → 4.2s (**2.9x speedup**, close to theoretical 4x)
- **Total time**: 12.9s → 4.9s (**2.6x overall speedup**)
- **BRAM**: 40 (14%) → 64 (22%) — 4 separate filter BRAMs
- **DSPs**: 15 (6%) → 66 (30%) — 4x multipliers for parallel MACs
- **LUTs**: 10369 (19%) → 15986 (30%) — parallel control logic
- **FFs**: 6494 (6%) → 12978 (12%) — parallel pipeline registers

### Scaling to N_PARALLEL=8
Doubling the parallelism factor requires only changing the constant and array sizes. Resource usage scales roughly linearly:
- **BRAM**: 64 (22%) → 96 (34%)
- **DSPs**: 66 (30%) → 88 (40%)
- **LUTs**: 15986 (30%) → 19224 (36%)
- **FFs**: 12978 (12%) → 15686 (14%)

Conv time: 4.2s → 2.7s (**1.55x** over N=4). Total: 4.9s → 3.4s.

### Per-layer breakdown

| Layer | Channels | Filters | N=1 (s) | N=4 (s) | N=8 (s) | Speedup (N=1→8) |
|-------|----------|---------|---------|---------|---------|-----------------|
| Conv 0 | 3 | 32 | 1.603 | 1.046 | 0.853 | 1.9x |
| Conv 1 | 32 | 64 | 3.785 | 1.259 | 0.790 | 4.8x |
| Conv 2 | 64 | 128 | 3.389 | 0.991 | 0.570 | 5.9x |
| Conv 3 | 128 | 256 | 3.101 | 0.841 | 0.454 | 6.8x |
| Conv 4 | 256 | 64 | 0.337 | 0.088 | 0.051 | 6.6x |

Layers with more filters and channels see the greatest speedup because the filter loading overhead is amortized over more spatial computation. Conv 0 (only 3 input channels) benefits least — loading 8 filters of 27 coefficients each is significant relative to the small per-pixel computation (27 MACs per filter). Conv 3 (128 channels, 256 filters) approaches the theoretical 8x because the 1152-coefficient filter load is dwarfed by the spatial computation.

The N=4→N=8 speedup (1.55x) falls short of the ideal 2x partly due to filter loading taking 2x longer (8 sequential loads vs 4) and partly because Conv 0 is already hitting diminishing returns. N=16 was not attempted as DSP usage would likely exceed the xc7z020's 220 available DSPs.

Runtime HW with N_PARALLEL=4:

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...
[HW] OUTPUT: 0.93905449 --> DOG
Conv 0 (HW) --> 1046209380 ns (1.046 s)
Conv 1 (HW) --> 1259167571 ns (1.259 s)
Conv 2 (HW) --> 991472220 ns (0.991 s)
Conv 3 (HW) --> 840652122 ns (0.841 s)
Conv 4 (HW) --> 88236292 ns (0.088 s)
MaxPool 0 --> 266050701 ns (0.266 s)
MaxPool 1 --> 125661812 ns (0.126 s)
MaxPool 2 --> 58855582 ns (0.059 s)
MaxPool 3 --> 25663336 ns (0.026 s)
MaxPool 4 --> 1187606 ns (0.001 s)
Dense 5 (SW) --> 148925228 ns (0.149 s)
Dense 6 (SW) --> 66003 ns (0.000 s)
Total Conv time (HW): 4225737585 ns (4.226 s) 87.1 %
Total MaxPool time:   477419037 ns (0.477 s) 9.8 %
Total Dense time (SW):148991231 ns (0.149 s) 3.1 %
Total Flatten time:   450341 ns (0.000 s) 0.0 %
Total Sigmoid time:   174504 ns (0.000 s) 0.0 %
Total time:           4852772698 ns (4.853 s) 100.0 %

Runtime HW with N_PARALLEL=8:

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...
[HW] OUTPUT: 0.93905449 --> DOG
Conv 0 (HW) --> 853454534 ns (0.853 s)
Conv 1 (HW) --> 789558762 ns (0.790 s)
Conv 2 (HW) --> 569858501 ns (0.570 s)
Conv 3 (HW) --> 454089467 ns (0.454 s)
Conv 4 (HW) --> 50742431 ns (0.051 s)
MaxPool 0 --> 266542631 ns (0.267 s)
MaxPool 1 --> 125830071 ns (0.126 s)
MaxPool 2 --> 58853714 ns (0.059 s)
MaxPool 3 --> 35129185 ns (0.035 s)
MaxPool 4 --> 1287920 ns (0.001 s)
Dense 5 (SW) --> 168253960 ns (0.168 s)
Dense 6 (SW) --> 66250 ns (0.000 s)
Total Conv time (HW): 2717703695 ns (2.718 s) 80.5 %
Total MaxPool time:   487643521 ns (0.488 s) 14.5 %
Total Dense time (SW):168320210 ns (0.168 s) 5.0 %
Total Flatten time:   468923 ns (0.000 s) 0.0 %
Total Sigmoid time:   120446 ns (0.000 s) 0.0 %
Total time:           3374256795 ns (3.374 s) 100.0 %


## 6. Overlapping Data Transfers with Computation (4-Buffer Prefetch)

### Problem
With 3 row buffers and circular indexing `% 3`, the buffer slot for the next row to load (`(y+3) % 3 == y % 3`) is the same slot being read for the current computation. This forces the row load and computation to be strictly sequential — the accelerator must finish loading the new row before starting computation, adding idle time.

### Solution
Add a 4th row buffer so the prefetch target never conflicts with the 3 rows being read:

```cpp
// 3 buffers: (y+3) % 3 == y % 3 → CONFLICT (same slot)
// 4 buffers: (y+3) % 4 != y%4, (y+1)%4, (y+2)%4 → NO CONFLICT
TFXP rowBuffer[4][4096];
```

The initial load now fills 4 rows (0,1,2,3) instead of 3. During each y iteration, the prefetch of row `y+3` writes to buffer `(y+3) % 4` while the computation reads from `y%4`, `(y+1)%4`, `(y+2)%4` — all different slots.

An additional benefit: `% 4` is a simple 2-bit mask in hardware, whereas `% 3` requires division logic. This reduced the compute pipeline iteration latency from 43 cycles to 11 cycles (II=1 maintained in both cases).

### Impact
- **Conv time**: 2.718s → 2.562s (**5.7% improvement**)
- **Total time**: 3.374s → 3.190s (**5.5% improvement**)
- **BRAM**: 96 (34%) — unchanged (4th buffer reuses existing BRAM allocation)
- **DSPs**: 88 (40%) — unchanged
- **LUTs**: 19224 (36%) → 17117 (32%) — reduced due to simpler `% 4` modulo logic
- **FFs**: 15686 (14%) → 12607 (11%) — fewer pipeline stages needed
- **Pipeline iteration latency**: 43 → 11 cycles (due to `% 4` bit-mask vs `% 3` division)

### Per-layer breakdown

| Layer | Channels | Filters | 3-buf (s) | 4-buf (s) | Improvement |
|-------|----------|---------|-----------|-----------|-------------|
| Conv 0 | 3 | 32 | 0.853 | 0.770 | 9.7% |
| Conv 1 | 32 | 64 | 0.790 | 0.749 | 5.2% |
| Conv 2 | 64 | 128 | 0.570 | 0.551 | 3.3% |
| Conv 3 | 128 | 256 | 0.454 | 0.446 | 1.8% |
| Conv 4 | 256 | 64 | 0.051 | 0.046 | 9.8% |

Conv 0 and Conv 4 benefited most — layers where the row loading time is a larger fraction of the total computation.

Runtime HW with 4-buffer prefetch overlap:

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...
[HW] OUTPUT: 0.93905449 --> DOG
Conv 0 (HW) --> 770464709 ns (0.770 s)
Conv 1 (HW) --> 749196507 ns (0.749 s)
Conv 2 (HW) --> 550978526 ns (0.551 s)
Conv 3 (HW) --> 445885147 ns (0.446 s)
Conv 4 (HW) --> 45952661 ns (0.046 s)
MaxPool 0 --> 266513821 ns (0.267 s)
MaxPool 1 --> 125884627 ns (0.126 s)
MaxPool 2 --> 58840988 ns (0.059 s)
MaxPool 3 --> 25607311 ns (0.026 s)
MaxPool 4 --> 1187262 ns (0.001 s)
Dense 5 (SW) --> 148850061 ns (0.149 s)
Dense 6 (SW) --> 67948 ns (0.000 s)
Total Conv time (HW): 2562477550 ns (2.562 s) 80.3 %
Total MaxPool time:   478034009 ns (0.478 s) 15.0 %
Total Dense time (SW):148918009 ns (0.149 s) 4.7 %
Total Flatten time:   470428 ns (0.000 s) 0.0 %
Total Sigmoid time:   164052 ns (0.000 s) 0.0 %
Total time:           3190064048 ns (3.190 s) 100.0 %


## 7. Separate AXI HP Port for Output (Attempted — Reverted)

### Motivation
With 8 parallel output writes per pixel position sharing the same AXI port (master1) as input row reads, there was potential contention between reads and writes. The xc7z020 has 4 HP ports — we were only using 2 (HP0 for input+output, HP1 for filters+biases). Splitting output writes onto a dedicated HP port would eliminate read/write arbitration on master1.

### Changes Attempted
- **HLS**: Moved output from `bundle=master1` to `bundle=master3`, creating a third AXI master port
- **Vivado block design**: Enabled HP2, added `axi_mem_intercon_2`, wired `master3` → HP2 with clock/reset/address mapping

Port layout:
- HP0 (master1): input reads only
- HP1 (master2): filter + bias reads only
- HP2 (master3): output writes only

### Results
**Runtime was identical**: 3.190s → 3.190s. Per-layer times matched to the millisecond, confirming the design is entirely **compute-bound**. The AXI memory interface has more than enough bandwidth for the current access patterns — the bottleneck is the multiply-accumulate computation in the convolution pipeline, not DDR transfer throughput or port contention.

### Resource Cost
The additional AXI adapter logic increased resources for zero benefit:
- **LUTs**: 17117 (32%) → 18435 (34%)
- **FFs**: 12607 (11%) → 13339 (12%)

### Decision
Reverted to 2-port configuration (master1 for input+output, master2 for filters+biases) to keep the design simpler. Further speedup would require increasing compute throughput (more parallel filters or higher clock frequency), not memory bandwidth.

## 8. 16x Filter Parallelism

### Motivation
With N_PARALLEL=8 at 40% DSP and 34% BRAM, there was headroom to double the parallelism factor to 16, processing 16 output filters simultaneously.

### Changes
- Increased `N_PARALLEL` from 8 to 16
- Doubled all parallel arrays: `localFilters[16][...]`, `bias[16]`, `acc[16]`

### Results
| Layer | N=8 (ms) | N=16 (ms) | Speedup |
|-------|----------|-----------|---------|
| Conv 0 (3ch→32f) | 770 | 716 | 1.08× |
| Conv 1 (32ch→64f) | 749 | 535 | 1.40× |
| Conv 2 (64ch→64f) | 551 | 350 | 1.57× |
| Conv 3 (64ch→128f) | 446 | 256 | 1.74× |
| Conv 4 (128ch→256f) | 46 | 25 | 1.84× |
| **Total Conv** | **2562** | **1882** | **1.36×** |
| **Total** | **3190** | **2509** | **1.27×** |

Scaling improves with more filters — Conv 3 and Conv 4 benefit most because 128/256 filters divide evenly by 16. Conv 0 barely improves because with only 32 filters, going from 4 groups (N=8) to 2 groups (N=16) reduces overhead proportionally less.

### Resources
- **BRAM**: 96 (34%) → 160 (57%) — 16 separate filter BRAMs instead of 8
- **DSP**: 88 (40%) → 137 (62%) — 16 parallel multipliers instead of 8
- **LUT**: 17117 (32%) → 22196 (41%)
- **FF**: 12607 (11%) → 18290 (17%)

N=32 was also attempted but exceeded FPGA resources during synthesis.

## 9. Output Row Buffering and AXI Latency Hint Removal

### Motivation
Analysis of the cycle budget revealed that **output writes were the dominant bottleneck**, not the MAC computation. For Conv 0 (3 input channels), the compute loop takes only 27 cycles per pixel, but writing 16 output values to 16 different output filter planes required 16 individual scattered AXI transactions per pixel — each incurring transaction setup overhead. For Conv 0, the output writes were taking roughly 12× longer than the actual computation.

Additionally, the `latency=20` parameter on the AXI interface pragmas was telling HLS to pessimistically assume 20 cycles of AXI round-trip latency, causing the scheduler to insert unnecessary pipeline bubbles.

### Changes
1. **Output row buffer** (`outRowBuf[16][256]`): Instead of writing each output pixel directly to DDR as it's computed, we buffer an entire output row per parallel filter in local BRAM. After the full x-loop completes, each filter's row is burst-written to DDR as consecutive addresses — enabling efficient AXI burst transfers instead of scattered single-beat writes.

2. **Removed `latency=20`** from all four AXI `m_axi` interface pragmas, letting HLS use its default latency estimate. This allows the scheduler to overlap AXI transactions more aggressively.

### Results
| Layer | Before (ms) | After (ms) | Speedup |
|-------|-------------|------------|---------|
| Conv 0 (3ch→32f) | 716 | 100 | 7.2× |
| Conv 1 (32ch→64f) | 535 | 238 | 2.2× |
| Conv 2 (64ch→64f) | 350 | 215 | 1.6× |
| Conv 3 (64ch→128f) | 256 | 199 | 1.3× |
| Conv 4 (128ch→256f) | 25 | 23 | 1.1× |
| **Total Conv** | **1882** | **775** | **2.43×** |
| **Total** | **2509** | **1402** | **1.79×** |

Conv 0 saw the largest speedup (7.2×) because with only 3 input channels, its compute loop was very short and the scattered output writes dominated execution time. Deeper layers with more channels (64-256) saw smaller improvements because the compute loop already dominates over the output writes.

Overall speedup from the original software-only baseline: **86.4s → 1.402s = 61.6×**

### Resources
- **BRAM**: 160 (57%) → 176 (62%) — 16 extra BRAMs for the output row buffers
- **DSP**: 137 (62%) → 91 (41%) — reduced due to removing the `latency=20` hint allowing HLS to share DSP resources more efficiently
- **LUT**: 22196 (41%) → 17099 (32%) — also reduced
- **FF**: 18290 (17%) → 13367 (12%) — also reduced

### Why output buffering helps so much
Without buffering, each (x,y) pixel computation ended with 16 writes to scattered memory addresses (one per output filter plane), each creating a separate AXI write transaction. With buffering, the same 16×254 writes per row become 16 sequential burst writes of 254 consecutive elements each. AXI burst transfers amortize the transaction setup overhead across many data beats, making them dramatically more efficient than individual scattered writes.

### Remaining bottleneck
With Conv at 0.775s (55.3%), MaxPool is now the second-largest component at 0.477s (34.0%) — entirely in software on the ARM CPU. The FPGA sits idle during MaxPool execution.

## 10. Loop Flattening and Overhead Reduction

### Motivation
Cycle analysis showed a 2–5× overhead ratio between the estimated compute cycles and actual runtime. Three sources of overhead were identified:
1. **Per-channel burst restart in row loading**: The `ch` and `px` loops were separate, causing HLS to restart the AXI read pipeline for each channel boundary.
2. **Sequential filter loading**: Filters were loaded one at a time (outer loop over `p`, inner over `i`), causing 16 separate burst restarts per filter group.
3. **Conditional branch in output writes**: The `if ((iFilter+p) < numFilters)` guard inside the output burst loop prevented HLS from optimizing the burst pattern.

### Changes
1. **Removed `LOOP_FLATTEN off`** from row loading and prefetch loops, allowing HLS to merge the `ch` and `px` loops into a single flattened pipeline. This eliminates per-channel restart overhead — visible in the synthesis report as `VITIS_LOOP_85_5_VITIS_LOOP_86_6_VITIS_LOOP_89_7` (3 loops fused, II=1).

2. **Interleaved filter loading**: Restructured from sequential loading (outer `p`, inner `i`) to interleaved (outer `i`, inner `p` with `#pragma HLS UNROLL`). All 16 filters are now loaded simultaneously from a single sequential read stream, eliminating 16× burst restarts per filter group. Synthesis confirms II=16 on the filter load pipeline.

3. **Removed conditional from output burst write**: Changed from `for (p = 0; p < N_PARALLEL; ++p) { if ((iFilter+p) < numFilters) { ... } }` to `for (p = 0; p < nActive; ++p) { ... }`, removing the branch that prevented burst optimization.

### Results
| Layer | Before (ms) | After (ms) | Change |
|-------|-------------|------------|--------|
| Conv 0 (3ch→32f) | 100 | 99 | -1% |
| Conv 1 (32ch→64f) | 238 | 236 | -1% |
| Conv 2 (64ch→64f) | 215 | 211 | -2% |
| Conv 3 (64ch→128f) | 199 | 198 | -1% |
| Conv 4 (128ch→256f) | 23 | 26 | +13% |
| **Total Conv** | **775** | **770** | **-0.6%** |
| **Total** | **1402** | **1397** | **-0.4%** |

The changes produced a negligible improvement (~5ms). This confirms that the overhead was not from loop restart costs or conditional branches, but rather inherent AXI transaction setup and pipeline drain costs that cannot be eliminated through pragma-level changes alone.

### Resources
- **BRAM**: 176 (62%) → 176 (62%) — unchanged
- **DSP**: 91 (41%) → 92 (41%) — +1 (negligible)
- **LUT**: 17099 (32%) → 16976 (31%) — slight decrease
- **FF**: 13367 (12%) → 20575 (19%) — increase due to flattened loop control logic being inlined into pipeline registers

The changes are kept for cleaner code structure despite minimal performance impact.

## 11. 2× X-Position Parallelism (DSP Utilization)

### Motivation
After all previous optimizations, DSP usage sat at only 92/220 (41%) — nearly 60% of the FPGA's multiply-accumulate resources were unused. The key insight is that for two adjacent output x-positions, the **filter coefficients are identical** (same channel, cy, cx) — only the input pixel values differ (offset by 1). This means x-parallelism can double the DSP usage without duplicating the filter BRAM storage.

### Changes
1. **X-loop processes 2 pixels per iteration**: The x-loop now steps by 2, computing two output pixels simultaneously using separate accumulator arrays (`acc0[16]` and `acc1[16]`). Each cycle reads two adjacent pixels (`pix0`, `pix1`) and multiplies both by the same filter value.

2. **`rowBuffer` partitioned** with `cyclic factor=2 dim=2`: Enables two simultaneous reads from adjacent addresses in the same cycle, feeding both pixel pipelines.

3. **`outRowBuf` partitioned** with `cyclic factor=2 dim=2`: Enables two simultaneous writes per cycle to store both computed pixels.

### Results
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

### Resources
- **BRAM**: 176 (62%) → 192 (68%) — +16 from rowBuffer and outRowBuf partitioning
- **DSP**: 92 (41%) → 140 (63%) — compute pipeline doubled from 52 to 100 DSPs
- **LUT**: 16976 (31%) → 24920 (46%) — additional multiplexing and control logic
- **FF**: 20575 (19%) → 20913 (19%) — essentially unchanged

### Why filter reads don't need duplication
Unlike cx-unrolling (which failed due to BRAM constraints), x-parallelism reads the same `localFilters[p][...]` value for both pixel computations. The filter address depends on `(iChannel, cy, cx)` — not on `x`. Only `rowBuffer` needs dual-port access, and since the two pixel addresses differ by exactly 1, `cyclic factor=2` partitioning places them in different BRAM banks, enabling conflict-free parallel reads.

### Current bottleneck
Conv (43.5%) and MaxPool (43.1%) are now virtually tied. The FPGA sits idle nearly half the total inference time waiting for software MaxPool to complete on the ARM CPU.

## 12. MaxPool Hardware Accelerator

### Motivation
After all Conv2D optimizations, Conv (43.5%) and MaxPool (43.1%) were virtually tied as bottlenecks. The ARM CPU spent 0.481s on software MaxPool — nearly half the total inference time — while the FPGA sat idle. MaxPool is a simple 2×2 max operation with stride 2, requiring zero multiplications (no DSPs), making it an ideal candidate for a lightweight hardware accelerator.

### HLS Design
The MaxPool accelerator ([maxpool.cpp](HLS/HLS_MaxPool/maxpool.cpp)) implements 2×2 max pooling with stride 2:

- **Separate AXI ports**: `gmem0` for input reads, `gmem1` for output writes — no read/write contention
- **Row-buffered**: Burst-reads 2 input rows into local BRAM (`rowBuf0`, `rowBuf1`), computes 2×2 max, burst-writes the output row (`outBuf`)
- **Handles odd dimensions**: Matches the software MaxPool behavior by truncating odd width/height
- **Zero DSPs**: Pure comparison logic (no multiplies), minimal BRAM footprint (3 × 256-entry buffers)

### Vivado Integration
- **HP2 and HP3**: MaxPool `gmem0` routed through `axi_mem_intercon_2` to HP2, `gmem1` through `axi_mem_intercon_3` to HP3
- **AXI peripheral bus**: `ps7_0_axi_periph` expanded to 2 master ports — M00 for Conv2D control, M01 for MaxPool control
- **Address**: MaxPool control registers at `0x40010000`

### Software Integration
- **CMaxPoolProxy** ([CMaxPoolProxy.hpp](Solution/src/CMaxPoolProxy.hpp), [CMaxPoolProxy.cpp](Solution/src/CMaxPoolProxy.cpp)): Follows the same `CAccelProxy` pattern as `CConv2DProxy`. Register offsets verified against HLS-generated `xmaxpool_hw_hw.h`.
- **DMA address sharing**: Since buffers are allocated via the Conv2D proxy, the MaxPool proxy uses `ShareDMAMappings()` to look up virtual→physical address translations from the Conv2D proxy's mapping table without copying (avoids double-free on cleanup).
- **Inference calls**: All 5 `MaxPool(...)` software calls replaced with `maxpooler.MaxPool_HW(...)` in [model.cpp](Solution/src/model.cpp).

### Results
| Layer | SW MaxPool (ms) | HW MaxPool (ms) | Speedup |
|-------|----------------|-----------------|---------|
| MaxPool 0 (32ch, 254×254) | 266 | 40.3 | 6.6× |
| MaxPool 1 (64ch, 125×125) | 126 | 21.4 | 5.9× |
| MaxPool 2 (128ch, 60×60) | 59 | 12.1 | 4.9× |
| MaxPool 3 (256ch, 28×28) | 26 | 7.3 | 3.6× |
| MaxPool 4 (64ch, 12×12) | 1.2 | 0.6 | 2.0× |
| **Total MaxPool** | **478** | **82** | **5.8×** |
| **Total inference** | **1116** | **744** | **1.50×** |

### Current bottleneck breakdown
- **Conv (HW): 68.9%** (0.512s)
- **Dense (SW): 20.0%** (0.149s)
- **MaxPool (HW): 11.0%** (0.082s)
- Flatten + Sigmoid: ~0.1%

Runtime HW with MaxPool accelerator:

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Opening MaxPool accelerator at 0x40010000...
[HW] MaxPool accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...
[HW] OUTPUT: 0.93905449 --> DOG
Conv 0 (HW) --> 73981713 ns (0.074 s)
Conv 1 (HW) --> 145555357 ns (0.146 s)
Conv 2 (HW) --> 132664840 ns (0.133 s)
Conv 3 (HW) --> 137935606 ns (0.138 s)
Conv 4 (HW) --> 22146129 ns (0.022 s)
MaxPool 0 --> 40320624 ns (0.040 s)
MaxPool 1 --> 21421384 ns (0.021 s)
MaxPool 2 --> 12117154 ns (0.012 s)
MaxPool 3 --> 7318206 ns (0.007 s)
MaxPool 4 --> 562018 ns (0.001 s)
Dense 5 (SW) --> 149007061 ns (0.149 s)
Dense 6 (SW) --> 67908 ns (0.000 s)
Total Conv time (HW): 512283645 ns (0.512 s) 68.9 %
Total MaxPool time:   81739386 ns (0.082 s) 11.0 %
Total Dense time (SW):149074969 ns (0.149 s) 20.0 %
Total Flatten time:   448108 ns (0.000 s) 0.1 %
Total Sigmoid time:   140692 ns (0.000 s) 0.0 %
Total time:           743686800 ns (0.744 s) 100.0 %

## 13. MaxPool Merged Burst Reads

### Motivation
In the initial MaxPool HLS design, each row-pair required two separate AXI burst reads (one per row). Since the data is stored in CHW layout, consecutive rows within a channel are contiguous in memory — meaning both rows can be read in a single burst transaction, halving the burst setup overhead.

### Changes
- **[maxpool.cpp](HLS/HLS_MaxPool/maxpool.cpp)**: Replaced two separate row buffers (`rowBuf0[256]`, `rowBuf1[256]`) with a single combined buffer (`rowBuf[512]`). The two-row read is now a single loop of `2 * width` iterations reading contiguous memory. The compute phase accesses row 0 at `rowBuf[c2]` and row 1 at `rowBuf[width + c2]`.

### Results
| Layer | Before (ms) | After (ms) | Change |
|-------|-------------|------------|--------|
| MaxPool 0 (32ch, 254×254) | 40.3 | 39.0 | -3.2% |
| MaxPool 1 (64ch, 125×125) | 21.4 | 20.3 | -5.1% |
| MaxPool 2 (128ch, 60×60) | 12.1 | 10.9 | -9.9% |
| MaxPool 3 (256ch, 28×28) | 7.3 | 6.2 | -15.1% |
| MaxPool 4 (64ch, 12×12) | 0.6 | 0.4 | -33.3% |
| **Total MaxPool** | **82** | **77** | **-6.1%** |
| **Total inference** | **744** | **739** | **-0.7%** |

Smaller layers saw the largest relative improvement because burst setup overhead is a larger fraction of the total transfer time when fewer elements are read per burst. The overall impact is marginal because MaxPool is only ~10% of total inference time.

### Current bottleneck breakdown
- **Conv (HW): 69.4%** (0.512s)
- **Dense (SW): 20.2%** (0.149s)
- **MaxPool (HW): 10.4%** (0.077s)
- Flatten + Sigmoid: ~0.1%

MaxPool is now at diminishing returns — even eliminating it entirely would only save 0.077s. The dominant bottlenecks are Conv (at DSP/BRAM limits on the xc7z020) and Dense (running in software on the ARM CPU).

Runtime HW with MaxPool merged burst reads:

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Opening MaxPool accelerator at 0x40010000...
[HW] MaxPool accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...
[HW] OUTPUT: 0.93905449 --> DOG
Conv 0 (HW) --> 73983754 ns (0.074 s)
Conv 1 (HW) --> 145554249 ns (0.146 s)
Conv 2 (HW) --> 132661283 ns (0.133 s)
Conv 3 (HW) --> 137966246 ns (0.138 s)
Conv 4 (HW) --> 22144372 ns (0.022 s)
MaxPool 0 --> 39030926 ns (0.039 s)
MaxPool 1 --> 20305917 ns (0.020 s)
MaxPool 2 --> 10880117 ns (0.011 s)
MaxPool 3 --> 6168662 ns (0.006 s)
MaxPool 4 --> 444911 ns (0.000 s)
Dense 5 (SW) --> 148907560 ns (0.149 s)
Dense 6 (SW) --> 65994 ns (0.000 s)
Total Conv time (HW): 512309904 ns (0.512 s) 69.4 %
Total MaxPool time:   76830533 ns (0.077 s) 10.4 %
Total Dense time (SW):148973554 ns (0.149 s) 20.2 %
Total Flatten time:   451895 ns (0.000 s) 0.1 %
Total Sigmoid time:   144701 ns (0.000 s) 0.0 %
Total time:           738710587 ns (0.739 s) 100.0 %

## Summary of All Files Modified

### HLS Source Files
- **[HLS/HLS_Conv/conv2d.cpp](HLS/HLS_Conv/conv2d.cpp)** — Conv2D HLS accelerator. Optimizations applied: dual AXI ports, interface tuning, filter caching, input row caching (4-buffer prefetch), 16× filter parallelism, output row buffering, loop flattening, interleaved filter loading, 2× x-position parallelism.
- **[HLS/HLS_Conv/conv2d.h](HLS/HLS_Conv/conv2d.h)** — Conv2D HLS header. Fixed-point type definitions, function signature with default parameters.
- **[HLS/HLS_MaxPool/maxpool.cpp](HLS/HLS_MaxPool/maxpool.cpp)** — MaxPool HLS accelerator. 2×2 max pooling with stride 2, separate AXI read/write ports, merged burst row reads, output row buffering.
- **[HLS/HLS_MaxPool/maxpool.h](HLS/HLS_MaxPool/maxpool.h)** — MaxPool HLS header.

### HLS Synthesis Scripts
- **[HLS/Scripts/Conv2D_HW_HLS_impl.tcl](HLS/Scripts/Conv2D_HW_HLS_impl.tcl)** — Conv2D HLS synthesis and IP export script.
- **[HLS/Scripts/MaxPool_HW_HLS_impl.tcl](HLS/Scripts/MaxPool_HW_HLS_impl.tcl)** — MaxPool HLS synthesis and IP export script. Targets xc7z020, 10ns clock, exports to `IP-repo/MaxPool_HW.zip`.

### Vivado Scripts
- **[Vivado/Scripts/Conv2D_HW_Vivado.tcl](Vivado/Scripts/Conv2D_HW_Vivado.tcl)** — Block design creation. Added: MaxPool_HW IP instance, 2 additional AXI interconnects (for MaxPool gmem0→HP2, gmem1→HP3), expanded peripheral interconnect to 2 master ports (M00 for Conv2D, M01 for MaxPool), IP repo paths updated to reference per-IP subdirectories.
- **[Vivado/Scripts/Conv2D_HW_Vivado_impl.tcl](Vivado/Scripts/Conv2D_HW_Vivado_impl.tcl)** — Vivado synthesis and implementation script (unchanged).

### Software Source Files
- **[Solution/src/CAccelProxy.hpp](Solution/src/CAccelProxy.hpp)** — Base accelerator proxy. Added `sharedDMAProxy` pointer and `ShareDMAMappings()` method to allow one proxy to look up DMA address translations from another without copying (avoids double-free).
- **[Solution/src/CAccelProxy.cpp](Solution/src/CAccelProxy.cpp)** — Updated constructor to initialize `sharedDMAProxy = NULL`. Updated `GetDMAPhysicalAddr()` to fall back to the shared proxy's mapping table when the address is not in the local map.
- **[Solution/src/CConv2DProxy.hpp](Solution/src/CConv2DProxy.hpp)** — Conv2D proxy header (unchanged).
- **[Solution/src/CConv2DProxy.cpp](Solution/src/CConv2DProxy.cpp)** — Conv2D proxy implementation (unchanged).
- **[Solution/src/CMaxPoolProxy.hpp](Solution/src/CMaxPoolProxy.hpp)** — MaxPool proxy header. Register struct matching HLS-generated offsets (control at 0x00, input_r at 0x10, output_r at 0x18, channels at 0x20, width at 0x28, height at 0x30). Base address 0x40010000.
- **[Solution/src/CMaxPoolProxy.cpp](Solution/src/CMaxPoolProxy.cpp)** — MaxPool proxy implementation. Translates virtual→physical addresses, writes registers, starts accelerator, polls for completion.
- **[Solution/src/model.h](Solution/src/model.h)** — Added `#include "CMaxPoolProxy.hpp"`. Updated `Inference()` signature to accept `CMaxPoolProxy& maxpooler`.
- **[Solution/src/model.cpp](Solution/src/model.cpp)** — Updated `Inference()` to accept `CMaxPoolProxy& maxpooler`. Replaced all 5 software `MaxPool()` calls with `maxpooler.MaxPool_HW()`.
- **[Solution/src/cnnSolver.cpp](Solution/src/cnnSolver.cpp)** — Added MaxPool proxy initialization at 0x40010000. Added `maxpooler.ShareDMAMappings(convolver)` after buffer allocation. Passed `maxpooler` to `Inference()`.
- **[Solution/src/cnn.h](Solution/src/cnn.h)** — CNN function declarations (unchanged).
- **[Solution/src/cnn.cpp](Solution/src/cnn.cpp)** — CNN function implementations including software MaxPool, Dense, Flatten, ReLU, Sigmoid (unchanged — software MaxPool kept for reference).

### Build Files
- **[Makefile](Makefile)** — Top-level Makefile. Added `ip_conv` and `ip_maxpool` targets. `make ip` now builds both. `vivado_project` unzips both IPs into separate subdirectories. Clean targets updated to remove MaxPool HLS project and IP.
- **[Solution/Makefile](Solution/Makefile)** — Software Makefile. Added `CMaxPoolProxy.o` to compile and link targets.

### Final performance summary
Overall speedup from the original software-only baseline: **28.0s → 0.739s = 37.9×**
Overall speedup from the initial HW accelerator: **86.4s → 0.739s = 116.9×**
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

### Impact
- **Conv time**: 12.2s → 4.2s (**2.9x speedup**, close to theoretical 4x)
- **Total time**: 12.9s → 4.9s (**2.6x overall speedup**)
- **BRAM**: 40 (14%) → 64 (22%) — 4 separate filter BRAMs
- **DSPs**: 15 (6%) → 66 (30%) — 4x multipliers for parallel MACs
- **LUTs**: 10369 (19%) → 15986 (30%) — parallel control logic
- **FFs**: 6494 (6%) → 12978 (12%) — parallel pipeline registers
- The gap from ideal 4x speedup comes from filter loading overhead (4 filters loaded sequentially per group)

### Per-layer breakdown

| Layer | Channels | Filters | Before (s) | After (s) | Speedup |
|-------|----------|---------|------------|-----------|---------|
| Conv 0 | 3 | 32 | 1.603 | 1.046 | 1.5x |
| Conv 1 | 32 | 64 | 3.785 | 1.259 | 3.0x |
| Conv 2 | 64 | 128 | 3.389 | 0.991 | 3.4x |
| Conv 3 | 128 | 256 | 3.101 | 0.841 | 3.7x |
| Conv 4 | 256 | 64 | 0.337 | 0.088 | 3.8x |

Layers with more filters see greater speedup (closer to 4x) because the filter loading overhead is amortized over more spatial computation. Layer 0 (only 3 input channels, 32 filters) benefits least because the computation per filter is small relative to the loading cost.

Runtime HW with 4x filter parallelism:

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

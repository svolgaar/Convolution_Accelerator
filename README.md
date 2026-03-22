# Convolution_Accelerator
Midterm project for the Class HW-SW Codesign on using an PYNQ FPGA board to accelerate a convolution algorithm using HLS.

## Execution Time and Resource Usage

| Description       | Width | Height | Channels | Filters | Time (ms) | LUTs       | FFs        | BRAMs | DSPs     |
|-------------------|-------|--------|----------|---------|-----------|------------|------------|-------|----------|
| SW                | 256   | 256    | 16       | 32      | 28000     | n/a        | n/a        | n/a   | n/a      |
| Initial solution  | 256   | 256    | 16       | 32      | 84563     | 5316 (9%)  | 5838 (5%)  | -     | 39 (17%) |
| + Dual AXI + Interface tuning + Filter caching | 256 | 256 | 16 | 32 | 12945 | 9180 (17%) | 5977 (5%) | 8 (2%) | 30 (13%) |

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









[HW] OUTPUT: 0.00000000 --> CAT
Conv 0 (HW) --> 5499389998 ns (5.499 s)
Conv 1 (HW) --> 28145986263 ns (28.146 s)
Conv 2 (HW) --> 25828306353 ns (25.828 s)
Conv 3 (HW) --> 22380201595 ns (22.380 s)
Conv 4 (HW) --> 2064445711 ns (2.064 s)
MaxPool 0 --> 266146365 ns (0.266 s)
MaxPool 1 --> 125632584 ns (0.126 s)
MaxPool 2 --> 58880434 ns (0.059 s)
MaxPool 3 --> 25615150 ns (0.026 s)
MaxPool 4 --> 1365483 ns (0.001 s)
Dense 5 (SW) --> 166148606 ns (0.166 s)
Dense 6 (SW) --> 66117 ns (0.000 s)
Total Conv time (HW): 83918329920 ns (83.918 s) 99.2 %
Total MaxPool time:   477640016 ns (0.478 s) 0.6 %
Total Dense time (SW):166214723 ns (0.166 s) 0.2 %
Total Flatten time:   457166 ns (0.000 s) 0.0 %
Total Sigmoid time:   169139 ns (0.000 s) 0.0 %
Total time:           84562810964 ns (84.563 s) 100.0 %


Runtime HW with filter cacheing and improved AXI port interfacing

xilinx@pynq:~/dogs_cats$ sudo ./cnnSolver dog.9499.jpg.rgba.planar
[HW] Opening Conv2D accelerator at 0x40000000...
[HW] Accelerator opened successfully.
[HW] Allocating DMA-compatible buffers...
[HW] DMA buffers allocated.
[HW] Running inference with Conv2D accelerator...
[HW] OUTPUT: 0.00000000 --> CAT
Conv 0 (HW) --> 858073553 ns (0.858 s)
Conv 1 (HW) --> 4182613227 ns (4.183 s)
Conv 2 (HW) --> 3759668339 ns (3.760 s)
Conv 3 (HW) --> 3222368220 ns (3.222 s)
Conv 4 (HW) --> 295277190 ns (0.295 s)
MaxPool 0 --> 266066550 ns (0.266 s)
MaxPool 1 --> 125711341 ns (0.126 s)
MaxPool 2 --> 58848483 ns (0.059 s)
MaxPool 3 --> 25598252 ns (0.026 s)
MaxPool 4 --> 1187157 ns (0.001 s)
Dense 5 (SW) --> 148922705 ns (0.149 s)
Dense 6 (SW) --> 65751 ns (0.000 s)
Total Conv time (HW): 12318000529 ns (12.318 s) 95.2 %
Total MaxPool time:   477411783 ns (0.477 s) 3.7 %
Total Dense time (SW):148988456 ns (0.149 s) 1.2 %
Total Flatten time:   451951 ns (0.000 s) 0.0 %
Total Sigmoid time:   174865 ns (0.000 s) 0.0 %
Total time:           12945027584 ns (12.945 s) 100.0 %
xilinx@pynq:~/dogs_cats$ 

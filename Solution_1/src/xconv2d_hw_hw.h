// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
// Tool Version Limit: 2019.12
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// ==============================================================
// control
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read/COR)
//        bit 7  - auto_restart (Read/Write)
//        bit 9  - interrupt (Read)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0 - enable ap_done interrupt (Read/Write)
//        bit 1 - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0 - ap_done (Read/TOW)
//        bit 1 - ap_ready (Read/TOW)
//        others - reserved
// 0x10 : Data signal of input_r
//        bit 31~0 - input_r[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of output_r
//        bit 31~0 - output_r[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of coeffs
//        bit 31~0 - coeffs[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of numChannels
//        bit 31~0 - numChannels[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of numFilters
//        bit 31~0 - numFilters[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of inputWidth
//        bit 31~0 - inputWidth[31:0] (Read/Write)
// 0x3c : reserved
// 0x40 : Data signal of inputHeight
//        bit 31~0 - inputHeight[31:0] (Read/Write)
// 0x44 : reserved
// 0x48 : Data signal of convWidth
//        bit 31~0 - convWidth[31:0] (Read/Write)
// 0x4c : reserved
// 0x50 : Data signal of convHeight
//        bit 31~0 - convHeight[31:0] (Read/Write)
// 0x54 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCONV2D_HW_CONTROL_ADDR_AP_CTRL          0x00
#define XCONV2D_HW_CONTROL_ADDR_GIE              0x04
#define XCONV2D_HW_CONTROL_ADDR_IER              0x08
#define XCONV2D_HW_CONTROL_ADDR_ISR              0x0c
#define XCONV2D_HW_CONTROL_ADDR_INPUT_R_DATA     0x10
#define XCONV2D_HW_CONTROL_BITS_INPUT_R_DATA     32
#define XCONV2D_HW_CONTROL_ADDR_OUTPUT_R_DATA    0x18
#define XCONV2D_HW_CONTROL_BITS_OUTPUT_R_DATA    32
#define XCONV2D_HW_CONTROL_ADDR_COEFFS_DATA      0x20
#define XCONV2D_HW_CONTROL_BITS_COEFFS_DATA      32
#define XCONV2D_HW_CONTROL_ADDR_NUMCHANNELS_DATA 0x28
#define XCONV2D_HW_CONTROL_BITS_NUMCHANNELS_DATA 32
#define XCONV2D_HW_CONTROL_ADDR_NUMFILTERS_DATA  0x30
#define XCONV2D_HW_CONTROL_BITS_NUMFILTERS_DATA  32
#define XCONV2D_HW_CONTROL_ADDR_INPUTWIDTH_DATA  0x38
#define XCONV2D_HW_CONTROL_BITS_INPUTWIDTH_DATA  32
#define XCONV2D_HW_CONTROL_ADDR_INPUTHEIGHT_DATA 0x40
#define XCONV2D_HW_CONTROL_BITS_INPUTHEIGHT_DATA 32
#define XCONV2D_HW_CONTROL_ADDR_CONVWIDTH_DATA   0x48
#define XCONV2D_HW_CONTROL_BITS_CONVWIDTH_DATA   32
#define XCONV2D_HW_CONTROL_ADDR_CONVHEIGHT_DATA  0x50
#define XCONV2D_HW_CONTROL_BITS_CONVHEIGHT_DATA  32


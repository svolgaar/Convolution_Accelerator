// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
// Tool Version Limit: 2019.12
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// ==============================================================
/***************************** Include Files *********************************/
#include "xconv2d_hw.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XConv2d_hw_CfgInitialize(XConv2d_hw *InstancePtr, XConv2d_hw_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XConv2d_hw_Start(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_AP_CTRL) & 0x80;
    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XConv2d_hw_IsDone(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XConv2d_hw_IsIdle(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XConv2d_hw_IsReady(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XConv2d_hw_EnableAutoRestart(XConv2d_hw *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_AP_CTRL, 0x80);
}

void XConv2d_hw_DisableAutoRestart(XConv2d_hw *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_AP_CTRL, 0);
}

void XConv2d_hw_Set_input_r(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_INPUT_R_DATA, Data);
}

u32 XConv2d_hw_Get_input_r(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_INPUT_R_DATA);
    return Data;
}

void XConv2d_hw_Set_output_r(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_OUTPUT_R_DATA, Data);
}

u32 XConv2d_hw_Get_output_r(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_OUTPUT_R_DATA);
    return Data;
}

void XConv2d_hw_Set_filters(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_FILTERS_DATA, Data);
}

u32 XConv2d_hw_Get_filters(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_FILTERS_DATA);
    return Data;
}

void XConv2d_hw_Set_numChannels(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_NUMCHANNELS_DATA, Data);
}

u32 XConv2d_hw_Get_numChannels(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_NUMCHANNELS_DATA);
    return Data;
}

void XConv2d_hw_Set_numFilters(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_NUMFILTERS_DATA, Data);
}

u32 XConv2d_hw_Get_numFilters(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_NUMFILTERS_DATA);
    return Data;
}

void XConv2d_hw_Set_inputWidth(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_INPUTWIDTH_DATA, Data);
}

u32 XConv2d_hw_Get_inputWidth(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_INPUTWIDTH_DATA);
    return Data;
}

void XConv2d_hw_Set_inputHeight(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_INPUTHEIGHT_DATA, Data);
}

u32 XConv2d_hw_Get_inputHeight(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_INPUTHEIGHT_DATA);
    return Data;
}

void XConv2d_hw_Set_convWidth(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_CONVWIDTH_DATA, Data);
}

u32 XConv2d_hw_Get_convWidth(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_CONVWIDTH_DATA);
    return Data;
}

void XConv2d_hw_Set_convHeight(XConv2d_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_CONVHEIGHT_DATA, Data);
}

u32 XConv2d_hw_Get_convHeight(XConv2d_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_CONVHEIGHT_DATA);
    return Data;
}

void XConv2d_hw_InterruptGlobalEnable(XConv2d_hw *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_GIE, 1);
}

void XConv2d_hw_InterruptGlobalDisable(XConv2d_hw *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_GIE, 0);
}

void XConv2d_hw_InterruptEnable(XConv2d_hw *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_IER);
    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_IER, Register | Mask);
}

void XConv2d_hw_InterruptDisable(XConv2d_hw *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_IER);
    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_IER, Register & (~Mask));
}

void XConv2d_hw_InterruptClear(XConv2d_hw *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv2d_hw_WriteReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_ISR, Mask);
}

u32 XConv2d_hw_InterruptGetEnabled(XConv2d_hw *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_IER);
}

u32 XConv2d_hw_InterruptGetStatus(XConv2d_hw *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XConv2d_hw_ReadReg(InstancePtr->Control_BaseAddress, XCONV2D_HW_CONTROL_ADDR_ISR);
}


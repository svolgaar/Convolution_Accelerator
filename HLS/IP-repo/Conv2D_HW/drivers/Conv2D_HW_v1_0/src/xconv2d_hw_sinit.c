// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2 (64-bit)
// Tool Version Limit: 2019.12
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#include "xparameters.h"
#include "xconv2d_hw.h"

extern XConv2d_hw_Config XConv2d_hw_ConfigTable[];

XConv2d_hw_Config *XConv2d_hw_LookupConfig(u16 DeviceId) {
	XConv2d_hw_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XCONV2D_HW_NUM_INSTANCES; Index++) {
		if (XConv2d_hw_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XConv2d_hw_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XConv2d_hw_Initialize(XConv2d_hw *InstancePtr, u16 DeviceId) {
	XConv2d_hw_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XConv2d_hw_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XConv2d_hw_CfgInitialize(InstancePtr, ConfigPtr);
}

#endif


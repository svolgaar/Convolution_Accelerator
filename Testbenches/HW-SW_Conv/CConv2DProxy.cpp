#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include "util.hpp"
#include "CAccelProxy.hpp"
#include "CConv2DProxy.hpp"

uint32_t CConv2DProxy::Conv2D_HW(void *input, void * output, void * coeffs,
      uint32_t numChannels, uint32_t numFilters,
      uint32_t inputWidth, uint32_t inputHeight,
      uint32_t convWidth, uint32_t convHeight)
{
  volatile TRegs * regs = (TRegs*)accelRegs;
  uint32_t phyInput, phyOutput, phyCoeffs;
  uint32_t status;

  if (logging)
    printf("CConv2DProxy::Conv2D_HW(Input=0x%08X, Output=0x%08X, Coeffes=0x%08X, "
          "NumChannels=%u, NumFilters=%u, Width=%u, Height=%u, FilterSize=%ux%u)\n", 
          (uint32_t)input, (uint32_t)output, (uint32_t)coeffs,
          numChannels, numFilters, inputWidth, inputHeight, convWidth, convHeight);

  if (accelRegs == NULL) {
    if (logging)
      printf("Error: Calling Conv2D_HW() on a non-initialized accelerator.\n");
    return DEVICE_NOT_INITIALIZED;
  }

  // We need to obtain the physical addresses corresponding to each of the virtual addresses passed by the application.
  // The accelerator uses only the physical addresses (and only contiguous memory).
  phyInput = GetDMAPhysicalAddr(input);
  if (phyInput == 0) {
    if (logging)
      printf("Error: No physical address found for virtual address 0x%08X\n", (uint32_t)input);
    return VIRT_ADDR_NOT_FOUND;
  }
  phyOutput = GetDMAPhysicalAddr(output);
  if (phyOutput == 0) {
    if (logging)
      printf("Error: No physical address found for virtual address 0x%08X\n", (uint32_t)output);
    return VIRT_ADDR_NOT_FOUND;
  }
  phyCoeffs = GetDMAPhysicalAddr(coeffs);
  if (phyCoeffs == 0) {
    if (logging)
      printf("Error: No physical address found for virtual address 0x%08X\n", (uint32_t)coeffs);
    return VIRT_ADDR_NOT_FOUND;
  }

  // Write to registers (todo) DONE
  regs->input_r = phyInput;
  regs->output_r = phyOutput;
  regs->coeffs = phyCoeffs;
  regs->numChannels = numChannels;
  regs->numFilters = numFilters;
  regs->inputWidth = inputWidth;
  regs->inputHeight = inputHeight;
  regs->convWidth = convWidth;
  regs->convHeight = convHeight;

  if (logging)
    printf("\nStarting accel...\n");

  // Send start command to the accel
  do {
    regs->control = 0x01;
    status = regs->control;
  } while ((status & 0x01) == 0);
  // Wait for done signal from the accel
  do {
    status = regs->control;
  } while ((status & 0x02) == 0);

  return OK;
}


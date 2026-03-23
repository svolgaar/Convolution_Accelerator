#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include "util.hpp"
#include "CAccelProxy.hpp"
#include "CMaxPoolProxy.hpp"

uint32_t CMaxPoolProxy::MaxPool_HW(void *input, void *output,
    uint32_t channels, uint32_t width, uint32_t height)
{
  volatile TRegs * regs = (TRegs*)accelRegs;
  uint32_t status;

  if (logging)
    printf("CMaxPoolProxy::MaxPool_HW(Input=0x%08X, Output=0x%08X, "
          "Channels=%u, Width=%u, Height=%u)\n",
          (uint32_t)input, (uint32_t)output,
          channels, width, height);

  if (accelRegs == NULL) {
    if (logging)
      printf("Error: Calling MaxPool_HW() on a non-initialized accelerator.\n");
    return DEVICE_NOT_INITIALIZED;
  }

  uint32_t phyInput = GetDMAPhysicalAddr(input);
  if (phyInput == 0) {
    if (logging)
      printf("Error: No physical address found for virtual address 0x%08X\n", (uint32_t)input);
    return VIRT_ADDR_NOT_FOUND;
  }
  uint32_t phyOutput = GetDMAPhysicalAddr(output);
  if (phyOutput == 0) {
    if (logging)
      printf("Error: No physical address found for virtual address 0x%08X\n", (uint32_t)output);
    return VIRT_ADDR_NOT_FOUND;
  }

  // Write to registers
  regs->input_r = phyInput;
  regs->output_r = phyOutput;
  regs->channels = channels;
  regs->width = width;
  regs->height = height;

  // Send start command
  do {
    regs->control = 0x01;
    status = regs->control;
  } while ((status & 0x01) == 0);

  // Wait for done
  do {
    status = regs->control;
  } while ((status & 0x02) == 0);

  return OK;
}

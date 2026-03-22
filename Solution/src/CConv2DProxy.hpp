#ifndef CCONV2D_HPP
#define CCONV2D_HPP


#define CONV2D_HW_ADDR  0x40000000
#define MAP_SIZE        65536

#include <stdint.h>
#include <map>


class CConv2DProxy : public CAccelProxy {
  protected:
    // Structure that mimics the layout of the peripheral registers.
    // Vitis HLS skips some addresses in the register file. We introduce
    // padding fields to create the right mapping to registers with our structure,
    /** @todo DONE*/
    struct TRegs {
      uint32_t control; // 0x00
      uint32_t gier, ier, isr; // 0x04, 0x08, 0x0C
      uint32_t input_r; // 0x10
      uint32_t padding0; // 0x14
      uint32_t output_r; // 0x18
      uint32_t padding1; // 0x1C
      uint32_t coeffs; // 0x20
      uint32_t padding2; // 0x24
      uint32_t biases; // 0x28
      uint32_t padding3; // 0x2C
      uint32_t numChannels; // 0x30
      uint32_t padding4; // 0x34
      uint32_t numFilters; // 0x38
      uint32_t padding5; // 0x3C
      uint32_t inputWidth; // 0x40
      uint32_t padding6; // 0x44
      uint32_t inputHeight; // 0x48
      uint32_t padding7; // 0x4C
      uint32_t convWidth; // 0x50
      uint32_t padding8; // 0x54
      uint32_t convHeight; // 0x58
      uint32_t padding9; // 0x5C
      uint32_t relu; // 0x60
    };

  public:
    CConv2DProxy(bool Logging = false)
      : CAccelProxy(Logging) {}

    ~CConv2DProxy() {}

    uint32_t Conv2D_HW(void *input, void * output, void * coeffs, void * biases,
      uint32_t numChannels, uint32_t numFilters,
      uint32_t inputWidth, uint32_t inputHeight,
      uint32_t convWidth = 3, uint32_t convHeight = 3, uint32_t relu = 0);
};

#endif  // CCONV2D_HPP


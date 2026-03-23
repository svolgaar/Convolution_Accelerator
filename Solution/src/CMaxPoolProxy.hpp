#ifndef CMAXPOOL_HPP
#define CMAXPOOL_HPP

#define MAXPOOL_HW_ADDR  0x40010000

#include <stdint.h>
#include <map>

class CMaxPoolProxy : public CAccelProxy {
  protected:
    // Register layout matching HLS-generated MaxPool_HW s_axilite interface.
    // Parameters: input, output, channels, width, height
    // Verify offsets against xmaxpool_hw_hw.h after synthesis.
    struct TRegs {
      uint32_t control;   // 0x00
      uint32_t gier;      // 0x04
      uint32_t ier;       // 0x08
      uint32_t isr;       // 0x0C
      uint32_t input_r;   // 0x10
      uint32_t padding0;  // 0x14
      uint32_t output_r;  // 0x18
      uint32_t padding1;  // 0x1C
      uint32_t channels;  // 0x20
      uint32_t padding2;  // 0x24
      uint32_t width;     // 0x28
      uint32_t padding3;  // 0x2C
      uint32_t height;    // 0x30
    };

  public:
    CMaxPoolProxy(bool Logging = false)
      : CAccelProxy(Logging) {}

    ~CMaxPoolProxy() {}

    uint32_t MaxPool_HW(void *input, void *output,
      uint32_t channels, uint32_t width, uint32_t height);
};

#endif  // CMAXPOOL_HPP

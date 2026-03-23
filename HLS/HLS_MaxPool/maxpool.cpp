#include "maxpool.h"

void MaxPool_HW(TFXP_MP *input, TFXP_MP *output,
    uint32_t channels, uint32_t width, uint32_t height)
{
#pragma HLS INTERFACE m_axi port=input bundle=gmem0 offset=slave depth=2064512 max_read_burst_length=256 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=output bundle=gmem1 offset=slave depth=516128 max_write_burst_length=256 num_write_outstanding=16
#pragma HLS INTERFACE s_axilite port=input
#pragma HLS INTERFACE s_axilite port=output
#pragma HLS INTERFACE s_axilite port=channels
#pragma HLS INTERFACE s_axilite port=width
#pragma HLS INTERFACE s_axilite port=height
#pragma HLS INTERFACE s_axilite port=return

  // Combined row buffer: 2 rows read in a single burst (contiguous in memory)
  // Max: 2 * 254 = 508 elements
  TFXP_MP rowBuf[512];

  // Output row buffer for burst writes
  // Max output width = 127
  TFXP_MP outBuf[128];

  // Handle odd dimensions
  uint32_t outWidth = ((width % 2) == 0) ? width : width - 1;
  uint32_t outHeight = ((height % 2) == 0) ? height : height - 1;
  uint32_t outW2 = outWidth / 2;

  for (uint32_t ch = 0; ch < channels; ++ch) {
    uint32_t chOffset = ch * width * height;
    uint32_t outChOffset = ch * outW2 * (outHeight / 2);

    for (uint32_t row = 0; row < outHeight; row += 2) {
      // Single burst read of both rows (contiguous in CHW layout)
      uint32_t srcBase = chOffset + row * width;
      for (uint32_t i = 0; i < 2 * width; ++i) {
#pragma HLS PIPELINE II=1
        rowBuf[i] = *(input + srcBase + i);
      }

      // Compute 2x2 max pooling — row0 at rowBuf[0..width-1], row1 at rowBuf[width..2*width-1]
      for (uint32_t col = 0; col < outW2; ++col) {
#pragma HLS PIPELINE II=1
        uint32_t c2 = col * 2;
        TFXP_MP a = rowBuf[c2];
        TFXP_MP b = rowBuf[c2 + 1];
        TFXP_MP c = rowBuf[width + c2];
        TFXP_MP d = rowBuf[width + c2 + 1];

        TFXP_MP max01 = (a > b) ? a : b;
        TFXP_MP max23 = (c > d) ? c : d;
        TFXP_MP maxVal = (max01 > max23) ? max01 : max23;

        outBuf[col] = maxVal;
      }

      // Burst-write output row
      uint32_t dstBase = outChOffset + (row / 2) * outW2;
      for (uint32_t i = 0; i < outW2; ++i) {
#pragma HLS PIPELINE II=1
        *(output + dstBase + i) = outBuf[i];
      }
    }
  }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

#include "conv2d.h"

static TFXP_MULT FXP_Mult(TFXP a, TFXP b, uint32_t decimalBits = DECIMALS)
{
  // We need a wider data type to correctly capture the result of the multiplication.
  TFXP_MULT res = (TFXP_MULT)a * (TFXP_MULT)b;
  res = res >> decimalBits;
  return res;
}

void Conv2D_HW(TFXP *input, TFXP * output, TFXP * filters, TFXP * biases,
      uint32_t numChannels, uint32_t numFilters,
      uint32_t inputWidth, uint32_t inputHeight,
      uint32_t convWidth, uint32_t convHeight,
      uint32_t relu)
{
#pragma HLS INTERFACE m_axi port=input bundle=master1 offset=slave depth=196608 max_read_burst_length=256 num_read_outstanding=16 latency=20
#pragma HLS INTERFACE m_axi port=output bundle=master1 offset=slave depth=2064512 max_write_burst_length=256 num_write_outstanding=16 latency=20
#pragma HLS INTERFACE m_axi port=filters bundle=master2 offset=slave depth=27648 max_read_burst_length=256 num_read_outstanding=16 latency=20
#pragma HLS INTERFACE m_axi port=biases bundle=master2 offset=slave depth=256 max_read_burst_length=256 num_read_outstanding=16 latency=20
#pragma HLS INTERFACE s_axilite port=input
#pragma HLS INTERFACE s_axilite port=output
#pragma HLS INTERFACE s_axilite port=filters
#pragma HLS INTERFACE s_axilite port=biases
#pragma HLS INTERFACE s_axilite port=numChannels
#pragma HLS INTERFACE s_axilite port=numFilters
#pragma HLS INTERFACE s_axilite port=inputWidth
#pragma HLS INTERFACE s_axilite port=inputHeight
#pragma HLS INTERFACE s_axilite port=convWidth
#pragma HLS INTERFACE s_axilite port=convHeight
#pragma HLS INTERFACE s_axilite port=relu
#pragma HLS INTERFACE s_axilite port=return

  for (uint32_t iFilter = 0; iFilter < numFilters; ++ iFilter) {
    TFXP bias = biases[iFilter];

    // Cache filter coefficients for this filter into local BRAM
    TFXP localFilters[256 * 3 * 3]; // Max: 256 channels x 3x3 kernel
    for (uint32_t i = 0; i < numChannels * convHeight * convWidth; ++ i) {
#pragma HLS PIPELINE II=1
      localFilters[i] = *(filters + iFilter*numChannels*convHeight*convWidth + i);
    }

    for (uint32_t y = 0; y < (inputHeight-2); ++y) {
      for (uint32_t x = 0; x < (inputWidth-2); ++ x) {
        TFXP acc;
        acc = 0;
        for (uint32_t iChannel = 0; iChannel < numChannels; ++ iChannel) {
          for (uint32_t cy = 0; cy < convHeight; ++ cy) {
            for (uint32_t cx = 0; cx < convWidth; ++cx) {
              TFXP pixelValue, filterValue;
              filterValue = localFilters[iChannel*convHeight*convWidth + cy*convWidth + cx];
              pixelValue = *(input + iChannel*inputWidth*inputHeight + (y+cy)*inputWidth + (x+cx));
              acc += FXP_Mult(filterValue, pixelValue, DECIMALS);
            }
          }
        }
        // Add bias for this filter
        acc += bias;
        // Apply ReLU if enabled
        if (relu) {
          acc = (acc < 0) ? 0 : acc;
        }
        //output[iFilter][y][x] = acc;
        *(output + iFilter * (inputHeight-2)*(inputWidth-2) + y*(inputWidth-2) + x) = acc;
      }
    }
  }
}
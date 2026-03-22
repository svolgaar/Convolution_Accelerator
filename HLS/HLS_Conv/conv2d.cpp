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

  // Parallelization factor for output filters
  const uint32_t N_PARALLEL = 8;

  // Local BRAM buffers — separate filter array per parallel filter for simultaneous access
  TFXP localFilters[8][256 * 3 * 3]; // N_PARALLEL x (max channels x 3x3 kernel)
#pragma HLS ARRAY_PARTITION variable=localFilters complete dim=1
  TFXP rowBuffer[3][4096];           // 3 row buffers, max numChannels * inputWidth = 32*128 = 4096

  for (uint32_t iFilter = 0; iFilter < numFilters; iFilter += N_PARALLEL) {
    // Number of active filters in this group (handles case where numFilters % N_PARALLEL != 0)
    uint32_t nActive = ((iFilter + N_PARALLEL) <= numFilters) ? N_PARALLEL : (numFilters - iFilter);

    // Load biases for this filter group
    TFXP bias[8];
#pragma HLS ARRAY_PARTITION variable=bias complete
    for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
      bias[p] = (p < nActive) ? biases[iFilter + p] : (TFXP)0;
    }

    // Cache filter coefficients for N_PARALLEL filters into local BRAM
    uint32_t filterLen = numChannels * convHeight * convWidth;
    for (uint32_t p = 0; p < N_PARALLEL; ++p) {
      if (p < nActive) {
        for (uint32_t i = 0; i < filterLen; ++ i) {
#pragma HLS PIPELINE II=1
          localFilters[p][i] = *(filters + (iFilter + p)*filterLen + i);
        }
      }
    }

    // Load initial 3 rows into BRAM buffers
    for (uint32_t row = 0; row < 3; ++ row) {
#pragma HLS LOOP_FLATTEN off
      for (uint32_t ch = 0; ch < numChannels; ++ ch) {
#pragma HLS LOOP_FLATTEN off
        uint32_t srcBase = ch * inputWidth * inputHeight + row * inputWidth;
        uint32_t dstBase = ch * inputWidth;
        for (uint32_t px = 0; px < inputWidth; ++ px) {
#pragma HLS PIPELINE II=1
          rowBuffer[row][dstBase + px] = *(input + srcBase + px);
        }
      }
    }

    for (uint32_t y = 0; y < (inputHeight-2); ++y) {
      // For y > 0, load the new row (y+2) into the buffer that held the expired row (y-1)
      if (y > 0) {
        uint32_t newRow = y + 2;
        uint32_t bufIdx = newRow % 3;
        for (uint32_t ch = 0; ch < numChannels; ++ ch) {
#pragma HLS LOOP_FLATTEN off
          uint32_t srcBase = ch * inputWidth * inputHeight + newRow * inputWidth;
          uint32_t dstBase = ch * inputWidth;
          for (uint32_t px = 0; px < inputWidth; ++ px) {
#pragma HLS PIPELINE II=1
            rowBuffer[bufIdx][dstBase + px] = *(input + srcBase + px);
          }
        }
      }

      for (uint32_t x = 0; x < (inputWidth-2); ++ x) {
        // Parallel accumulators — one per output filter
        TFXP acc[8];
#pragma HLS ARRAY_PARTITION variable=acc complete
        for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
          acc[p] = 0;
        }

        for (uint32_t iChannel = 0; iChannel < numChannels; ++ iChannel) {
          for (uint32_t cy = 0; cy < convHeight; ++ cy) {
            for (uint32_t cx = 0; cx < convWidth; ++cx) {
              // Read pixel once, share across all parallel filters
              TFXP pixelValue = rowBuffer[(y + cy) % 3][iChannel * inputWidth + x + cx];
              for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
                TFXP filterValue = localFilters[p][iChannel*convHeight*convWidth + cy*convWidth + cx];
                acc[p] += FXP_Mult(filterValue, pixelValue, DECIMALS);
              }
            }
          }
        }

        // Add bias, apply ReLU, and write output for each active filter
        for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
          acc[p] += bias[p];
          if (relu) {
            acc[p] = (acc[p] < 0) ? (TFXP)0 : acc[p];
          }
          if ((iFilter + p) < numFilters) {
            *(output + (iFilter + p) * (inputHeight-2)*(inputWidth-2) + y*(inputWidth-2) + x) = acc[p];
          }
        }
      }
    }
  }
}
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
#pragma HLS INTERFACE m_axi port=input bundle=master1 offset=slave depth=196608 max_read_burst_length=256 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=output bundle=master1 offset=slave depth=2064512 max_write_burst_length=256 num_write_outstanding=16
#pragma HLS INTERFACE m_axi port=filters bundle=master2 offset=slave depth=27648 max_read_burst_length=256 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=biases bundle=master2 offset=slave depth=256 max_read_burst_length=256 num_read_outstanding=16
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
  const uint32_t N_PARALLEL = 16;

  // Local BRAM buffers — separate filter array per parallel filter for simultaneous access
  TFXP localFilters[16][256 * 3 * 3]; // N_PARALLEL x (max channels x 3x3 kernel)
#pragma HLS ARRAY_PARTITION variable=localFilters complete dim=1
  // 4 row buffers (instead of 3) to allow prefetch of next row without conflicting
  // with the 3 rows being read during computation. With 3 buffers, (y+3)%3 == y%3 — collision.
  // With 4 buffers, the prefetch target is always a different slot than the 3 being read.
  TFXP rowBuffer[4][4096];           // 4 row buffers, max numChannels * inputWidth = 32*128 = 4096

  // Output row buffer: accumulate a full row of outputs per parallel filter,
  // then burst-write to DDR instead of individual scattered writes per pixel.
  // Max output width = 256-2 = 254 pixels.
  TFXP outRowBuf[16][256];
#pragma HLS ARRAY_PARTITION variable=outRowBuf complete dim=1

  for (uint32_t iFilter = 0; iFilter < numFilters; iFilter += N_PARALLEL) {
    // Number of active filters in this group (handles case where numFilters % N_PARALLEL != 0)
    uint32_t nActive = ((iFilter + N_PARALLEL) <= numFilters) ? N_PARALLEL : (numFilters - iFilter);

    // Load biases for this filter group
    TFXP bias[16];
#pragma HLS ARRAY_PARTITION variable=bias complete
    for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
      bias[p] = (p < nActive) ? biases[iFilter + p] : (TFXP)0;
    }

    // Cache filter coefficients for N_PARALLEL filters into local BRAM
    // Interleaved loading: read one element per cycle, write to all 16 filter BRAMs
    // simultaneously. Eliminates 16 separate burst restarts per filter group.
    uint32_t filterLen = numChannels * convHeight * convWidth;
    for (uint32_t i = 0; i < filterLen; ++ i) {
#pragma HLS PIPELINE II=1
      for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
        if (p < nActive)
          localFilters[p][i] = *(filters + (iFilter + p)*filterLen + i);
      }
    }

    // Load initial 4 rows into BRAM buffers (3 for first computation + 1 prefetched)
    uint32_t initRows = (inputHeight < 4) ? inputHeight : 4;
    for (uint32_t row = 0; row < initRows; ++ row) {
      for (uint32_t ch = 0; ch < numChannels; ++ ch) {
        uint32_t srcBase = ch * inputWidth * inputHeight + row * inputWidth;
        uint32_t dstBase = ch * inputWidth;
        for (uint32_t px = 0; px < inputWidth; ++ px) {
#pragma HLS PIPELINE II=1
          rowBuffer[row][dstBase + px] = *(input + srcBase + px);
        }
      }
    }

    uint32_t outWidth = inputWidth - 2;
    uint32_t outHeight = inputHeight - 2;

    for (uint32_t y = 0; y < outHeight; ++y) {
      // Prefetch row y+3 into buffer (y+3)%4 while computing with rows y, y+1, y+2
      if (y > 0 && (y + 3) < inputHeight) {
        uint32_t prefetchRow = y + 3;
        uint32_t bufIdx = prefetchRow % 4;
        for (uint32_t ch = 0; ch < numChannels; ++ ch) {
          uint32_t srcBase = ch * inputWidth * inputHeight + prefetchRow * inputWidth;
          uint32_t dstBase = ch * inputWidth;
          for (uint32_t px = 0; px < inputWidth; ++ px) {
#pragma HLS PIPELINE II=1
            rowBuffer[bufIdx][dstBase + px] = *(input + srcBase + px);
          }
        }
      }

      // Compute full output row into local buffer
      for (uint32_t x = 0; x < outWidth; ++ x) {
        // Parallel accumulators — one per output filter
        TFXP acc[16];
#pragma HLS ARRAY_PARTITION variable=acc complete
        for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
          acc[p] = 0;
        }

        for (uint32_t iChannel = 0; iChannel < numChannels; ++ iChannel) {
          for (uint32_t cy = 0; cy < convHeight; ++ cy) {
            for (uint32_t cx = 0; cx < convWidth; ++cx) {
              // Read pixel once, share across all parallel filters
              TFXP pixelValue = rowBuffer[(y + cy) % 4][iChannel * inputWidth + x + cx];
              for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
                TFXP filterValue = localFilters[p][iChannel*convHeight*convWidth + cy*convWidth + cx];
                acc[p] += FXP_Mult(filterValue, pixelValue, DECIMALS);
              }
            }
          }
        }

        // Add bias, apply ReLU, store to output row buffer
        for (uint32_t p = 0; p < N_PARALLEL; ++p) {
#pragma HLS UNROLL
          acc[p] += bias[p];
          if (relu) {
            acc[p] = (acc[p] < 0) ? (TFXP)0 : acc[p];
          }
          outRowBuf[p][x] = acc[p];
        }
      }

      // Burst-write each filter's output row from local buffer to DDR
      // Each filter's row is consecutive in memory, enabling efficient burst transfers.
      // Write all N_PARALLEL slots unconditionally — unused padding outputs are never read.
      for (uint32_t p = 0; p < nActive; ++p) {
        uint32_t dstBase = (iFilter + p) * outHeight * outWidth + y * outWidth;
        for (uint32_t x = 0; x < outWidth; ++x) {
#pragma HLS PIPELINE II=1
          *(output + dstBase + x) = outRowBuf[p][x];
        }
      }
    }
  }
}

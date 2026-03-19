#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

#include "conv2d.h"

#define MAX_WIDTH 128
#define MAX_HEIGHT 128
#define MAX_CHANNELS 256
#define MAX_FILTERS 256

TFXP input[MAX_WIDTH * MAX_HEIGHT * MAX_CHANNELS];
TFXP coeffs[MAX_CHANNELS * MAX_FILTERS * 9];
TFXP outputSW[MAX_WIDTH * MAX_HEIGHT * MAX_FILTERS];
TFXP outputHW[MAX_WIDTH * MAX_HEIGHT * MAX_FILTERS];

///////////////////////////////////////////////////////////////////////////////
inline TFXP FXP_Mult(TFXP a, TFXP b, uint32_t decimalBits = DECIMALS)
{
  //return a*b;
  // We need a wider data type to correctly capture the result of the multiplication.
  TFXP_MULT res = (TFXP_MULT)a * (TFXP_MULT)b;
  res = res >> decimalBits;
  return res;
}

///////////////////////////////////////////////////////////////////////////////
// This is a convolution for [width, height, channels] --> [width-2, height-2, filters]
// There is no padding
// Stride is fixed to 1x1
// Iterates first to produce each of the output filters, which are computed
// of each other. Then, traverses the output image (for that filter), computing each output
// pixel. For every output pixel, convolves the corresponding window on the input image for 
// each input channel, using the coefficients for each channel.
void Conv2D_SW(TFXP *input, TFXP * output, TFXP * coeffs,
      uint32_t numChannels, uint32_t numFilters,
      uint32_t inputWidth, uint32_t inputHeight,
      uint32_t convWidth = 3, uint32_t convHeight = 3)
{
  /** Copy here the code of the application whose functionality we want to match @todo */
  for (uint32_t iFilter = 0; iFilter < numFilters; ++ iFilter) {
    for (uint32_t y = 0; y < (inputHeight-2); ++y) {
      for (uint32_t x = 0; x < (inputWidth-2); ++ x) {
        TFXP acc;
        acc = 0;
        for (uint32_t iChannel = 0; iChannel < numChannels; ++ iChannel) {
          for (uint32_t cy = 0; cy < convHeight; ++ cy) {
            for (uint32_t cx = 0; cx < convWidth; ++cx) {
              //acc += filters[iFilter][iChannel][cy][cx] * input[iChannel][y+cy][x+cx];
              TFXP v, f;
              f = *(coeffs + iFilter*numChannels*convHeight*convWidth + iChannel*convHeight*convWidth + cy*convWidth + cx);
              v = *(input + iChannel*inputWidth*inputHeight + (y+cy)*inputWidth + (x+cx));
              acc += FXP_Mult(f, v, DECIMALS);
            }
          }
        }
        //output[iFilter][y][x] = acc;
        *(output + iFilter * (inputHeight-2)*(inputWidth-2) + y*(inputWidth-2) + x) = acc;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void InitVectors(TFXP * input, uint32_t sizeInput, TFXP * coeffs, uint32_t sizeCoeffs)
{
  for (uint32_t ii = 0; ii < sizeInput; ++ ii)
    input[ii] = (rand() % 2) ? rand() : -rand();
  for (uint32_t ii = 0; ii < sizeCoeffs; ++ ii)
    coeffs[ii] = (rand() % 2) ? rand() : -rand();
}

///////////////////////////////////////////////////////////////////////////////
bool CompareVectors(TFXP * input1, TFXP * input2, uint32_t size)
{
  bool res = true;

  for (uint32_t ii = 0; res && ii < size; ++ ii) {
    res = (input1[ii] == input2[ii]);
  }

  return res;
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char ** argv)
{
  uint32_t width = MAX_WIDTH, height = MAX_HEIGHT;
  uint32_t channels, filters;
  uint32_t currentOutputSize;
  uint32_t sizes[][2] = { {3, 32}, {16, 16}, {32, 32}, {64, 64}, {128, 128}, {256, 256} };
  uint32_t numSizes = sizeof(sizes) / (sizeof(uint32_t) * 2);
  bool errors = false;

  srand(time(NULL));
  InitVectors(input, MAX_WIDTH * MAX_HEIGHT * MAX_CHANNELS, coeffs, MAX_CHANNELS * MAX_FILTERS * 9);

  for (uint32_t iTest = 0; iTest < numSizes; ++ iTest) {
    channels = sizes[iTest][0]; filters = sizes[iTest][1]; 
    currentOutputSize = (MAX_WIDTH-2) * (MAX_HEIGHT-2) * filters;
    printf("Evaluating execution for %" PRIu32 " --> %" PRIu32 "\n", channels, filters);
    memset(outputSW, 0, currentOutputSize * sizeof(TFXP));
    memset(outputHW, 0, currentOutputSize * sizeof(TFXP));
    printf("  SW\n");
    Conv2D_SW(input, outputSW, coeffs, channels, filters, width, height, 3, 3);
    printf("  HW\n");
    Conv2D_HW(input, outputHW, coeffs, channels, filters, width, height, 3, 3);
    if (!CompareVectors(outputSW, outputHW, currentOutputSize)) {
      printf("\n\n====== ERROR COMPARING RESULTS WITH REFERENCE!!! ======\n\n");
      errors = true;
    }
    else {
      printf("  --> OK!\n");
    }
  }
  printf("\n");

  return errors ? -1 : 0;
}



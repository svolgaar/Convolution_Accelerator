#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h> // This header file defines standard type codes for use with printf across 32/64 bit platforms

#include "util.hpp"

const uint32_t DECIMALS = 20;
typedef int32_t TFXP;     // Parameters and activations
typedef int64_t TFXP_MULT;// Intermmediate results of multiplications

#define MAX_WIDTH 128
#define MAX_HEIGHT 128
#define MAX_CHANNELS 256
#define MAX_FILTERS 256
TFXP input[MAX_WIDTH * MAX_HEIGHT * MAX_CHANNELS];
TFXP output[MAX_WIDTH * MAX_HEIGHT * MAX_FILTERS];
TFXP weights[MAX_CHANNELS * MAX_FILTERS * 9];

// Use a higher value once done debugging
#define NUM_REPES 1

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
void Conv2D(TFXP *input, TFXP * output, TFXP * filters,
      uint32_t numChannels, uint32_t numFilters,
      uint32_t inputWidth, uint32_t inputHeight,
      uint32_t convWidth = 3, uint32_t convHeight = 3)
{
  for (uint32_t iFilter = 0; iFilter < numFilters; ++ iFilter) {
    for (uint32_t y = 0; y < (inputHeight-2); ++y) {
      for (uint32_t x = 0; x < (inputWidth-2); ++ x) {
        TFXP acc;
        acc = 0;
        for (uint32_t iChannel = 0; iChannel < numChannels; ++ iChannel) {
          for (uint32_t cy = 0; cy < convHeight; ++ cy) {
            for (uint32_t cx = 0; cx < convWidth; ++cx) {
              //acc += filters[iFilter][iChannel][cy][cx] * input[iChannel][y+cy][x+cx];
              TFXP pixelValue, filterValue;
              filterValue = *(filters + iFilter*numChannels*convHeight*convWidth + iChannel*convHeight*convWidth + cy*convWidth + cx);
              pixelValue = *(input + iChannel*inputWidth*inputHeight + (y+cy)*inputWidth + (x+cx));
              acc += FXP_Mult(filterValue, pixelValue, DECIMALS);
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
void InitVectors(TFXP * input, uint32_t sizeInput, TFXP * weights, uint32_t sizeWeights)
{
  for (uint32_t ii = 0; ii < sizeInput; ++ ii)
    input[ii] = rand();
  for (uint32_t ii = 0; ii < sizeWeights; ++ ii)
    weights[ii] = rand();
}

///////////////////////////////////////////////////////////////////////////////
bool CompareVectors(TFXP * input1, TFXP * input2, uint32_t size)
{
  bool res = true;

  for (uint32_t ii = 0; res && ii < size; ++ ii)
    res = (input1[ii] == input2[ii]);

  return res;
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char ** argv)
{
  struct timespec start, end;
  uint64_t elapsedTimeSW;
  uint32_t width = MAX_WIDTH, height = MAX_HEIGHT;
  uint32_t channels, filters;
  uint32_t currentOutputSize;
  uint32_t sizes[][2] = { {3, 32}, {16, 16}, {32, 32}, {64, 64}, {128, 128}, {256, 256} };
  uint32_t numSizes = sizeof(sizes) / (sizeof(uint32_t) * 2);

  srand(time(NULL));
  InitVectors(input, MAX_WIDTH * MAX_HEIGHT * MAX_CHANNELS, weights, MAX_CHANNELS * MAX_FILTERS * 9);

  for (uint32_t iTest = 0; iTest < numSizes; ++ iTest) {
    channels = sizes[iTest][0]; filters = sizes[iTest][1]; 
    currentOutputSize = MAX_WIDTH * MAX_HEIGHT * filters;
    printf("Measuring SW time for %" PRIu32 " --> %" PRIu32 " ", channels, filters);
    fflush(stdout);
    elapsedTimeSW = 0;
    for (uint32_t repe = 0; repe < NUM_REPES; ++ repe) {
      printf("%s", repe % 2 == 0 ? "." : "*"); fflush(stdout);
      memset(output, 0, currentOutputSize * sizeof(TFXP));
      clock_gettime(CLOCK_MONOTONIC_RAW, &start);
      Conv2D(input, output, weights, channels, filters, width, height, 3, 3);
      clock_gettime(CLOCK_MONOTONIC_RAW, &end);
      elapsedTimeSW += CalcTimeDiff(end, start);
    }
    printf("\r(SW) Image size: [%" PRIu32 " x %" PRIu32 "], Channels=%" PRIu32 ", Filters=%" PRIu32 " --> %0.3lf s (%" PRIu64 " ns)\n", width, height, channels, filters,
      (elapsedTimeSW/1e9)/NUM_REPES, elapsedTimeSW/NUM_REPES);
  }
  printf("\n");

  return 0;
}



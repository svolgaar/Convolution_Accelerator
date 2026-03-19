#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h> // This header file defines standard type codes for use with printf across 32/64 bit platforms
#include <map>

#include "CAccelProxy.hpp"
#include "CConv2DProxy.hpp"
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
TFXP coeffs[MAX_CHANNELS * MAX_FILTERS * 9];

// Use a higher value once done debugging
#define NUM_REPES 1

///////////////////////////////////////
// Address constants
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
  for (uint32_t iFilter = 0; iFilter < numFilters; ++ iFilter) {
    for (uint32_t y = 0; y < (inputHeight-2); ++y) {
      for (uint32_t x = 0; x < (inputWidth-2); ++ x) {
        TFXP acc;
        acc = 0;
        for (uint32_t iChannel = 0; iChannel < numChannels; ++ iChannel) {
          for (uint32_t cy = 0; cy < convHeight; ++ cy) {
            for (uint32_t cx = 0; cx < convWidth; ++cx) {
              //acc += coeffs[iFilter][iChannel][cy][cx] * input[iChannel][y+cy][x+cx];
              TFXP pixelValue, coeffValue;
              coeffValue = *(coeffs + iFilter*numChannels*convHeight*convWidth + iChannel*convHeight*convWidth + cy*convWidth + cx);
              pixelValue = *(input + iChannel*inputWidth*inputHeight + (y+cy)*inputWidth + (x+cx));
              acc += FXP_Mult(coeffValue, pixelValue, DECIMALS);
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
void InitVectors(TFXP * input, TFXP * inputHW, uint32_t sizeInput, 
                TFXP * coeffs, TFXP * coeffsHW, uint32_t sizeCoeffs)
{
  TFXP tmp;

  for (uint32_t ii = 0; ii < sizeInput; ++ ii) {
    tmp = (rand() % 2) ? rand() : -rand();
    input[ii] = tmp;
    inputHW[ii] = tmp;
  }
  for (uint32_t ii = 0; ii < sizeCoeffs; ++ ii) {
    tmp = (rand() % 2) ? rand() : -rand();
    coeffs[ii] = tmp;
    coeffsHW[ii] = tmp;
  }
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
bool InitDevice(CConv2DProxy & convolver, TFXP* &inputHW, TFXP* &outputHW, TFXP* &coeffsHW, bool log=true)
{
  printf("\n\nThis program requires that the bitstream is loaded in the FPGA.\n");
  printf("This program has to be run with sudo.\n");
  printf("Press ENTER to confirm that the bitstream is loaded (proceeding without it can crash the board).\n\n");
  getchar();

  if ( convolver.Open(CONV2D_HW_ADDR, MAP_SIZE) != CAccelProxy::OK ) {
    if (log)
      printf("Error mapping device at physical address 0x%08X\n", CONV2D_HW_ADDR);
    return false;
  }
  if (log)
    printf("Device at physical address 0x%08X successfully mapped into the application virtual address space\n\n",
            CONV2D_HW_ADDR);

  // We have to allocate DMA memory for the device. We receive addresses in the *virtual* address space of the application.
  if (log)
    printf("Allocating DMA memory...\n");
  inputHW = (TFXP *)convolver.AllocDMACompatible(MAX_WIDTH * MAX_HEIGHT * MAX_CHANNELS * sizeof(TFXP));
  outputHW = (TFXP *)convolver.AllocDMACompatible(MAX_WIDTH * MAX_HEIGHT * MAX_FILTERS * sizeof(TFXP));
  coeffsHW = (TFXP *)convolver.AllocDMACompatible(MAX_CHANNELS * MAX_FILTERS * 9 * sizeof(TFXP));
  if (log) {
    printf("DMA memory allocated.\n");
    printf("Input: Virtual address: 0x%08X (%u)\n", (uint32_t)inputHW, (uint32_t)inputHW);
    printf("Output: Virtual address: 0x%08X (%u)\n", (uint32_t)outputHW, (uint32_t)outputHW);
    printf("Coeffs: Virtual address: 0x%08X (%u)\n", (uint32_t)coeffsHW, (uint32_t)coeffsHW);
  }

  if ( (inputHW == NULL) || (outputHW == NULL) || (coeffsHW == NULL) ) {
    if (log)
      printf("Error allocating DMA memory.\n");
    return false;
  }
 
  return true;
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char ** argv)
{
  struct timespec start, end;
  uint64_t elapsedTimeSW, elapsedTimeHW;
  uint32_t width = MAX_WIDTH, height = MAX_HEIGHT;
  uint32_t channels, filters;
  uint32_t currentOutputSize;
  uint32_t sizes[][2] = { {3, 32}, {16, 16}, {32, 32}, {64, 64}, {128, 128}, {256, 256} };
  uint32_t numSizes = sizeof(sizes) / (sizeof(uint32_t) * 2);
  TFXP * inputHW, * outputHW, * coeffsHW; // We need specially-allocated arrays to share with the accelerator
  
  CConv2DProxy convolver(false); // Deactivate logging.
  if (!InitDevice(convolver, inputHW, outputHW, coeffsHW))
    return -1;

  srand(time(NULL));

  // Start the tests. The vectors we use in SW and for the accel have to have the same values
  // so that we can compare the outputs.
  // Instead than initializing all of them, we could use memcpy().
  InitVectors(input, inputHW, MAX_WIDTH * MAX_HEIGHT * MAX_CHANNELS, coeffs, coeffsHW, MAX_CHANNELS * MAX_FILTERS * 9);

  for (uint32_t iTest = 0; iTest < numSizes; ++ iTest) {
    channels = sizes[iTest][0]; filters = sizes[iTest][1]; 
    // Output convolution for 3x3 filters with no padding is 2 pixels narrower and 2 pixels shorter
    currentOutputSize = (MAX_WIDTH-2) * (MAX_HEIGHT-2) * filters;

    printf("Measuring SW time for %" PRIu32 " --> %" PRIu32 " ", channels, filters);
    fflush(stdout);
    elapsedTimeSW = 0;
    /**
     * SW execution
    */
    for (uint32_t repe = 0; repe < NUM_REPES; ++ repe) {
      printf("%s", repe % 2 == 0 ? "." : "*"); fflush(stdout);
      memset(output, 0, currentOutputSize * sizeof(TFXP));
      clock_gettime(CLOCK_MONOTONIC_RAW, &start);
      Conv2D_SW(input, output, coeffs, channels, filters, width, height, 3, 3);
      clock_gettime(CLOCK_MONOTONIC_RAW, &end);
      elapsedTimeSW += CalcTimeDiff(end, start);
    }
    printf("\r(SW) Image size: [%" PRIu32 " x %" PRIu32 "], Channels=%" PRIu32 ", Filters=%" PRIu32 " --> %0.3lf s (%" PRIu64 " ns)\n", width, height, channels, filters,
      (elapsedTimeSW/1e9)/NUM_REPES, elapsedTimeSW/NUM_REPES);

    /**
     * HW execution
    */
    printf("Measuring HW time for %" PRIu32 " --> %" PRIu32 " ", channels, filters);
    fflush(stdout);
    elapsedTimeHW = 0;
    for (uint32_t repe = 0; repe < NUM_REPES; ++ repe) {
      printf("%s", repe % 2 == 0 ? "." : "*"); fflush(stdout);
      memset(outputHW, 0, currentOutputSize * sizeof(TFXP));
      clock_gettime(CLOCK_MONOTONIC_RAW, &start);
      Conv2D_HW(inputHW, outputHW, coeffsHW, channels, filters, width, height, 3, 3);
      clock_gettime(CLOCK_MONOTONIC_RAW, &end);
      elapsedTimeHW += CalcTimeDiff(end, start);
      /** Compare the output of the accelerator with the previously generated output from the SW @todo */
      if (!CompareVectors(output, outputHW, currentOutputSize)) {
        printf("\n\n====== ERROR COMPARING RESULTS WITH REFERENCE!!! ======\n\n");
      }
    }
    printf("\r(HW) Image size: [%" PRIu32 " x %" PRIu32 "], Channels=%" PRIu32 ", Filters=%" PRIu32 " --> %0.3lf s (%" PRIu64 " ns)\n\n", width, height, channels, filters,
      (elapsedTimeHW/1e9)/NUM_REPES, elapsedTimeHW/NUM_REPES);
  }


  // Free the DMA memory. ---IMPORTANT--- DMA memory is a system-wide resource!!!!!! It's not freed automatically when the app is closed.
  convolver.FreeDMACompatible(inputHW);
  convolver.FreeDMACompatible(outputHW);
  convolver.FreeDMACompatible(coeffsHW);
  return 0;
}



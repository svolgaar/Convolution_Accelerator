#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "model.h"

const uint32_t INPUT_SIZE = (256*256*3);

TFXP * weights[NUM_LAYERS];
TFXP * biases[NUM_LAYERS];
TTimes times;

uint8_t inputImage[INPUT_SIZE];   // RGB pixel data separated in planes.

TFXP * inputImageFxp;
TFXP * buffer0;
TFXP * buffer1;


void InitTimes(TTimes & times);
void PrintTimes(TTimes & times, uint32_t numLayers);

int main(int argc, char ** argv)
{
  if (argc != 2) {
    printf("Usage: cnnSolver image.rgba.planar\n");
    return -1;
  }

  printf("[HW] Opening Conv2D accelerator at 0x%08X...\n", CONV2D_HW_ADDR);
  CConv2DProxy convolver;
  convolver.Open(CONV2D_HW_ADDR, MAP_SIZE);
  printf("[HW] Accelerator opened successfully.\n");

  printf("[HW] Opening MaxPool accelerator at 0x%08X...\n", MAXPOOL_HW_ADDR);
  CMaxPoolProxy maxpooler;
  maxpooler.Open(MAXPOOL_HW_ADDR, MAP_SIZE);
  printf("[HW] MaxPool accelerator opened successfully.\n");

  printf("[HW] Allocating DMA-compatible buffers...\n");
  inputImageFxp = (TFXP*)convolver.AllocDMACompatible(INPUT_SIZE * sizeof(TFXP));
  buffer0       = (TFXP*)convolver.AllocDMACompatible(4129024 * sizeof(TFXP));
  buffer1       = (TFXP*)convolver.AllocDMACompatible(1032256 * sizeof(TFXP));
  if (!inputImageFxp || !buffer0 || !buffer1) {
    printf("Error: failed to allocate DMA buffers (not enough CMA memory?)\n");
    return -1;
  }
  printf("[HW] DMA buffers allocated.\n");

  // Share Conv2D's DMA address mappings with MaxPool so it can translate
  // the same virtual addresses to physical addresses for its AXI ports.
  maxpooler.ShareDMAMappings(convolver);

  if (!LoadModelInFxP(weights, biases, convolver)) {
    printf("Error loading the CNN model and converting to FxP!\n");
    convolver.FreeDMACompatible(inputImageFxp);
    convolver.FreeDMACompatible(buffer0);
    convolver.FreeDMACompatible(buffer1);
    return -1;
  }

  if (!LoadImageInFxp(argv[1], inputImageFxp, inputImage, INPUT_SIZE)) {
    printf("Error loading the image file: %s\n", argv[1]);
    FreeParams(NUM_LAYERS, (void**)weights, &convolver);
    FreeParams(NUM_LAYERS, (void**)biases, &convolver);
    convolver.FreeDMACompatible(inputImageFxp);
    convolver.FreeDMACompatible(buffer0);
    convolver.FreeDMACompatible(buffer1);
    return -1;
  }

  InitTimes(times);
  printf("[HW] Running inference with Conv2D accelerator...\n");
  TFXP finalPrediction = Inference(inputImageFxp, buffer0, buffer1, weights, biases, times, convolver, maxpooler);
  printf("[HW] OUTPUT: %0.8lf --> %s\n", Fxp2Float(finalPrediction, DECIMALS),
    Fxp2Float(finalPrediction) < 0.5 ? "CAT" : "DOG");
  PrintTimes(times, NUM_LAYERS);

  FreeParams(NUM_LAYERS, (void**)weights, &convolver);
  FreeParams(NUM_LAYERS, (void**)biases, &convolver);
  convolver.FreeDMACompatible(inputImageFxp);
  convolver.FreeDMACompatible(buffer0);
  convolver.FreeDMACompatible(buffer1);

  return Fxp2Float(finalPrediction) < 0.5 ? 0 : 1;
}


void InitTimes(TTimes & times)
{
  for (uint32_t ii = 0; ii < NUM_LAYERS; ++ ii) {
    times.timeConv[ii] = 0;
    times.timeMaxPool[ii] = 0;
    times.timeDense[ii] = 0;
  }
  times.timeFlatten = 0;
  times.timeSigmoid = 0;
}

void PrintTimes(TTimes & times, uint32_t numLayers)
{
  uint64_t accConv = 0, accMaxPool = 0, accDense = 0;
  double totalTime;

  for (uint32_t ii = 0; ii < numLayers; ++ ii) {
    if (times.timeConv[ii] != 0) {
      printf("Conv %u (HW) --> %" PRIu64 " ns (%0.3lf s)\n", ii, times.timeConv[ii], times.timeConv[ii]/1e9);
      accConv += times.timeConv[ii];
    }
  }

  for (uint32_t ii = 0; ii < numLayers; ++ ii) {
    if (times.timeMaxPool[ii] != 0) {
      printf("MaxPool %u --> %" PRIu64 " ns (%0.3lf s)\n", ii, times.timeMaxPool[ii], times.timeMaxPool[ii]/1e9);
      accMaxPool += times.timeMaxPool[ii];
    }
  }

  for (uint32_t ii = 0; ii < numLayers; ++ ii) {
    if (times.timeDense[ii] != 0) {
      printf("Dense %u (SW) --> %" PRIu64 " ns (%0.3lf s)\n", ii, times.timeDense[ii], times.timeDense[ii]/1e9);
      accDense += times.timeDense[ii];
    }
  }

  totalTime = accConv + accMaxPool + accDense + times.timeFlatten + times.timeSigmoid;
  printf("Total Conv time (HW): %" PRIu64 " ns (%0.3lf s) %0.1lf %%\n", accConv, accConv/1e9, (accConv/totalTime)*100);
  printf("Total MaxPool time:   %" PRIu64 " ns (%0.3lf s) %0.1lf %%\n", accMaxPool, accMaxPool/1e9, (accMaxPool/totalTime)*100);
  printf("Total Dense time (SW):%" PRIu64 " ns (%0.3lf s) %0.1lf %%\n", accDense, accDense/1e9, (accDense/totalTime)*100);
  printf("Total Flatten time:   %" PRIu64 " ns (%0.3lf s) %0.1lf %%\n", times.timeFlatten, times.timeFlatten/1e9, (times.timeFlatten/totalTime)*100);
  printf("Total Sigmoid time:   %" PRIu64 " ns (%0.3lf s) %0.1lf %%\n", times.timeSigmoid, times.timeSigmoid/1e9, (times.timeSigmoid/totalTime)*100);
  printf("Total time:           %" PRIu64 " ns (%0.3lf s) %0.1lf %%\n", (uint64_t)totalTime, totalTime/1e9, 100.0);
}

#ifndef CONV2D_H
#define CONV2D_H

const uint32_t DECIMALS = 20;
typedef int32_t TFXP;     // Parameters and activations
typedef int64_t TFXP_MULT;// Intermmediate results of multiplications

void Conv2D_HW(TFXP *input, TFXP * output, TFXP * filters, TFXP * biases,
      uint32_t numChannels, uint32_t numFilters,
      uint32_t inputWidth, uint32_t inputHeight,
      uint32_t convWidth = 3, uint32_t convHeight = 3,
      uint32_t relu = 0);

#endif // CONV2D_H


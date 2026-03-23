#ifndef MAXPOOL_H
#define MAXPOOL_H

#include <stdint.h>

const uint32_t MP_DECIMALS = 20;
typedef int32_t TFXP_MP;

void MaxPool_HW(TFXP_MP *input, TFXP_MP *output,
    uint32_t channels, uint32_t width, uint32_t height);

#endif

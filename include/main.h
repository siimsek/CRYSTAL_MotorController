#ifndef MAIN_H
#define MAIN_H

#include "stm32f0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ADC_HandleTypeDef hadc;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif

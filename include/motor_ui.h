#ifndef MOTOR_UI_H
#define MOTOR_UI_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MotorUI_Init(void);
void MotorUI_Task(void);
void MotorUI_ButtonIRQ(uint16_t gpio_pin);
void MotorUI_ForceRedraw(void);

/* Stage 1/2 testlerinde sensor yerine deger vermek icin.
 * temperature_x10: 405 = 40.5C
 * current_x100: 150 = 1.50A = 1500mA
 */
void MotorUI_SetSimulatedValues(int16_t temperature_x10,
                                uint16_t current_x100);

/* Pompanin calisma istegi. Alarm ve sensor fail-safe bu istegi her zaman
 * gecersiz kilabilir. Varsayilan false'tur. */
void MotorUI_SetMotorRunRequest(bool run_requested);
bool MotorUI_IsMotorRunRequested(void);
bool MotorUI_IsMotorPowerPermitted(void);

int16_t MotorUI_GetTemperatureX10(void);
uint16_t MotorUI_GetCurrentX100(void);
int16_t MotorUI_GetTemperatureSetX10(void);
uint16_t MotorUI_GetCurrentSetX100(void);

#ifdef __cplusplus
}
#endif

#endif

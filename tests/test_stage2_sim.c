#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Include the production unit at Stage 2 so the host test can verify the
 * simulated-value display and alarm-transition contract documented in
 * motor_ui.h ("Stage 1/2 testlerinde sensor yerine deger vermek icin"). */
#include "motor_ui.h"
#include "u8g2.h"

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
ADC_TypeDef test_adc_instance;
ADC_HandleTypeDef hadc = { .Instance = &test_adc_instance };
GPIO_TypeDef gpio_a;
GPIO_TypeDef gpio_b;
GPIO_TypeDef gpio_c;
uint32_t SystemCoreClock;

const uint8_t u8g2_font_6x10_tf[] = {0};
const uint8_t u8g2_font_9x15_tf[] = {0};

static uint32_t test_tick;

uint32_t HAL_GetTick(void) { return test_tick; }
void HAL_Delay(uint32_t ms) { test_tick += ms; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin) { (void)p; (void)pin; return GPIO_PIN_RESET; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState level) { (void)p; (void)pin; (void)level; }

void u8g2_Setup_ssd1306_i2c_128x64_noname_1(u8g2_t *u8g2, int mode,
                                             uint8_t (*byte_cb)(u8x8_t *, uint8_t, uint8_t, void *),
                                             uint8_t (*gpio_cb)(u8x8_t *, uint8_t, uint8_t, void *))
{ (void)u8g2; (void)mode; (void)byte_cb; (void)gpio_cb; }
void u8g2_Setup_ssd1306_i2c_128x64_noname_f(u8g2_t *u8g2, int mode,
                                             uint8_t (*byte_cb)(u8x8_t *, uint8_t, uint8_t, void *),
                                             uint8_t (*gpio_cb)(u8x8_t *, uint8_t, uint8_t, void *))
{ (void)u8g2; (void)mode; (void)byte_cb; (void)gpio_cb; }
void u8g2_SetI2CAddress(u8g2_t *u8g2, uint8_t address) { (void)u8g2; (void)address; }
void u8g2_InitDisplay(u8g2_t *u8g2) { (void)u8g2; }
void u8g2_SetPowerSave(u8g2_t *u8g2, uint8_t save) { (void)u8g2; (void)save; }
void u8g2_SetFontMode(u8g2_t *u8g2, uint8_t mode) { (void)u8g2; (void)mode; }
void u8g2_SetBitmapMode(u8g2_t *u8g2, uint8_t mode) { (void)u8g2; (void)mode; }
void u8g2_SetContrast(u8g2_t *u8g2, uint8_t contrast) { (void)u8g2; (void)contrast; }
void u8g2_FirstPage(u8g2_t *u8g2) { (void)u8g2; }
uint8_t u8g2_NextPage(u8g2_t *u8g2) { (void)u8g2; return 0U; }
void u8g2_ClearBuffer(u8g2_t *u8g2) { (void)u8g2; }
void u8g2_SendBuffer(u8g2_t *u8g2) { (void)u8g2; }
void u8g2_SetFont(u8g2_t *u8g2, const uint8_t *font) { (void)u8g2; (void)font; }
void u8g2_DrawUTF8(u8g2_t *u8g2, uint8_t x, uint8_t y, const char *s) { (void)u8g2; (void)x; (void)y; (void)s; }
void u8g2_DrawStr(u8g2_t *u8g2, uint8_t x, uint8_t y, const char *s) { (void)u8g2; (void)x; (void)y; (void)s; }
void u8g2_DrawLine(u8g2_t *u8g2, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) { (void)u8g2; (void)x1; (void)y1; (void)x2; (void)y2; }
void u8g2_DrawPixel(u8g2_t *u8g2, uint8_t x, uint8_t y) { (void)u8g2; (void)x; (void)y; }
void u8g2_DrawXBM(u8g2_t *u8g2, uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bits) { (void)u8g2; (void)x; (void)y; (void)w; (void)h; (void)bits; }
uint8_t u8g2_GetUTF8Width(u8g2_t *u8g2, const char *s) { (void)u8g2; (void)s; return 0U; }
uint8_t u8x8_GetI2CAddress(u8x8_t *u8x8) { (void)u8x8; return 0U; }

/* u8g2 port callbacks referenced by MotorUI_Init at every stage. */
uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{ (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr; return 1U; }
uint8_t u8x8_stm32_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{ (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr; return 1U; }

#include "motor_ui.c"

int main(void)
{
    char temp_text[12];
    char current_text[12];

    MotorUI_Init();
    assert(g_screen == UI_SCREEN_SPLASH);

    /* Splash expires into MAIN. */
    test_tick = UI_SPLASH_MS + 1U;
    MotorUI_Task();
    assert(g_screen == UI_SCREEN_MAIN);

    /* Simulated values must render as numbers, never "---"/"ERROR". */
    MotorUI_SetSimulatedValues(500, 100);
    assert(MotorUI_GetTemperatureX10() == 500);
    assert(MotorUI_GetCurrentX100() == 100);
    format_temperature_main_for_display(g_display_temp_x10,
                                        temp_text, sizeof(temp_text), false);
    format_current_main_for_display(g_display_current_x100,
                                    current_text, sizeof(current_text), false);
    assert(strcmp(temp_text, "50.0 C") == 0);
    assert(strcmp(current_text, "1.00 A") == 0);

    /* Armed simulated over-temperature trips the alarm (dwell + force). */
    test_tick = ALERT_UI_ARM_MS + 1U;
    MotorUI_SetSimulatedValues(900, 100);
    test_tick += ALERT_UI_ENTER_MS + 1U;
    MotorUI_Task();
    assert(g_alarm_type == ALARM_TEMPERATURE);
    assert(g_alert_forced);
    assert(g_screen == UI_SCREEN_TEMP_ALERT);

    /* Clearing the condition returns the UI to MAIN. */
    MotorUI_SetSimulatedValues(500, 100);
    MotorUI_Task();
    assert(g_alarm_type == ALARM_NONE);
    assert(!g_alert_forced);
    assert(g_screen == UI_SCREEN_MAIN);

    printf("Stage 1/2 simule deger/alarm gecis testi: OK\n");
    return 0;
}

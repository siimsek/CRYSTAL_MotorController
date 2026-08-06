#include "motor_ui_config.h"
#include "u8g2.h"

/* OLED yalnızca I2C2 (hi2c2) üzerinde. EEPROM I2C1'i buraya bağlanmaz. */
extern I2C_HandleTypeDef MOTOR_UI_OLED_I2C_HANDLE;

static void delay_us_approx(uint32_t us)
{
    volatile uint32_t loops = (SystemCoreClock / 8000000U) * us;
    while (loops-- > 0U) {
        __NOP();
    }
}

uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8,
                               uint8_t msg,
                               uint8_t arg_int,
                               void *arg_ptr)
{
    /* U8g2 bir START/END transferi arasinda en fazla 32 byte kullanir. */
    static uint8_t transfer_buffer[32];
    static uint8_t transfer_length = 0U;
    const uint8_t *source;

    switch (msg) {
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
        return 1U;

    case U8X8_MSG_BYTE_START_TRANSFER:
        transfer_length = 0U;
        return 1U;

    case U8X8_MSG_BYTE_SEND:
        source = (const uint8_t *)arg_ptr;
        while (arg_int > 0U) {
            if (transfer_length >= sizeof(transfer_buffer)) {
                return 0U;
            }
            transfer_buffer[transfer_length++] = *source++;
            --arg_int;
        }
        return 1U;

    case U8X8_MSG_BYTE_END_TRANSFER:
        return (HAL_I2C_Master_Transmit(&MOTOR_UI_OLED_I2C_HANDLE,
                                        u8x8_GetI2CAddress(u8x8),
                                        transfer_buffer,
                                        transfer_length,
                                        100U) == HAL_OK) ? 1U : 0U;

    default:
        return 0U;
    }
}

uint8_t u8x8_stm32_gpio_and_delay(u8x8_t *u8x8,
                                  uint8_t msg,
                                  uint8_t arg_int,
                                  void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;

    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        return 1U;

    case U8X8_MSG_DELAY_MILLI:
        HAL_Delay(arg_int);
        return 1U;

    case U8X8_MSG_DELAY_10MICRO:
        delay_us_approx((uint32_t)arg_int * 10U);
        return 1U;

    case U8X8_MSG_DELAY_100NANO:
        __NOP();
        return 1U;

    case U8X8_MSG_DELAY_I2C:
        delay_us_approx((uint32_t)arg_int);
        return 1U;

    case U8X8_MSG_GPIO_RESET:
    case U8X8_MSG_GPIO_I2C_CLOCK:
    case U8X8_MSG_GPIO_I2C_DATA:
        return 1U;

    default:
        return 1U;
    }
}

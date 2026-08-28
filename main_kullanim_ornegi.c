/*
 * Bu dosya tam bir CubeMX main.c / stm32f0xx_it.c degildir.
 * Isaretli kisimlari CubeMX USER CODE alanlarina ekleyin.
 *
 * Projeye eklenmesi gereken kaynaklar:
 *   motor_ui.c, motor_ui.h, motor_ui_config.h, u8g2_stm32_port.c
 *
 * CubeMX birimleri:
 *   I2C1  -> EEPROM  PB6/PB7  (hi2c1)
 *   I2C2  -> OLED    PB10/PB11 (hi2c2)
 *   ADC   -> PA1 CH1 akim, PA2 CH2 sicaklik
 *   EXTI  -> PA0 EXTI0 (EXTI0_1), PA6/PA7/PC14 EXTI4_15
 *   LSE   -> KAPALI (PC14 OK butonu, PC15 kullanilmaz)
 */

/* USER CODE BEGIN Includes */
#include "motor_ui.h"
/* USER CODE END Includes */

/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init(); // EEPROM
    MX_I2C2_Init(); // OLED
#if MOTOR_UI_STAGE >= 3U
    MX_ADC_Init();
#endif

    // USER CODE BEGIN 2
    MotorUI_Init();
    /* Varsayilan: acilista guc yolu kapanir (ara kesici). Alarm kontaklari acar.
     * Yazilimsal kesme icin MotorUI_SetMotorRunRequest(false) cagirin. */
    // USER CODE END 2

    while (1)
    {
        // USER CODE BEGIN WHILE
        MotorUI_Task();
        // USER CODE END WHILE
    }
}
*/

/*
 * --- EXTI: projede TEK HAL_GPIO_EXTI_Callback ---
 *
 * Varsayilan (bu depo): MOTOR_UI_DEFINE_HAL_EXTI_CALLBACK 0'dir; tek
 * callback src/main.c icinde tanimlanir ve MotorUI_ButtonIRQ() cagirir.
 * Kendi CubeMX projenizde baska bir yerde HAL_GPIO_EXTI_Callback
 * tanimlamiyorsaniz motor_ui_config.h'de 1 yapin (motor_ui.c callback'i
 * devreye girer). Baska dosyada AYNI fonksiyonu tanimlamayin (link hatasi).
 *
 * CubeMX stm32f0xx_it.c (degistirmeyin; HAL zaten cagirir):
 *   void EXTI0_1_IRQHandler(void)  { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);  } // PA0 DOWN
 *   void EXTI4_15_IRQHandler(void) {
 *       HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);  // PA6 BOOT
 *       HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);  // PA7 UP
 *       HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14); // PC14 OK
 *   }
 * Not: HAL, pending EXTI hattina gore dogru pini cozer; CubeMX genelde
 * her hattin pinini ayri cagirir veya tek callback uretir.
 *
 * Kendi callback'iniz varsa:
 *   #define MOTOR_UI_DEFINE_HAL_EXTI_CALLBACK 0U
 * ve:
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    MotorUI_ButtonIRQ(GPIO_Pin);
}
 *
 * ISR icinde OLED / I2C / EEPROM / ADC / HAL_Delay YAPMAYIN.
 */

#ifndef MOTOR_UI_CONFIG_H
#define MOTOR_UI_CONFIG_H

#include "main.h"

/* =========================================================================
 *                         KULLANICI AYARLARI (LIMITLER)
 * =========================================================================
 * Bu bolum sistemin temel sicaklik ve akim limitlerini belirler. 
 * Kodu anlamasaniz bile sadece buradaki degerleri degistirerek motorunuzun
 * asiri akim veya asiri isinma koruma sinirlarini kolayca ayarlayabilirsiniz.
 *
 * Sicaklik x10 formatindadir: 400 = 40.0C
 * Akim x100 formatindadir: 150 = 1.50A = 1500mA
 * ------------------------------------------------------------------------- */
#define TEMP_MIN_X10                                355      /* 35.5C */
#define TEMP_MAX_X10                                1065     /* 106.5C */
#define TEMP_DEFAULT_X10                            710      /* 71.0C */
#define TEMP_FINE_STEP_X10                          10       /* 1.0C */
#define TEMP_ALERT_HYST_X10                         10       /* 1.0C */

#define CURRENT_MIN_X100                            78U      /* 780mA (0.78A) */
#define CURRENT_MAX_X100                            233U     /* 2330mA (2.33A) */
#define CURRENT_DEFAULT_X100                        155U     /* 1550mA */
#define CURRENT_FINE_STEP_X100                      1U       /* 10mA */
#define CURRENT_ALERT_HYST_X100                     10U      /* 100mA */

/*

 * CALISMA ASAMASI
 * 1: Yalniz OLED ve sabit/simule degerler
 * 2: OLED + 4 buton + menu + ayar/onay akisi
 * 3: Stage 2 + NTC + ACS712 + EEPROM + buzzer
 * 4: Stage 3 + pompa guc kesme rolesi
 *
 * Ilk fiziksel testte 1 kullanin. Role ve motor bagliyken dogrudan 4 ile
 * baslamayin. Ayrintili sira README_TR.md ve docs/TEST_PLANI.md icindedir.
 */
#define MOTOR_UI_STAGE                              4U

/* CubeMX tarafindan uretilen handle adlari.
 * OLED: I2C2 (PB10/PB11) — EEPROM: I2C1 (PB6/PB7). Karistirmayin. */
#define MOTOR_UI_OLED_I2C_HANDLE                    hi2c2
#define MOTOR_UI_EEPROM_I2C_HANDLE                  hi2c1
#define MOTOR_UI_ADC_HANDLE                         hadc

/* Geriye uyumluluk: eski tek-handle adi OLED busunu isaret eder. */
#define MOTOR_UI_I2C_HANDLE                         MOTOR_UI_OLED_I2C_HANDLE

/* OLED: SSD1306 128x64 I2C. */
#define OLED_I2C_ADDRESS_7BIT                       0x3CU

/* -------------------------------------------------------------------------
 * GPIO PIN ESLEMELERI (sema)
 * -------------------------------------------------------------------------
 * MotorUI yalniz MOTOR_UI_* mutlak pinleri kullanir (CubeMX label bagimsiz).
 * Boylece CubeMX'in BUZZER_Pin / RELAYTRIG_Pin gibi makrolariyla cakisma olmaz.
 *
 * PA5 = UI BOOT (gercek BOOT0 degil). Butonlar: harici 10K PD, basili = HIGH.
 * PC14 = OK: LSE/RTC kristalleri KAPALI olmalidir (HSI ile calis).
 * SWD PA13/PA14 ve USART1 PA9/PA10 kullanilmaz.
 */
#define MOTOR_UI_USE_BOOT_BUTTON                    1U
/* stm32xx_it.c veya main.c icinde kendi HAL_GPIO_EXTI_Callback'iniz
 * varsa bunu 0 yapip MotorUI_ButtonIRQ(GPIO_Pin) cagirin. */
#define MOTOR_UI_DEFINE_HAL_EXTI_CALLBACK            0U
#define MOTOR_UI_BUTTONS_ACTIVE_LOW                 0U

/* Stage'ten bagimsiz: Init'te buzzer/role/alarm/LED guvenli seviyeye cekilir.
 * CubeMX bu pinleri Output Push-Pull + dogru clock ile baslatmalidir. */
#define MOTOR_UI_SAFE_GPIO_ON_INIT                  1U

/* OK PC14 / DOWN PA0 / BOOT PA6 / UP PA7 */
#define MOTOR_UI_BTN_OK_GPIO_Port                   GPIOC
#define MOTOR_UI_BTN_OK_Pin                         GPIO_PIN_14
#define MOTOR_UI_BTN_DOWN_GPIO_Port                 GPIOA
#define MOTOR_UI_BTN_DOWN_Pin                       GPIO_PIN_0
#define MOTOR_UI_BTN_BOOT_GPIO_Port                 GPIOA
#define MOTOR_UI_BTN_BOOT_Pin                       GPIO_PIN_6
#define MOTOR_UI_BTN_UP_GPIO_Port                   GPIOA
#define MOTOR_UI_BTN_UP_Pin                       GPIO_PIN_7

/* Cikislar: ULN2003 / 2N7002 */
#define MOTOR_UI_BUZZER_GPIO_Port                   GPIOA
#define MOTOR_UI_BUZZER_Pin                         GPIO_PIN_12
#define MOTOR_UI_RELAY_GPIO_Port                    GPIOB
#define MOTOR_UI_RELAY_Pin                          GPIO_PIN_12
#define MOTOR_UI_OVER_TEMP_GPIO_Port                GPIOA
#define MOTOR_UI_OVER_TEMP_Pin                      GPIO_PIN_8
#define MOTOR_UI_OVER_CURRENT_GPIO_Port             GPIOB
#define MOTOR_UI_OVER_CURRENT_Pin                   GPIO_PIN_15
#define MOTOR_UI_STATUS_LED_GPIO_Port               GPIOB
#define MOTOR_UI_STATUS_LED_Pin                     GPIO_PIN_0

/* Sadece dokumantasyon / eski isim uyumu; CubeMX ayni adi vermisse ezme. */
#ifndef BUTTON_OK_Pin
#define BUTTON_OK_GPIO_Port                         MOTOR_UI_BTN_OK_GPIO_Port
#define BUTTON_OK_Pin                               MOTOR_UI_BTN_OK_Pin
#endif
#ifndef BUTTON_DOWN_Pin
#define BUTTON_DOWN_GPIO_Port                       MOTOR_UI_BTN_DOWN_GPIO_Port
#define BUTTON_DOWN_Pin                             MOTOR_UI_BTN_DOWN_Pin
#endif
#ifndef BUTTON_BOOT_Pin
#define BUTTON_BOOT_GPIO_Port                       MOTOR_UI_BTN_BOOT_GPIO_Port
#define BUTTON_BOOT_Pin                             MOTOR_UI_BTN_BOOT_Pin
#endif
#ifndef BUTTON_UP_Pin
#define BUTTON_UP_GPIO_Port                         MOTOR_UI_BTN_UP_GPIO_Port
#define BUTTON_UP_Pin                               MOTOR_UI_BTN_UP_Pin
#endif
#ifndef BUZZER_Pin
#define BUZZER_GPIO_Port                            MOTOR_UI_BUZZER_GPIO_Port
#define BUZZER_Pin                                  MOTOR_UI_BUZZER_Pin
#endif
#ifndef RELAY_TRIG_Pin
#define RELAY_TRIG_GPIO_Port                        MOTOR_UI_RELAY_GPIO_Port
#define RELAY_TRIG_Pin                              MOTOR_UI_RELAY_Pin
#endif
#ifndef OVER_TEMP_Pin
#define OVER_TEMP_GPIO_Port                         MOTOR_UI_OVER_TEMP_GPIO_Port
#define OVER_TEMP_Pin                               MOTOR_UI_OVER_TEMP_Pin
#endif
#ifndef OVER_CURRENT_Pin
#define OVER_CURRENT_GPIO_Port                      MOTOR_UI_OVER_CURRENT_GPIO_Port
#define OVER_CURRENT_Pin                            MOTOR_UI_OVER_CURRENT_Pin
#endif
#ifndef STATUS_LED_Pin
#define STATUS_LED_GPIO_Port                        MOTOR_UI_STATUS_LED_GPIO_Port
#define STATUS_LED_Pin                              MOTOR_UI_STATUS_LED_Pin
#endif

/* GPIO aktif seviyeleri (ULN2003A / 2N7002: MCU HIGH = kanal aktif). */
#define BUZZER_ACTIVE_LEVEL                         GPIO_PIN_SET
#define BUZZER_ACTIVE_STATE                         BUZZER_ACTIVE_LEVEL

#define OVER_TEMP_ACTIVE_LEVEL                      GPIO_PIN_SET
#define OVER_CURRENT_ACTIVE_LEVEL                   GPIO_PIN_SET
#define STATUS_LED_ACTIVE_LEVEL                     GPIO_PIN_SET

/*
 * Role bobini: PB12 HIGH -> ULN2003 aktif -> Omron G2RL-2 (DC12) bobin enerjili.
 * Kontak tarafinda motor NO mu NC mi bagli oldugu olcumle dogrulanmalidir.
 * Asagidaki MOTOR_POWER_* seviyeleri olcum sonucuna gore kolayca degistirilir.
 */
#define RELAY_COIL_ACTIVE_LEVEL                     GPIO_PIN_SET
#define RELAY_COIL_INACTIVE_LEVEL                   GPIO_PIN_RESET
#define RELAY_DRIVER_ACTIVE_STATE                   RELAY_COIL_ACTIVE_LEVEL

/* Varsayilan: bobin enerjili = motor gucu izinli (tipik NO fail-safe).
 * Motor NC uzerinden besleniyorsa bu iki makroyu birbiriyle degistirin. */
#define MOTOR_POWER_ALLOW_RELAY_LEVEL               RELAY_COIL_ACTIVE_LEVEL
#define MOTOR_POWER_CUT_RELAY_LEVEL                 RELAY_COIL_INACTIVE_LEVEL

/* Dokumantasyon / eski ad: bobin enerjiliyken motor izni varsayimi. */
#define RELAY_ENERGIZED_FOR_MOTOR_RUN               1U

/* Motor kendiliginden baslamasin. Uygulama MotorUI_SetMotorRunRequest(true)
 * cagirmadan role motor izni vermez. */
#define MOTOR_RUN_REQUEST_DEFAULT                   0U
#define RELAY_SAFE_STARTUP_MS                       1000U
#define RELAY_CHATTER_GUARD_MS                      3000U    /* 3s: Role titremesi ve kontak yanmasi koruma suresi */
#define RELAY_REQUIRE_VALID_SENSORS                 1U

/* -------------------------------------------------------------------------
 * BUTON ZAMANLAMALARI
 * ------------------------------------------------------------------------- */
#define BUTTON_DEBOUNCE_MS                          35U
#define BUTTON_HOLD_START_MS                        500U
/* OK/BOOT/menu: action sonrasi OK/BOOT birakilana + sure dolana kadar kilit.
 * Set ekraninda UP/DOWN deger ayari kilit disidir ve hold ile hizlanir. */
#define BUTTON_ACTION_LOCK_MS                       180U

/* Basili tutma 1-2-5-10-20-50 carpan dizisi ile hizlanir. */
#define BUTTON_HOLD_LEVEL_1_MS                      1200U
#define BUTTON_HOLD_LEVEL_2_MS                      2400U
#define BUTTON_HOLD_LEVEL_3_MS                      4000U
#define BUTTON_HOLD_LEVEL_4_MS                      6500U
#define BUTTON_HOLD_LEVEL_5_MS                      9000U

#define BUTTON_REPEAT_LEVEL_0_MS                    220U
#define BUTTON_REPEAT_LEVEL_1_MS                    180U
#define BUTTON_REPEAT_LEVEL_2_MS                    140U
#define BUTTON_REPEAT_LEVEL_3_MS                    110U
#define BUTTON_REPEAT_LEVEL_4_MS                    90U
#define BUTTON_REPEAT_LEVEL_5_MS                    75U

/* -------------------------------------------------------------------------
 * EKRAN VE SENSOR ZAMANLAMALARI
 * ------------------------------------------------------------------------- */
#define SENSOR_UPDATE_MS                            200U     /* 200ms: guvenlik icin hizli okuma */
#define DISPLAY_VALUE_UPDATE_MS                     1000U    /* 1s: ekran yenileme (kalibrasyon modu) */
/* Boot splash then main; alerts stay off until splash + short settle. */
#define UI_SPLASH_MS                                3000U
#define ALERT_UI_ARM_MS                             5000U    /* 5s: Guc verildikten sonra 5s boyunca uyari ve guc kesme kapali */
#define ALERT_UI_ENTER_MS                           250U
#define ALERT_BLINK_MS                              350U
#define EDIT_VALUE_BLINK_MS                         300U
#define OLED_AUTO_DIM_TIMEOUT_MS                    300000U  /* 5dk tus aktivitesi yoksa %5 parlakliga dus */
#define OLED_CONTRAST_HIGH                          255U     /* %100 parlaklik (Normal mod) */
#define OLED_CONTRAST_DIM                           13U      /* %5 parlaklik (Dimmer / Burn-in koruma modu) */
#define FACTORY_RESET_HOLD_MS                       10000U   /* BOOT + OK 10s basili tutulunca Fabrika Ayarlarina Don */



/* -------------------------------------------------------------------------
 * BUZZER PATERN ZAMANLARI
 * -------------------------------------------------------------------------
 * Sicaklik : _______________ (Surekli ton)
 * Akim     : ____ ____ ____  (Kesikli ton: 350ms ses / 350ms sessizlik)
 * Ikisi    : Ilk tetiklenip gucu kesen alarm paternine devam edilir.
 */
#define BUZZER_SHORT_ON_MS                          350U
#define BUZZER_LONG_ON_MS                           350U
#define BUZZER_SYMBOL_GAP_MS                        350U
#define BUZZER_PATTERN_GAP_MS                       350U
#define ALARM_MUTE_HOLD_MS                          5000U

/* -------------------------------------------------------------------------
 * ADC
 * ------------------------------------------------------------------------- */
#define ADC_REFERENCE_MV                            3300.0f
#define ADC_FULL_SCALE                              4095.0f
#define ADC_AVERAGE_SAMPLES                         8U       /* 32 -> 8: ADC bloklama suresi 640ms -> ~160ms */
#define ADC_SAMPLE_TIME                             ADC_SAMPLETIME_239CYCLES_5

/* PA1 = ACS712 (ADC_CHANNEL_1), PA2 = NTC (ADC_CHANNEL_2). */
#define CURRENT_ADC_CHANNEL                         ADC_CHANNEL_1
#define TEMPERATURE_ADC_CHANNEL                     ADC_CHANNEL_2
#define ACS_ADC_CHANNEL                             CURRENT_ADC_CHANNEL
#define NTC_ADC_CHANNEL                             TEMPERATURE_ADC_CHANNEL

/* 100k NTC varsayimi:
 * 3.3V --- 100k sabit direnc --- ADC --- 100k NTC --- GND
 * Baglanti yonu kart olcumu ile dogrulanmadan NTC_IS_CONNECTED_TO_GND
 * ters cevirilmemelidir.
 */
#define NTC_NOMINAL_OHM                             100000.0f
#define NTC_SERIES_OHM                              100000.0f
#define NTC_NOMINAL_TEMP_K                          298.15f
#define NTC_BETA                                    3950.0f
#define NTC_IS_CONNECTED_TO_GND                     0U
#define NTC_FILTER_ALPHA                            0.20f
#define NTC_PRESENT_RAW_MIN                         700U
#define NTC_PRESENT_RAW_MAX                         3900U

/* ACS712ELCTR-20A-T:
 * 5V besleme, tipik 100mV/A hassasiyet.
 * OLCUM SONUCU (2026-08-05): Kartta bolucü YOK, ACS712 VIOUT dogrudan PA1'e.
 * 0A'da PA1 = 2.675V (olculdu). Teorik 2.500V'tan +175mV ofset mevcut.
 * sensor_mV = adc_mV * 1 / 1  (bolucu yok, dogrudan)
 * Maks guvenli akim: (3300 - 2675) / 100 = 6.25A (ADC satürasyonu limiti)
 * Pompa max ayar 5A olarak tutuldugu surece ADC guvendedir.
 */
#define ACS_SENSITIVITY_MV_PER_A                    100.0f
#define ACS_SENSOR_MV_PER_ADC_MV_NUM                1.0f    /* bolucu yok */
#define ACS_SENSOR_MV_PER_ADC_MV_DEN                1.0f    /* bolucu yok */
#define ACS_AUTO_ZERO_AT_STARTUP                    1U       /* acilista PA1 ornekler, sabit 2.5V yok */
#define ACS_ZERO_SAMPLE_COUNT                       128U
/* Acilis kalibrasyonu basarisiz olursa yedek: olculmus 0A voltaji (mV). */
#define ACS_FALLBACK_ZERO_SENSOR_MV                 2675.0f  /* 0A'da PA1 olcum sonucu */
#define ACS_CURRENT_DEADBAND_X100                   80U      /* 80 = 0.80A (800mA / 0.8A alti olcumler 0A kabul edilir) */
#define ACS_CURRENT_FILTER_ALPHA                    0.20f
#define ACS_IDLE_AUTO_ZERO_TRACKING                 1U       /* Motor dururken 0A voltaj kaymasini arka planda kalibre et */
#define ACS_IDLE_AUTO_ZERO_ALPHA                    0.05f
#define DISPLAY_TEMP_HYST_X10                       2U       /* 2 = ±0.2°C gosterge titreme onleme histerezisi */
#define DISPLAY_CURRENT_HYST_X100                   2U       /* 2 = ±0.02A (20mA) gosterge titreme onleme histerezisi */

/* ACS712 baglanti kontrolu: sifir akim cikisi ADC'de orta bantta olmalidir.
 * Kart uzerindeki bolucuye gore fiziksel olcum sonrasi daraltilabilir. */
#define ACS_PRESENT_RAW_MIN                         800U
#define ACS_PRESENT_RAW_MAX                         3600U

/* Sensor gecerliligi ve fail-safe. */
#define SENSOR_VALID_REQUIRED_COUNT                 1U
#define SENSOR_INVALID_TRIP_COUNT                   2U
#define ALARM_TRIP_REQUIRED_COUNT                   1U
#define ALARM_CLEAR_REQUIRED_COUNT                  1U
#define SENSOR_FAULT_CUTS_RELAY                     1U
#define SENSOR_FAULT_USES_BOTH_PATTERN              0U

/* -------------------------------------------------------------------------
 * EEPROM (I2C1: PB6 SCL / PB7 SDA)
 * -------------------------------------------------------------------------
 * Exact EEPROM modeli bilinmedigi icin ilk kurulumda kapali birakilmistir.
 * 24C02 benzeri oldugu dogrulanirsa 1 yapin ve adres/page degerlerini kontrol
 * edin. EEPROM kapaliyken onaylanan ayarlar RAM'de calisir ancak enerji
 * kesilince kaybolur.
 */
#define EEPROM_ENABLE                               1U
#define EEPROM_I2C_ADDRESS_7BIT                     0x50U
#define EEPROM_MEMORY_ADDRESS                       0x00U
#define EEPROM_MEMORY_ADDRESS_SIZE                  I2C_MEMADD_SIZE_8BIT
#define EEPROM_PAGE_SIZE                            8U
#define EEPROM_WRITE_TIMEOUT_MS                     20U

#endif

#include "motor_ui.h"
#include "motor_ui_config.h"
#include "u8g2.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern I2C_HandleTypeDef MOTOR_UI_OLED_I2C_HANDLE;
#if MOTOR_UI_STAGE >= 3U && EEPROM_ENABLE
extern I2C_HandleTypeDef MOTOR_UI_EEPROM_I2C_HANDLE;
#endif
#if MOTOR_UI_STAGE >= 3U
extern ADC_HandleTypeDef MOTOR_UI_ADC_HANDLE;
#endif

extern uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8,
                                      uint8_t msg,
                                      uint8_t arg_int,
                                      void *arg_ptr);
extern uint8_t u8x8_stm32_gpio_and_delay(u8x8_t *u8x8,
                                         uint8_t msg,
                                         uint8_t arg_int,
                                         void *arg_ptr);

typedef enum {
    UI_SCREEN_SPLASH = 0,
    UI_SCREEN_MAIN,
    UI_SCREEN_TEMP_ALERT,
    UI_SCREEN_CURRENT_ALERT,
    UI_SCREEN_BOTH_ALERT,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_TEMP_SET,
    UI_SCREEN_CURRENT_SET,
    UI_SCREEN_DEFAULT_CONFIRM,
    UI_SCREEN_CONFIRM_1,
    UI_SCREEN_CONFIRM_2
} ui_screen_t;

typedef enum {
    PENDING_NONE = 0,
    PENDING_TEMP,
    PENDING_CURRENT,
    PENDING_DEFAULTS
} pending_change_t;

typedef enum {
    CONFIRM_YES = 0,
    CONFIRM_NO = 1
} confirm_selection_t;

typedef enum {
    ALARM_NONE = 0,
    ALARM_TEMPERATURE,
    ALARM_CURRENT,
    ALARM_BOTH,
    ALARM_SENSOR_FAULT
} alarm_type_t;

typedef enum {
    BUTTON_ID_OK = 0,
    BUTTON_ID_DOWN,
    BUTTON_ID_BOOT,
    BUTTON_ID_UP
} button_id_t;

#if MOTOR_UI_STAGE >= 2U
typedef struct {
    bool raw_pressed;
    bool stable_pressed;
    uint32_t raw_change_tick;
    uint32_t press_start_tick;
    uint32_t last_repeat_tick;
} button_state_t;
#endif

#if MOTOR_UI_STAGE >= 3U
typedef enum {
    BUZZER_PHASE_IDLE = 0,
    BUZZER_PHASE_ON,
    BUZZER_PHASE_SYMBOL_GAP,
    BUZZER_PHASE_PATTERN_GAP
} buzzer_phase_t;
#endif

#define SETTINGS_MAGIC_0       ((uint8_t)'C')
#define SETTINGS_MAGIC_1       ((uint8_t)'R')
#define SETTINGS_MAGIC_2       ((uint8_t)'Y')
#define SETTINGS_MAGIC_3       ((uint8_t)'S')
#define SETTINGS_VERSION_V2    2U
#define SETTINGS_V2_RECORD_SIZE 12U
#define SETTINGS_VERSION       3U
#define SETTINGS_RECORD_SIZE   EEPROM_SLOT_SIZE
#define SETTINGS_COMMIT_OFFSET (SETTINGS_RECORD_SIZE - 1U)

static u8g2_t g_u8g2;
static ui_screen_t g_screen = UI_SCREEN_SPLASH;
static pending_change_t g_pending_change = PENDING_NONE;
static confirm_selection_t g_confirm_selection = CONFIRM_NO;
static alarm_type_t g_alarm_type = ALARM_NONE;
static alarm_type_t g_first_cut_alarm = ALARM_NONE;

static int16_t g_measured_temp_x10 = 276;   /* ~27.6C — matches safe pot default */
static uint16_t g_measured_current_x100 = 88U; /* ~0.88A — matches safe pot default */
static int16_t g_display_temp_x10 = 276;
static uint16_t g_display_current_x100 = 88U;
static bool g_display_hysteresis_initialized = false;

static void update_display_hysteresis_values(void)
{
    int16_t temp_diff = (g_measured_temp_x10 > g_display_temp_x10)
                      ? (g_measured_temp_x10 - g_display_temp_x10)
                      : (g_display_temp_x10 - g_measured_temp_x10);

    int32_t current_diff = ((int32_t)g_measured_current_x100 > (int32_t)g_display_current_x100)
                         ? ((int32_t)g_measured_current_x100 - (int32_t)g_display_current_x100)
                         : ((int32_t)g_display_current_x100 - (int32_t)g_measured_current_x100);

    if (!g_display_hysteresis_initialized || (temp_diff >= (int16_t)DISPLAY_TEMP_HYST_X10)) {
        g_display_temp_x10 = g_measured_temp_x10;
    }

    if (!g_display_hysteresis_initialized ||
        (current_diff >= (int32_t)DISPLAY_CURRENT_HYST_X100)) {
        g_display_current_x100 = g_measured_current_x100;
    }

    g_display_hysteresis_initialized = true;
}
static int16_t g_set_temp_x10 = TEMP_DEFAULT_X10;
static uint16_t g_set_current_x100 = CURRENT_DEFAULT_X100;
static int16_t g_edit_temp_x10 = TEMP_DEFAULT_X10;
static uint16_t g_edit_current_x100 = CURRENT_DEFAULT_X100;

static uint8_t g_menu_index = 0U;
static bool g_edit_mode = false;
static bool g_temp_alarm = false;
static bool g_current_alarm = false;
static uint8_t g_temp_alarm_trip_count = 0U;
static uint8_t g_temp_alarm_clear_count = 0U;
static uint8_t g_current_alarm_trip_count = 0U;
static uint8_t g_current_alarm_clear_count = 0U;
static bool g_sensor_fault = false;
static bool g_temp_sensor_valid = false;
static bool g_current_sensor_valid = false;
static bool g_alert_forced = false;
static alarm_type_t g_ui_alarm_candidate = ALARM_NONE;
static uint32_t g_ui_alarm_candidate_tick = 0U;
static bool g_blink_on = true;
static bool g_display_dirty = true;

static bool g_motor_run_requested = (MOTOR_RUN_REQUEST_DEFAULT != 0U);
static bool g_motor_power_permitted = false;
#if MOTOR_UI_STAGE >= 3U
static bool g_sensors_valid = false;
static uint8_t g_sensor_valid_count = 0U;
static uint8_t g_sensor_invalid_count = 0U;
#endif

static uint32_t g_init_tick = 0U;
static uint32_t g_last_sensor_tick = 0U;
static uint32_t g_last_display_tick = 0U;
static uint32_t g_last_blink_tick = 0U;
static uint32_t g_last_user_activity_tick = 0U;
static bool g_oled_dimmed = false;

static void ui_reset_user_activity(uint32_t now)
{
    g_last_user_activity_tick = now;
    if (g_oled_dimmed) {
        g_oled_dimmed = false;
#if MOTOR_UI_STAGE >= 1U
        u8g2_SetContrast(&g_u8g2, OLED_CONTRAST_HIGH);
        g_display_dirty = true;
#endif
    }
}

#if MOTOR_UI_STAGE >= 2U
static button_state_t g_button_ok;
static button_state_t g_button_down;
static button_state_t g_button_up;
#if MOTOR_UI_USE_BOOT_BUTTON
static button_state_t g_button_boot;
#endif
static volatile bool g_button_irq_hint = false;
#endif

#if MOTOR_UI_STAGE >= 3U
static bool g_sensor_filter_initialized = false;
static float g_temp_filter_x10 = 250.0f;

typedef struct {
    uint16_t raw_min;
    uint16_t raw_max;
    uint16_t sample_count;
    uint16_t present_count;
    uint32_t start_tick;
    bool active;
    bool result_ready;
    bool result_valid;
    uint16_t result_x100;
} acs_pp_window_t;

static acs_pp_window_t g_acs_pp_window;

static const char g_pattern_temperature[] = "C";      /* C: Continuous tone (Surekli ses) */
static const char g_pattern_current[] = "____";       /* Intermittent beeps (Kesikli ses: 350ms acik / 350ms kapali) */
static const char g_pattern_both[] = "____";
static const char *g_buzzer_pattern = NULL;
static buzzer_phase_t g_buzzer_phase = BUZZER_PHASE_IDLE;
static uint8_t g_buzzer_symbol_index = 0U;
static uint32_t g_buzzer_deadline = 0U;
static bool g_alarm_buzzer_muted = false;
#endif

#if MOTOR_UI_STAGE >= 3U && EEPROM_ENABLE
typedef enum {
    EEPROM_SAVE_IDLE = 0,
    EEPROM_SAVE_INVALIDATE_START,
    EEPROM_SAVE_INVALIDATE_WAIT,
    EEPROM_SAVE_PAGE_START,
    EEPROM_SAVE_PAGE_WAIT,
    EEPROM_SAVE_COMMIT_START,
    EEPROM_SAVE_COMMIT_WAIT,
    EEPROM_SAVE_READY_WAIT,
    EEPROM_SAVE_RETRY_WAIT
} eeprom_save_state_t;

static uint8_t g_settings_record[SETTINGS_RECORD_SIZE];
static uint8_t g_settings_invalid_commit = 0U;
static uint16_t g_settings_save_address = EEPROM_SLOT_0_ADDRESS;
static uint8_t g_settings_page_offset = 0U;
static uint32_t g_settings_sequence = 0U;
static uint32_t g_settings_retry_tick = 0U;
static eeprom_save_state_t g_settings_ready_next_state = EEPROM_SAVE_IDLE;
static bool g_settings_save_pending = false;
static bool g_settings_i2c_done = false;
static bool g_settings_i2c_error = false;
static eeprom_save_state_t g_settings_save_state = EEPROM_SAVE_IDLE;
#endif

static void update_alert_state(void);
static void safety_update_outputs(uint32_t now);
#if MOTOR_UI_STAGE >= 2U
static void handle_button_action(button_id_t button,
                                 uint8_t acceleration_multiplier,
                                 bool is_repeat);
#endif

/* 30x33 temperature warning bitmap */
static const uint8_t tempalert_bits[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x80,0x00,0x00,0x00,0xc0,0x01,0x00,0x00,0xc0,0x01,0x00,
    0x00,0xc0,0x01,0x00,0x00,0xc0,0x0f,0x00,0x00,0xc0,0x01,0x00,
    0x00,0xc0,0x0f,0x00,0x00,0xc0,0x0f,0x00,0x00,0xc0,0x01,0x00,
    0x00,0xc0,0x0f,0x00,0x00,0xc0,0x0f,0x00,0x00,0xc0,0x01,0x00,
    0x00,0xc0,0x01,0x00,0x00,0xc0,0x01,0x00,0xc0,0xd9,0xcb,0x01,
    0xe0,0xef,0xfb,0x03,0x60,0xef,0x7b,0x03,0x00,0xc0,0x01,0x00,
    0x00,0x00,0x00,0x00,0x80,0x7f,0xef,0x00,0xc0,0xff,0xff,0x01,
    0xc0,0x8c,0x98,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

/* 24x24 current warning bitmap */
static const uint8_t currentalert_bits[] = {
    0x00,0xf8,0x03,0x00,0xf8,0x03,0x00,0xfc,0x01,0x00,0xfc,0x01,
    0x00,0xfe,0x00,0x00,0x7e,0x00,0x00,0x7f,0x00,0x00,0x3f,0x00,
    0x80,0xff,0x07,0x80,0xff,0x03,0xc0,0xff,0x01,0xc0,0xff,0x00,
    0xe0,0x7f,0x00,0x00,0x7e,0x00,0x00,0x3e,0x00,0x00,0x1e,0x00,
    0x00,0x0f,0x00,0x00,0x07,0x00,0x80,0x07,0x00,0x80,0x03,0x00,
    0xc0,0x01,0x00,0xc0,0x00,0x00,0x40,0x00,0x00,0x20,0x00,0x00
};

/* 17x7 menu arrow bitmap from OLED_Projesi.oled.json. */
static const uint8_t arrow_bits[] = {
    0x00, 0x20, 0x00,
    0x00, 0x40, 0x00,
    0x00, 0x80, 0x00,
    0xff, 0xff, 0x01,
    0x00, 0x80, 0x00,
    0x00, 0x40, 0x00,
    0x00, 0x20, 0x00
};

#define ACILIS_BITMAP_X       6U
#define ACILIS_BITMAP_Y       7U
#define ACILIS_BITMAP_WIDTH   116U
#define ACILIS_BITMAP_HEIGHT  51U

static const uint8_t acilis_bitmap_bits[] = {
    0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x1f, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff,
    0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xf0, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0xf0, 0xff, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0xc0, 0xff, 0x07,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe,
    0x00, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xe0, 0x7f, 0x00, 0xfc, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xfc, 0x07, 0x00, 0xf0, 0x7f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x00, 0xc0, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
    0x00, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xc0, 0x3f, 0x00, 0x00, 0x80, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xc0, 0x1f, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x0f, 0x00, 0x00, 0x00, 0x7f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x07, 0x00,
    0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xf8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0xfc, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xfc, 0x01, 0x00, 0xfc, 0xe3, 0x3f,
    0x1e, 0xe0, 0xf1, 0xbf, 0xff, 0x07, 0x0f, 0x38, 0x03, 0xfe, 0x00, 0x00,
    0xff, 0xe3, 0xff, 0x1e, 0xe0, 0xfd, 0xff, 0xff, 0x07, 0x1f, 0x38, 0x0a,
    0xfe, 0x00, 0x80, 0xff, 0xe3, 0xff, 0x3d, 0xf0, 0xfc, 0xdf, 0xff, 0x87,
    0x1f, 0x38, 0x00, 0x7e, 0x00, 0xc0, 0x01, 0x60, 0xe0, 0x3d, 0xf0, 0x0e,
    0x00, 0x70, 0x80, 0x1f, 0x38, 0x00, 0x7e, 0x00, 0xc0, 0x01, 0x60, 0xc0,
    0x79, 0x78, 0x0e, 0x00, 0x70, 0xc0, 0x3d, 0x38, 0x00, 0x3f, 0x00, 0xe0,
    0x01, 0x60, 0xc0, 0x71, 0x38, 0x0e, 0x00, 0x70, 0xc0, 0x39, 0x38, 0x00,
    0x3f, 0x00, 0xe0, 0x01, 0x60, 0xc0, 0xf1, 0x3c, 0x0e, 0x00, 0x70, 0xc0,
    0x39, 0x38, 0x00, 0x3f, 0x00, 0xe0, 0x01, 0x60, 0xc0, 0xe1, 0x1c, 0xfe,
    0x01, 0x70, 0xe0, 0x79, 0x38, 0x00, 0x1f, 0x00, 0xe0, 0x01, 0xe0, 0xff,
    0xc1, 0x0d, 0xf0, 0x1f, 0x70, 0xe0, 0x70, 0x38, 0x00, 0x1f, 0x00, 0xe0,
    0x01, 0xe0, 0xff, 0xc0, 0x07, 0x80, 0x3f, 0x70, 0xf0, 0xf0, 0x38, 0x00,
    0x1f, 0x01, 0xe0, 0x01, 0x60, 0x0f, 0x80, 0x07, 0x00, 0x3c, 0x70, 0x70,
    0xe0, 0x38, 0x00, 0x9f, 0x01, 0xe0, 0x01, 0x60, 0x1e, 0x00, 0x03, 0x00,
    0x38, 0x70, 0x70, 0xe0, 0x39, 0x00, 0x9e, 0x01, 0xc0, 0x01, 0x60, 0x3c,
    0x00, 0x03, 0x00, 0x38, 0x70, 0x78, 0xe0, 0x39, 0x00, 0x9e, 0x01, 0xc0,
    0x03, 0x60, 0x7c, 0x00, 0x03, 0x00, 0x38, 0x70, 0x38, 0xc0, 0x39, 0x00,
    0x9e, 0x03, 0x80, 0xff, 0x63, 0xf8, 0x00, 0x03, 0xfc, 0x1f, 0x70, 0x3c,
    0xc0, 0xfb, 0x0f, 0x9e, 0x03, 0x00, 0xff, 0x63, 0xf0, 0x01, 0x03, 0xfc,
    0x1f, 0x70, 0x3c, 0x80, 0xfb, 0x0f, 0x9c, 0x03, 0x00, 0xf0, 0x43, 0x80,
    0x03, 0x02, 0xfc, 0x00, 0x60, 0x0c, 0x00, 0xf7, 0x0f, 0x1c, 0x07, 0x00,
    0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x0f, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x38, 0x0f, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x1f, 0x00, 0x00, 0x80, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x3e, 0x00,
    0x00, 0x80, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x60, 0x7e, 0x00, 0x00, 0xc0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x40, 0xfe, 0x00, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01, 0x00, 0xf8, 0x07,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x0f,
    0xe0, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xf8, 0x1f, 0xc0, 0x07, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xf0, 0x7f, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x03, 0x80, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static int16_t clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (int16_t)value;
}

static uint16_t clamp_u16(int32_t value, uint16_t min_value, uint16_t max_value)
{
    if (value < (int32_t)min_value) {
        return min_value;
    }
    if (value > (int32_t)max_value) {
        return max_value;
    }
    return (uint16_t)value;
}

/* ========================================================================= *
 *                                                                           *
 *                          [ MODULE: DISPLAY ]                              *
 *                     (OLED Rendering & Formatters)                         *
 * ========================================================================= */

static void format_temperature(int16_t value_x10, char *buffer, size_t buffer_size)
{
    int16_t whole = (int16_t)(value_x10 / 10);
    int16_t fraction = (int16_t)(value_x10 % 10);
    if (fraction < 0) {
        fraction = (int16_t)-fraction;
    }
    (void)snprintf(buffer, buffer_size, "%02d.%1d C", whole, fraction);
}

static void format_temperature_set(int16_t value_x10,
                                   char *buffer,
                                   size_t buffer_size)
{
    int16_t whole;
    if (value_x10 >= 0) {
        whole = (int16_t)((value_x10 + 5) / 10);
    } else {
        whole = (int16_t)(-(((-value_x10) + 5) / 10));
    }
    (void)snprintf(buffer, buffer_size, "%d C", (int)whole);
}

static int16_t snap_temp_whole_x10(int16_t value_x10)
{
    if (value_x10 >= 0) {
        return (int16_t)(((value_x10 + 5) / 10) * 10);
    }
    return (int16_t)(((value_x10 - 5) / 10) * 10);
}

static void format_current(uint16_t value_x100, char *buffer, size_t buffer_size)
{
    uint16_t whole = (uint16_t)(value_x100 / 100U);
    uint16_t fraction = (uint16_t)(value_x100 % 100U);
    (void)snprintf(buffer, buffer_size, "%u.%02u A",
                   (unsigned int)whole,
                   (unsigned int)fraction);
}



static void blank_numeric_characters(char *buffer)
{
    size_t index;
    for (index = 0U; buffer[index] != '\0'; ++index) {
        if ((buffer[index] >= '0') && (buffer[index] <= '9')) {
            buffer[index] = ' ';
        }
    }
}

/* ========================================================================= *
 *                                                                           *
 *                      [ MODULE: SENSORS & EEPROM ]                         *
 *                                                                           *
 * ========================================================================= */

#if MOTOR_UI_STAGE >= 3U && EEPROM_ENABLE
static uint16_t crc16_ccitt(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t bit;

    for (i = 0U; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0U; bit < 8U; ++bit) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static bool eeprom_read_bytes(uint16_t address, uint8_t *data, uint16_t length)
{
    return HAL_I2C_Mem_Read(&MOTOR_UI_EEPROM_I2C_HANDLE,
                            (uint16_t)(EEPROM_I2C_ADDRESS_7BIT << 1),
                            address,
                            EEPROM_MEMORY_ADDRESS_SIZE,
                            data,
                            length,
                            100U) == HAL_OK;
}

static bool settings_record_v3_valid(const uint8_t *record,
                                     uint32_t *sequence,
                                     int16_t *temp_value,
                                     uint16_t *current_value)
{
    uint16_t stored_crc;

    if ((record[0] != SETTINGS_MAGIC_0) || (record[1] != SETTINGS_MAGIC_1) ||
        (record[2] != SETTINGS_MAGIC_2) || (record[3] != SETTINGS_MAGIC_3) ||
        (record[4] != SETTINGS_VERSION) ||
        (record[SETTINGS_COMMIT_OFFSET] != EEPROM_COMMIT_VALUE)) {
        return false;
    }
    stored_crc = (uint16_t)record[13] | ((uint16_t)record[14] << 8);
    if (stored_crc != crc16_ccitt(record, 13U)) {
        return false;
    }
    *temp_value = (int16_t)((uint16_t)record[9] | ((uint16_t)record[10] << 8));
    *current_value = (uint16_t)record[11] | ((uint16_t)record[12] << 8);
    if ((*temp_value < TEMP_MIN_X10) || (*temp_value > TEMP_MAX_X10) ||
        (*current_value < CURRENT_MIN_X100) || (*current_value > CURRENT_MAX_X100)) {
        return false;
    }
    *sequence = (uint32_t)record[5] | ((uint32_t)record[6] << 8) |
                ((uint32_t)record[7] << 16) | ((uint32_t)record[8] << 24);
    return true;
}

static bool settings_load_v2(void)
{
    uint8_t record[SETTINGS_V2_RECORD_SIZE];
    uint16_t stored_crc;
    int16_t temp_value;
    uint16_t current_value;

    if (!eeprom_read_bytes(EEPROM_MEMORY_ADDRESS, record, sizeof(record)) ||
        (record[0] != SETTINGS_MAGIC_0) || (record[1] != SETTINGS_MAGIC_1) ||
        (record[2] != SETTINGS_MAGIC_2) || (record[3] != SETTINGS_MAGIC_3) ||
        (record[4] != SETTINGS_VERSION_V2)) {
        return false;
    }
    stored_crc = (uint16_t)record[10] | ((uint16_t)record[11] << 8);
    if (stored_crc != crc16_ccitt(record, 10U)) {
        return false;
    }
    temp_value = (int16_t)((uint16_t)record[6] | ((uint16_t)record[7] << 8));
    current_value = (uint16_t)record[8] | ((uint16_t)record[9] << 8);
    if ((temp_value < TEMP_MIN_X10) || (temp_value > TEMP_MAX_X10) ||
        (current_value < CURRENT_MIN_X100) || (current_value > CURRENT_MAX_X100)) {
        return false;
    }
    g_set_temp_x10 = temp_value;
    g_set_current_x100 = current_value;
    return true;
}

static bool settings_load(void)
{
    uint8_t slot_0[SETTINGS_RECORD_SIZE];
    uint8_t slot_1[SETTINGS_RECORD_SIZE];
    uint32_t sequence_0;
    uint32_t sequence_1;
    int16_t temp_0;
    int16_t temp_1;
    uint16_t current_0;
    uint16_t current_1;
    bool valid_0;
    bool valid_1;

    valid_0 = eeprom_read_bytes(EEPROM_SLOT_0_ADDRESS, slot_0, sizeof(slot_0)) &&
              settings_record_v3_valid(slot_0, &sequence_0, &temp_0, &current_0);
    valid_1 = eeprom_read_bytes(EEPROM_SLOT_1_ADDRESS, slot_1, sizeof(slot_1)) &&
              settings_record_v3_valid(slot_1, &sequence_1, &temp_1, &current_1);
    if (valid_0 || valid_1) {
        if (valid_1 && (!valid_0 || ((int32_t)(sequence_1 - sequence_0) > 0))) {
            g_set_temp_x10 = temp_1;
            g_set_current_x100 = current_1;
            g_settings_sequence = sequence_1;
            g_settings_save_address = EEPROM_SLOT_0_ADDRESS;
        } else {
            g_set_temp_x10 = temp_0;
            g_set_current_x100 = current_0;
            g_settings_sequence = sequence_0;
            g_settings_save_address = EEPROM_SLOT_1_ADDRESS;
        }
        return true;
    }
    if (settings_load_v2()) {
        /* Preserve the legacy record until its v3 replacement commits. */
        g_settings_save_address = EEPROM_SLOT_1_ADDRESS;
        return true;
    }
    return false;
}

static bool settings_save(void)
{
    uint16_t crc;

    if (g_settings_save_state != EEPROM_SAVE_IDLE) {
        g_settings_save_pending = true;
        return true;
    }
    (void)memset(g_settings_record, 0, sizeof(g_settings_record));
    g_settings_sequence++;
    g_settings_record[0] = SETTINGS_MAGIC_0;
    g_settings_record[1] = SETTINGS_MAGIC_1;
    g_settings_record[2] = SETTINGS_MAGIC_2;
    g_settings_record[3] = SETTINGS_MAGIC_3;
    g_settings_record[4] = SETTINGS_VERSION;
    g_settings_record[5] = (uint8_t)(g_settings_sequence & 0xFFU);
    g_settings_record[6] = (uint8_t)((g_settings_sequence >> 8) & 0xFFU);
    g_settings_record[7] = (uint8_t)((g_settings_sequence >> 16) & 0xFFU);
    g_settings_record[8] = (uint8_t)((g_settings_sequence >> 24) & 0xFFU);
    g_settings_record[9] = (uint8_t)((uint16_t)g_set_temp_x10 & 0xFFU);
    g_settings_record[10] = (uint8_t)(((uint16_t)g_set_temp_x10 >> 8) & 0xFFU);
    g_settings_record[11] = (uint8_t)(g_set_current_x100 & 0xFFU);
    g_settings_record[12] = (uint8_t)((g_set_current_x100 >> 8) & 0xFFU);
    crc = crc16_ccitt(g_settings_record, 13U);
    g_settings_record[13] = (uint8_t)(crc & 0xFFU);
    g_settings_record[14] = (uint8_t)((crc >> 8) & 0xFFU);
    g_settings_record[SETTINGS_COMMIT_OFFSET] = EEPROM_COMMIT_VALUE;
    g_settings_page_offset = 0U;
    g_settings_save_pending = false;
    g_settings_save_state = EEPROM_SAVE_INVALIDATE_START;
    return true;
}

static void settings_save_retry(uint32_t now)
{
    g_settings_i2c_done = false;
    g_settings_i2c_error = false;
    g_settings_retry_tick = now + EEPROM_RETRY_DELAY_MS;
    g_settings_save_state = EEPROM_SAVE_RETRY_WAIT;
}

static void settings_task(uint32_t now)
{
    uint16_t address;
    uint16_t length;

    switch (g_settings_save_state) {
    case EEPROM_SAVE_IDLE:
        if (g_settings_save_pending) {
            (void)settings_save();
        }
        break;
    case EEPROM_SAVE_INVALIDATE_START:
        if (HAL_I2C_Mem_Write_IT(&MOTOR_UI_EEPROM_I2C_HANDLE,
                                 (uint16_t)(EEPROM_I2C_ADDRESS_7BIT << 1),
                                 (uint16_t)(g_settings_save_address + SETTINGS_COMMIT_OFFSET),
                                 EEPROM_MEMORY_ADDRESS_SIZE, &g_settings_invalid_commit, 1U) != HAL_OK) {
            settings_save_retry(now);
        } else {
            g_settings_i2c_done = false;
            g_settings_i2c_error = false;
            g_settings_retry_tick = now + EEPROM_WRITE_TIMEOUT_MS;
            g_settings_save_state = EEPROM_SAVE_INVALIDATE_WAIT;
        }
        break;
    case EEPROM_SAVE_INVALIDATE_WAIT:
    case EEPROM_SAVE_PAGE_WAIT:
    case EEPROM_SAVE_COMMIT_WAIT:
        if (g_settings_i2c_error) {
            settings_save_retry(now);
        } else if (!g_settings_i2c_done && ((int32_t)(now - g_settings_retry_tick) >= 0)) {
            settings_save_retry(now);
        } else if (g_settings_i2c_done) {
            if (g_settings_save_state == EEPROM_SAVE_INVALIDATE_WAIT) {
                g_settings_ready_next_state = EEPROM_SAVE_PAGE_START;
            } else if (g_settings_save_state == EEPROM_SAVE_PAGE_WAIT) {
                g_settings_page_offset = (uint8_t)(g_settings_page_offset + EEPROM_PAGE_SIZE);
                g_settings_ready_next_state = (g_settings_page_offset < SETTINGS_COMMIT_OFFSET)
                                           ? EEPROM_SAVE_PAGE_START : EEPROM_SAVE_COMMIT_START;
            } else {
                g_settings_ready_next_state = EEPROM_SAVE_IDLE;
            }
            g_settings_i2c_done = false;
            g_settings_retry_tick = now + EEPROM_WRITE_TIMEOUT_MS;
            g_settings_save_state = EEPROM_SAVE_READY_WAIT;
        }
        break;
    case EEPROM_SAVE_PAGE_START:
        address = (uint16_t)(g_settings_save_address + g_settings_page_offset);
        length = ((SETTINGS_COMMIT_OFFSET - g_settings_page_offset) < EEPROM_PAGE_SIZE)
               ? (uint16_t)(SETTINGS_COMMIT_OFFSET - g_settings_page_offset) : EEPROM_PAGE_SIZE;
        if (HAL_I2C_Mem_Write_IT(&MOTOR_UI_EEPROM_I2C_HANDLE,
                                 (uint16_t)(EEPROM_I2C_ADDRESS_7BIT << 1), address,
                                 EEPROM_MEMORY_ADDRESS_SIZE,
                                 &g_settings_record[g_settings_page_offset], length) != HAL_OK) {
            settings_save_retry(now);
        } else {
            g_settings_i2c_done = false;
            g_settings_i2c_error = false;
            g_settings_retry_tick = now + EEPROM_WRITE_TIMEOUT_MS;
            g_settings_save_state = EEPROM_SAVE_PAGE_WAIT;
        }
        break;
    case EEPROM_SAVE_COMMIT_START:
        if (HAL_I2C_Mem_Write_IT(&MOTOR_UI_EEPROM_I2C_HANDLE,
                                 (uint16_t)(EEPROM_I2C_ADDRESS_7BIT << 1),
                                 (uint16_t)(g_settings_save_address + SETTINGS_COMMIT_OFFSET),
                                 EEPROM_MEMORY_ADDRESS_SIZE,
                                 &g_settings_record[SETTINGS_COMMIT_OFFSET], 1U) != HAL_OK) {
            settings_save_retry(now);
        } else {
            g_settings_i2c_done = false;
            g_settings_i2c_error = false;
            g_settings_retry_tick = now + EEPROM_WRITE_TIMEOUT_MS;
            g_settings_save_state = EEPROM_SAVE_COMMIT_WAIT;
        }
        break;
    case EEPROM_SAVE_RETRY_WAIT:
        if ((int32_t)(now - g_settings_retry_tick) >= 0) {
            g_settings_save_state = EEPROM_SAVE_INVALIDATE_START;
        }
        break;
    case EEPROM_SAVE_READY_WAIT:
        if ((int32_t)(now - g_settings_retry_tick) >= 0) {
            if (g_settings_ready_next_state == EEPROM_SAVE_IDLE) {
                g_settings_save_address = (g_settings_save_address == EEPROM_SLOT_0_ADDRESS)
                                        ? EEPROM_SLOT_1_ADDRESS : EEPROM_SLOT_0_ADDRESS;
            }
            g_settings_save_state = g_settings_ready_next_state;
        }
        break;
    default:
        g_settings_save_state = EEPROM_SAVE_IDLE;
        break;
    }
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &MOTOR_UI_EEPROM_I2C_HANDLE) {
        g_settings_i2c_done = true;
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &MOTOR_UI_EEPROM_I2C_HANDLE) {
        g_settings_i2c_error = true;
    }
}
#endif

#if MOTOR_UI_STAGE >= 3U
static bool adc_convert_once(uint16_t *raw)
{
    if (HAL_ADC_Start(&MOTOR_UI_ADC_HANDLE) != HAL_OK) {
        return false;
    }
    if (HAL_ADC_PollForConversion(&MOTOR_UI_ADC_HANDLE, 10U) != HAL_OK) {
        (void)HAL_ADC_Stop(&MOTOR_UI_ADC_HANDLE);
        return false;
    }
    *raw = (uint16_t)HAL_ADC_GetValue(&MOTOR_UI_ADC_HANDLE);
    return true;
}

static bool adc_select_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef channel_config = {0};
    uint16_t dummy;
    uint16_t i;

    (void)HAL_ADC_Stop(&MOTOR_UI_ADC_HANDLE);
    MOTOR_UI_ADC_HANDLE.Instance->CHSELR = 0U;

    channel_config.Channel = channel;
    channel_config.Rank = ADC_RANK_CHANNEL_NUMBER;
    channel_config.SamplingTime = ADC_SAMPLE_TIME;

    if (HAL_ADC_ConfigChannel(&MOTOR_UI_ADC_HANDLE, &channel_config) != HAL_OK) {
        return false;
    }

    for (i = 0U; i < ADC_DUMMY_CONVERSIONS; ++i) {
        if (!adc_convert_once(&dummy)) {
            return false;
        }
    }
    return true;
}

static bool adc_read_average(uint32_t channel,
                             uint16_t sample_count,
                             uint16_t *result)
{
    uint32_t sum = 0U;
    uint16_t i;
    uint16_t raw;

    if (!adc_select_channel(channel)) {
        return false;
    }

    for (i = 0U; i < sample_count; ++i) {
        if (!adc_convert_once(&raw)) {
            return false;
        }
        sum += raw;
    }

    (void)HAL_ADC_Stop(&MOTOR_UI_ADC_HANDLE);
    *result = (uint16_t)(sum / sample_count);
    return true;
}

static bool acs_validate_presence(void)
{
    /* Baslangicta sadece ADC/sensor baglantisinin makul aralikta oldugunu
     * kontrol et. Akim p-p/RMS penceresinden hesaplanir; burada ofset
     * saklanmaz ve mutlak kalibrasyon varsayilmaz. */
    uint16_t raw;
    if (!adc_read_average(ACS_ADC_CHANNEL, ACS_PRESENCE_SAMPLE_COUNT, &raw)) {
        return false;
    }
    if ((raw < ACS_PRESENT_RAW_MIN) || (raw > ACS_PRESENT_RAW_MAX)) {
        return false;
    }
    return true;
}

static bool read_temperature_x10(int16_t *temperature_x10)
{
    uint16_t raw;
    float resistance;
    float inverse_temperature;
    float temperature_c;

    if (!adc_read_average(NTC_ADC_CHANNEL, ADC_AVERAGE_SAMPLES, &raw)) {
        return false;
    }

    if ((raw < NTC_PRESENT_RAW_MIN) || (raw > NTC_PRESENT_RAW_MAX)) {
        return false;
    }

#if NTC_IS_CONNECTED_TO_GND
    resistance = NTC_SERIES_OHM * ((float)raw / (ADC_FULL_SCALE - (float)raw));
#else
    resistance = NTC_SERIES_OHM * ((ADC_FULL_SCALE - (float)raw) / (float)raw);
#endif

    if (resistance <= 0.0f) {
        return false;
    }

    inverse_temperature = (1.0f / NTC_NOMINAL_TEMP_K) +
                          (logf(resistance / NTC_NOMINAL_OHM) / NTC_BETA);
    if (inverse_temperature <= 0.0f) {
        return false;
    }

    temperature_c = (1.0f / inverse_temperature) - 273.15f;
    if (!isfinite(temperature_c) ||
        (temperature_c < -50.0f) ||
        (temperature_c > 150.0f)) {
        return false;
    }

    *temperature_x10 = (int16_t)lroundf(temperature_c * 10.0f);
    return true;
}

static bool acs_pp_to_current_x100(uint16_t raw_min,
                                   uint16_t raw_max,
                                   uint16_t sample_count,
                                   uint16_t present_count,
                                   uint16_t *current_x100)
{
    float vpp_mv;
    float vrms_mv;
    float current_a;
    int32_t value_x100;

    if ((sample_count < ACS_PP_SAMPLE_MIN) ||
        (present_count < (sample_count / 2U)) ||
        (raw_max < raw_min)) {
        return false;
    }

    vpp_mv = ((float)(raw_max - raw_min) * ADC_REFERENCE_MV *
              ACS_SENSOR_MV_PER_ADC_MV_NUM) /
             (ADC_FULL_SCALE * ACS_SENSOR_MV_PER_ADC_MV_DEN);
    vrms_mv = (vpp_mv * 0.5f) * ACS_SINE_RMS_FACTOR;
    current_a = vrms_mv / ACS_SENSITIVITY_MV_PER_A;
    if (!isfinite(current_a) || (current_a < 0.0f) || (current_a > 30.0f)) {
        return false;
    }

    value_x100 = (int32_t)lroundf(current_a * 100.0f);
    if (value_x100 <= (int32_t)ACS_ZERO_CURRENT_CUTOFF_X100) {
        value_x100 = 0;
    }
    if (value_x100 > 3000) {
        value_x100 = 3000;
    }

    *current_x100 = (uint16_t)value_x100;
    return true;
}

static void acs_pp_window_start(uint32_t now)
{
    memset(&g_acs_pp_window, 0, sizeof(g_acs_pp_window));
    g_acs_pp_window.start_tick = now;
    if (!adc_select_channel(ACS_ADC_CHANNEL)) {
        g_acs_pp_window.result_ready = true;
        return;
    }
    g_acs_pp_window.active = true;
}

static void acs_pp_window_finish(void)
{
    (void)HAL_ADC_Stop(&MOTOR_UI_ADC_HANDLE);
    g_acs_pp_window.active = false;
    g_acs_pp_window.result_ready = true;
    g_acs_pp_window.result_valid = acs_pp_to_current_x100(
        g_acs_pp_window.raw_min, g_acs_pp_window.raw_max,
        g_acs_pp_window.sample_count, g_acs_pp_window.present_count,
        &g_acs_pp_window.result_x100);
}

static void acs_pp_window_task(uint32_t now)
{
    uint16_t raw;

    if (!g_acs_pp_window.active) {
        return;
    }
    if ((now - g_acs_pp_window.start_tick) >= ACS_PP_WINDOW_MS) {
        acs_pp_window_finish();
        return;
    }
    if (!adc_convert_once(&raw)) {
        acs_pp_window_finish();
        g_acs_pp_window.result_valid = false;
        return;
    }

    if (g_acs_pp_window.sample_count == 0U) {
        g_acs_pp_window.raw_min = raw;
        g_acs_pp_window.raw_max = raw;
    } else {
        if (raw < g_acs_pp_window.raw_min) {
            g_acs_pp_window.raw_min = raw;
        }
        if (raw > g_acs_pp_window.raw_max) {
            g_acs_pp_window.raw_max = raw;
        }
    }
    if ((raw >= ACS_PRESENT_RAW_MIN) && (raw <= ACS_PRESENT_RAW_MAX)) {
        ++g_acs_pp_window.present_count;
    }
    ++g_acs_pp_window.sample_count;
}

static bool acs_pp_window_consume(uint16_t *current_x100)
{
    if (!g_acs_pp_window.result_ready) {
        if (g_acs_pp_window.active) {
            acs_pp_window_finish();
        }
        return false;
    }
    if (g_acs_pp_window.result_valid) {
        *current_x100 = g_acs_pp_window.result_x100;
    }
    return g_acs_pp_window.result_valid;
}

static void sensors_update(void)
{
    int16_t raw_temp_x10;
    uint16_t raw_current_x100;
    bool temp_ok = read_temperature_x10(&raw_temp_x10);
    bool current_ok = acs_pp_window_consume(&raw_current_x100);

    g_temp_sensor_valid = temp_ok;
    g_current_sensor_valid = current_ok;

    if (temp_ok) {
        if (!g_sensor_filter_initialized) {
            g_temp_filter_x10 = (float)raw_temp_x10;
        } else {
            g_temp_filter_x10 += NTC_FILTER_ALPHA *
                                 ((float)raw_temp_x10 - g_temp_filter_x10);
        }
        g_measured_temp_x10 = (int16_t)lroundf(g_temp_filter_x10);
    }

    if (current_ok) {
        g_measured_current_x100 = raw_current_x100;
    }

    update_display_hysteresis_values();

    if (temp_ok && current_ok) {
        g_sensor_invalid_count = 0U;
        if (g_sensor_valid_count < SENSOR_VALID_REQUIRED_COUNT) {
            ++g_sensor_valid_count;
        }
        if (g_sensor_valid_count >= SENSOR_VALID_REQUIRED_COUNT) {
            g_sensors_valid = true;
            g_sensor_fault = false;
        }
        g_sensor_filter_initialized = true;
    } else {
        g_sensor_valid_count = 0U;
        if (g_sensor_invalid_count < SENSOR_INVALID_TRIP_COUNT) {
            ++g_sensor_invalid_count;
        }
        if (g_sensor_invalid_count >= SENSOR_INVALID_TRIP_COUNT) {
            g_sensors_valid = false;
            g_sensor_fault = true;
        }
    }

    /* NOT: g_display_dirty buradan set edilmez.
     * Ekran yenilemesi DISPLAY_VALUE_UPDATE_MS (1s) zamanlayicisi,
     * alarm, buton ve blink olaylari tarafindan yonetilir.
     * Bu sayede sensur 200ms'de okunup alarm tetiklenirken
     * ekran gereksiz yere saniyede 5 kez yenilenmez. */
}
#endif

static bool alerts_are_armed(uint32_t now)
{
    return ((now - g_init_tick) >= ALERT_UI_ARM_MS);
}

static void format_sensor_fault_text(char *buffer, size_t buffer_size)
{
    if (!alerts_are_armed(HAL_GetTick())) {
        (void)snprintf(buffer, buffer_size, "---");
        return;
    }
    (void)snprintf(buffer, buffer_size, "ERROR");
}

static void format_temperature_for_display(int16_t value_x10,
                                           char *buffer,
                                           size_t buffer_size,
                                           bool edit_value)
{
    if (!g_temp_sensor_valid) {
        format_sensor_fault_text(buffer, buffer_size);
        return;
    }
    format_temperature(value_x10, buffer, buffer_size);
    if (edit_value && g_edit_mode && !g_blink_on) {
        blank_numeric_characters(buffer);
    }
}

static void format_temperature_set_for_display(int16_t value_x10,
                                             char *buffer,
                                             size_t buffer_size,
                                             bool edit_value)
{
    format_temperature_set(value_x10, buffer, buffer_size);
    if (edit_value && g_edit_mode && !g_blink_on) {
        blank_numeric_characters(buffer);
    }
}

static void format_temperature_main_for_display(int16_t value_x10,
                                                char *buffer,
                                                size_t buffer_size,
                                                bool edit_value)
{
    if (!g_temp_sensor_valid) {
        format_sensor_fault_text(buffer, buffer_size);
        return;
    }
    format_temperature(value_x10, buffer, buffer_size);
    (void)edit_value;
}

static void format_current_for_display(uint16_t value_x100,
                                       char *buffer,
                                       size_t buffer_size,
                                       bool edit_value)
{
    if (!g_current_sensor_valid) {
        format_sensor_fault_text(buffer, buffer_size);
        return;
    }
    format_current(value_x100, buffer, buffer_size);
    if (edit_value && g_edit_mode && !g_blink_on) {
        blank_numeric_characters(buffer);
    }
}

static void format_current_main_for_display(uint16_t value_x100,
                                            char *buffer,
                                            size_t buffer_size,
                                            bool edit_value)
{
    if (!g_current_sensor_valid) {
        format_sensor_fault_text(buffer, buffer_size);
        return;
    }
    format_current(value_x100, buffer, buffer_size);
    if (edit_value && g_edit_mode && !g_blink_on) {
        blank_numeric_characters(buffer);
    }
}

static void format_current_set_for_display(uint16_t value_x100,
                                           char *buffer,
                                           size_t buffer_size,
                                           bool edit_value)
{
    format_current(value_x100, buffer, buffer_size);
    if (edit_value && g_edit_mode && !g_blink_on) {
        blank_numeric_characters(buffer);
    }
}

/*
 * Asagidaki cizim fonksiyonlarinin koordinatlari, fontlari, cizgileri,
 * bitmap konumlari ve metin yerlesimleri kullanicinin OLED JSON tasarimiyla
 * birebir aynidir. Acilis (splash) ekrani kullanicinin verdigi
 * draw_acilis / 128x57 bitmap ile ayni. Uygulama mantigi yalnizca gorunen
 * degerleri ve bitmap gorunurlugunu degistirir.
 */
static void draw_splash_screen(void)
{
    /* SCREEN_ACILIS: u8g2_DrawXBM(..., 6, 7, 116, 51, bitmap_0_bits) */
    u8g2_DrawXBM(&g_u8g2,
                 ACILIS_BITMAP_X,
                 ACILIS_BITMAP_Y,
                 ACILIS_BITMAP_WIDTH,
                 ACILIS_BITMAP_HEIGHT,
                 acilis_bitmap_bits);
}

static void draw_main_screen(void)
{
    char temp_text[12];
    char current_text[12];

    format_temperature_main_for_display(g_display_temp_x10,
                                        temp_text,
                                        sizeof(temp_text),
                                        false);
    format_current_main_for_display(g_display_current_x100,
                                    current_text,
                                    sizeof(current_text),
                                    false);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 47U, 9U, "CRYSTAL");
    u8g2_DrawLine(&g_u8g2, 0U, 13U, 127U, 13U);
    u8g2_DrawLine(&g_u8g2, 65U, 14U, 65U, 64U);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 9U, 32U, "Sicaklik");
    u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
    u8g2_DrawUTF8(&g_u8g2, 4U, 52U, temp_text);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 88U, 32U, "Akim");
    u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
    u8g2_DrawUTF8(&g_u8g2, 68U, 52U, current_text);
}

static void draw_temp_alert_screen(void)
{
    char temp_text[12];
    char current_text[12];

    format_temperature_for_display(g_display_temp_x10, temp_text, sizeof(temp_text), false);
    format_current_for_display(g_display_current_x100, current_text, sizeof(current_text), false);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 47U, 9U, "CRYSTAL");
    u8g2_DrawLine(&g_u8g2, 0U, 13U, 127U, 13U);
    u8g2_DrawLine(&g_u8g2, 65U, 14U, 65U, 64U);

    /* Draw Current (Normal) */
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 88U, 32U, "Akim");
    u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
    u8g2_DrawUTF8(&g_u8g2, 68U, 52U, current_text);

    /* Draw Temp (Alerting) */
    if (g_blink_on) {
        /* tempalert_bits is 30x33 */
        u8g2_DrawXBM(&g_u8g2, 16U, 34U, 30U, 33U, tempalert_bits);
    } else {
        u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
        u8g2_DrawUTF8(&g_u8g2, 7U, 29U, "Sicaklik");
        u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
        u8g2_DrawUTF8(&g_u8g2, 16U, 40U, temp_text);
    }
}

static void draw_current_alert_screen(void)
{
    char temp_text[12];
    char current_text[12];

    format_temperature_for_display(g_display_temp_x10, temp_text, sizeof(temp_text), false);
    format_current_for_display(g_display_current_x100, current_text, sizeof(current_text), false);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 47U, 9U, "CRYSTAL");
    u8g2_DrawLine(&g_u8g2, 0U, 13U, 127U, 13U);
    u8g2_DrawLine(&g_u8g2, 68U, 14U, 68U, 64U);

    /* Draw Temp (Normal) */
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 9U, 32U, "Sicaklik");
    u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
    u8g2_DrawUTF8(&g_u8g2, 4U, 52U, temp_text);

    /* Draw Current (Alerting) */
    if (g_blink_on) {
        /* currentalert_bits is 24x24 */
        u8g2_DrawXBM(&g_u8g2, 89U, 38U, 24U, 24U, currentalert_bits);
    } else {
        u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
        u8g2_DrawUTF8(&g_u8g2, 89U, 28U, "Akim");
        u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
        u8g2_DrawUTF8(&g_u8g2, 86U, 38U, current_text);
    }
}

static void draw_both_alert_screen(void)
{
    char temp_text[12];
    char current_text[12];

    format_temperature_for_display(g_display_temp_x10, temp_text, sizeof(temp_text), false);
    format_current_for_display(g_display_current_x100, current_text, sizeof(current_text), false);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 47U, 9U, "CRYSTAL");
    u8g2_DrawLine(&g_u8g2, 0U, 13U, 127U, 13U);
    u8g2_DrawLine(&g_u8g2, 68U, 14U, 68U, 64U);

    /* Draw Temp (Alerting) */
    if (g_blink_on) {
        u8g2_DrawXBM(&g_u8g2, 18U, 33U, 30U, 33U, tempalert_bits);
    } else {
        u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
        u8g2_DrawUTF8(&g_u8g2, 9U, 28U, "Sicaklik");
        u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
        u8g2_DrawUTF8(&g_u8g2, 18U, 39U, temp_text);
    }

    /* Draw Current (Alerting) */
    if (g_blink_on) {
        u8g2_DrawXBM(&g_u8g2, 89U, 38U, 24U, 24U, currentalert_bits);
    } else {
        u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
        u8g2_DrawUTF8(&g_u8g2, 89U, 28U, "Akim");
        u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
        u8g2_DrawUTF8(&g_u8g2, 86U, 38U, current_text);
    }
}

static void draw_settings_screen(void)
{
    static const uint8_t arrow_y[4] = {16U, 28U, 41U, 53U};
    char temp_set_text[12];
    char current_set_text[12];

    format_temperature_set_for_display(g_set_temp_x10,
                                       temp_set_text,
                                       sizeof(temp_set_text),
                                       false);
    format_current_set_for_display(g_set_current_x100,
                                   current_set_text,
                                   sizeof(current_set_text),
                                   false);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 46U, 9U, "AYARLAR");
    u8g2_DrawLine(&g_u8g2, 0U, 13U, 127U, 13U);
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 27U, 24U, "Sicaklik");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 27U, 36U, "Akim");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 27U, 49U, "Varsayilan");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 27U, 61U, "Ana Ekrana Don");
    u8g2_DrawXBM(&g_u8g2, 3U, arrow_y[g_menu_index], 17U, 7U, arrow_bits);
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 92U, 24U, temp_set_text);
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 92U, 36U, current_set_text);
}

static void draw_temp_set_screen(void)
{
    char value_text[12];
    static const char title[] = "SICAKLIK AYARI";
    static const char hint[] = "OK Basip Ayarla";

    format_temperature_set_for_display(g_edit_temp_x10,
                                       value_text,
                                       sizeof(value_text),
                                       true);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2,
                  25U,
                  9U,
                  title);
    u8g2_DrawLine(&g_u8g2, 0U, 13U, 127U, 13U);
    u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
    u8g2_DrawUTF8(&g_u8g2,
                  34U,
                  36U,
                  value_text);
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2,
                  19U,
                  57U,
                  hint);
}

static void draw_current_set_screen(void)
{
    char value_text[12];
    static const char title[] = "AKIM AYARI";
    static const char hint[] = "OK Basip Ayarla";

    format_current_set_for_display(g_edit_current_x100,
                                   value_text,
                                   sizeof(value_text),
                                   true);

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2,
                  34U,
                  9U,
                  title);
    u8g2_DrawLine(&g_u8g2, 0U, 13U, 127U, 13U);
    u8g2_SetFont(&g_u8g2, u8g2_font_9x15_tf);
    u8g2_DrawUTF8(&g_u8g2,
                  28U,
                  36U,
                  value_text);
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2,
                  19U,
                  57U,
                  hint);
}

static void draw_default_confirm_screen(void)
{
    uint8_t arrow_y = (g_confirm_selection == CONFIRM_YES) ? 32U : 47U;

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 5U, 11U, "VARSAYILAN DEGERLERI");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 10U, 24U, "ONAYLIYOR MUSUNUZ?");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 29U, 40U, "EVET");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 29U, 55U, "HAYIR");
    u8g2_DrawXBM(&g_u8g2, 5U, arrow_y, 17U, 7U, arrow_bits);
}

static void draw_confirm_1_screen(void)
{
    uint8_t arrow_y = (g_confirm_selection == CONFIRM_YES) ? 32U : 47U;

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 22U, 11U, "DEGISIKLIKLERI");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 10U, 24U, "ONAYLIYOR MUSUNUZ?");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 29U, 40U, "EVET");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 29U, 55U, "HAYIR");
    u8g2_DrawXBM(&g_u8g2, 5U, arrow_y, 17U, 7U, arrow_bits);
}

static void draw_confirm_2_screen(void)
{
    uint8_t arrow_y = (g_confirm_selection == CONFIRM_YES) ? 32U : 47U;

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 16U, 12U, "DEGISIKLIKLERDEN");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 29U, 40U, "EVET");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&g_u8g2, 29U, 55U, "HAYIR");
    u8g2_DrawXBM(&g_u8g2, 5U, arrow_y, 17U, 7U, arrow_bits);
    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawUTF8(&g_u8g2, 25U, 24U, "EMIN MISINIZ?");
}


static void draw_screen_contents(void)
{
    switch (g_screen) {
    case UI_SCREEN_SPLASH:
        draw_splash_screen();
        break;
    case UI_SCREEN_MAIN:
        draw_main_screen();
        break;
    case UI_SCREEN_TEMP_ALERT:
        draw_temp_alert_screen();
        break;
    case UI_SCREEN_CURRENT_ALERT:
        draw_current_alert_screen();
        break;
    case UI_SCREEN_BOTH_ALERT:
        draw_both_alert_screen();
        break;
    case UI_SCREEN_SETTINGS:
        draw_settings_screen();
        break;
    case UI_SCREEN_TEMP_SET:
        draw_temp_set_screen();
        break;
    case UI_SCREEN_CURRENT_SET:
        draw_current_set_screen();
        break;
    case UI_SCREEN_DEFAULT_CONFIRM:
        draw_default_confirm_screen();
        break;
    case UI_SCREEN_CONFIRM_1:
        draw_confirm_1_screen();
        break;
    case UI_SCREEN_CONFIRM_2:
        draw_confirm_2_screen();
        break;
    default:
        break;
    }
}

static void render_display(void)
{
    /* F030 4KB SRAM: page buffer (_1) — ayni cordinat/font/bitmap, FirstPage/NextPage. */
    u8g2_FirstPage(&g_u8g2);
    do {
        draw_screen_contents();
    } while (u8g2_NextPage(&g_u8g2) != 0U);
    g_display_dirty = false;
}

static void cancel_edit_and_confirmation(void)
{
    g_edit_mode = false;
    g_pending_change = PENDING_NONE;
    g_confirm_selection = CONFIRM_NO;
}

static ui_screen_t alert_screen_for_type(alarm_type_t alarm)
{
    switch (alarm) {
    case ALARM_TEMPERATURE:
        return UI_SCREEN_TEMP_ALERT;
    case ALARM_CURRENT:
        return UI_SCREEN_CURRENT_ALERT;
    case ALARM_BOTH:
        return UI_SCREEN_BOTH_ALERT;
    case ALARM_SENSOR_FAULT:
        if (!g_temp_sensor_valid && !g_current_sensor_valid) {
            return UI_SCREEN_BOTH_ALERT;
        } else if (!g_temp_sensor_valid) {
            return UI_SCREEN_TEMP_ALERT;
        } else if (!g_current_sensor_valid) {
            return UI_SCREEN_CURRENT_ALERT;
        }
        return UI_SCREEN_BOTH_ALERT;
    case ALARM_NONE:
    default:
        return UI_SCREEN_MAIN;
    }
}

#if MOTOR_UI_STAGE >= 3U
static GPIO_PinState opposite_pin_state(GPIO_PinState state)
{
    return (state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

/* ========================================================================= *
 *                                                                           *
 *                    [ MODULE: HARDWARE & ACTUATORS ]                       *
 *                        (Buzzer, Relay, Alarm LEDs)                        *
 * ========================================================================= */

static void buzzer_write(bool enabled)
{
    bool active = enabled && !g_alarm_buzzer_muted;
    HAL_GPIO_WritePin(MOTOR_UI_BUZZER_GPIO_Port,
                      MOTOR_UI_BUZZER_Pin,
                      active ? BUZZER_ACTIVE_STATE
                             : opposite_pin_state(BUZZER_ACTIVE_STATE));
}

static uint32_t buzzer_symbol_duration(char symbol)
{
    return (symbol == '_') ? BUZZER_LONG_ON_MS : BUZZER_SHORT_ON_MS;
}

static const char *buzzer_pattern_for_alarm(alarm_type_t alarm)
{
    switch (alarm) {
    case ALARM_TEMPERATURE:
        return g_pattern_temperature;
    case ALARM_CURRENT:
        return g_pattern_current;
    case ALARM_BOTH:
        return g_pattern_both;
    case ALARM_SENSOR_FAULT:
#if SENSOR_FAULT_USES_BOTH_PATTERN
        return g_pattern_both;
#else
        return NULL;
#endif
    case ALARM_NONE:
    default:
        return NULL;
    }
}

static void buzzer_restart_pattern(alarm_type_t alarm, uint32_t now)
{
    g_buzzer_pattern = buzzer_pattern_for_alarm(alarm);
    g_buzzer_symbol_index = 0U;

    if ((g_buzzer_pattern == NULL) || (g_buzzer_pattern[0] == '\0')) {
        g_buzzer_phase = BUZZER_PHASE_IDLE;
        buzzer_write(false);
        return;
    }

    g_buzzer_phase = BUZZER_PHASE_ON;
    buzzer_write(true);
    if (g_buzzer_pattern[0] == 'C') {
        g_buzzer_deadline = now + 1000000000U;
    } else {
        g_buzzer_deadline = now +
            buzzer_symbol_duration(g_buzzer_pattern[g_buzzer_symbol_index]);
    }
}

static bool tick_reached(uint32_t now, uint32_t deadline)
{
    return ((int32_t)(now - deadline) >= 0);
}

static void buzzer_task(uint32_t now)
{
    uint8_t guard = 0U;

    if ((g_buzzer_pattern != NULL) && (g_buzzer_pattern[0] == 'C')) {
        g_buzzer_phase = BUZZER_PHASE_ON;
        buzzer_write(true);
        return;
    }

    while ((g_buzzer_phase != BUZZER_PHASE_IDLE) &&
           tick_reached(now, g_buzzer_deadline) &&
           (guard < 4U)) {
        ++guard;

        if (g_buzzer_phase == BUZZER_PHASE_ON) {
            buzzer_write(false);
            if (g_buzzer_pattern[g_buzzer_symbol_index + 1U] == '\0') {
                g_buzzer_phase = BUZZER_PHASE_PATTERN_GAP;
                g_buzzer_deadline += BUZZER_PATTERN_GAP_MS;
            } else {
                g_buzzer_phase = BUZZER_PHASE_SYMBOL_GAP;
                g_buzzer_deadline += BUZZER_SYMBOL_GAP_MS;
            }
        } else if (g_buzzer_phase == BUZZER_PHASE_SYMBOL_GAP) {
            ++g_buzzer_symbol_index;
            g_buzzer_phase = BUZZER_PHASE_ON;
            buzzer_write(true);
            g_buzzer_deadline +=
                buzzer_symbol_duration(g_buzzer_pattern[g_buzzer_symbol_index]);
        } else if (g_buzzer_phase == BUZZER_PHASE_PATTERN_GAP) {
            g_buzzer_symbol_index = 0U;
            g_buzzer_phase = BUZZER_PHASE_ON;
            buzzer_write(true);
            g_buzzer_deadline +=
                buzzer_symbol_duration(g_buzzer_pattern[0]);
        } else {
            g_buzzer_phase = BUZZER_PHASE_IDLE;
            buzzer_write(false);
        }
    }
}
#endif

#if MOTOR_UI_STAGE >= 3U
static void alarm_outputs_write(bool over_temp_active, bool over_current_active)
{
    HAL_GPIO_WritePin(MOTOR_UI_OVER_TEMP_GPIO_Port,
                      MOTOR_UI_OVER_TEMP_Pin,
                      over_temp_active ? OVER_TEMP_ACTIVE_LEVEL
                                       : opposite_pin_state(OVER_TEMP_ACTIVE_LEVEL));
    HAL_GPIO_WritePin(MOTOR_UI_OVER_CURRENT_GPIO_Port,
                      MOTOR_UI_OVER_CURRENT_Pin,
                      over_current_active ? OVER_CURRENT_ACTIVE_LEVEL
                                          : opposite_pin_state(OVER_CURRENT_ACTIVE_LEVEL));
}

static void status_led_write(bool enabled)
{
    HAL_GPIO_WritePin(MOTOR_UI_STATUS_LED_GPIO_Port,
                      MOTOR_UI_STATUS_LED_Pin,
                      enabled ? STATUS_LED_ACTIVE_LEVEL
                              : opposite_pin_state(STATUS_LED_ACTIVE_LEVEL));
}

static void discrete_outputs_update(void)
{
    bool over_temp = false;
    bool over_current = false;

    switch (g_alarm_type) {
    case ALARM_TEMPERATURE:
        over_temp = true;
        break;
    case ALARM_CURRENT:
        over_current = true;
        break;
    case ALARM_BOTH:
    case ALARM_SENSOR_FAULT:
        over_temp = true;
        over_current = true;
        break;
    case ALARM_NONE:
    default:
        break;
    }

    alarm_outputs_write(over_temp, over_current);
    /* Durum LED: alarm yokken yanar; karmaşık animasyon yok. */
    status_led_write(g_alarm_type == ALARM_NONE);
}
#endif

#if MOTOR_UI_STAGE >= 4U
static uint32_t g_last_relay_toggle_tick = 0U;
static bool g_relay_has_closed = false;

static void relay_apply_motor_permission(bool permitted)
{
    /* Bobin ULN2003 üzerinden sürülür; NO kontak polaritesi MOTOR_POWER_* ile.
     * izinli = kontak kapali (guc yolu), kesik = kontak acik. */
    HAL_GPIO_WritePin(MOTOR_UI_RELAY_GPIO_Port,
                      MOTOR_UI_RELAY_Pin,
                      permitted ? MOTOR_POWER_ALLOW_RELAY_LEVEL
                                : MOTOR_POWER_CUT_RELAY_LEVEL);
    g_motor_power_permitted = permitted;
    g_last_relay_toggle_tick = HAL_GetTick();
    if (permitted) {
        g_relay_has_closed = true;
    }
}
#endif

static void safety_update_outputs(uint32_t now)
{
#if MOTOR_UI_STAGE >= 3U
    static alarm_type_t last_buzzer_alarm = ALARM_NONE;
    alarm_type_t active_buzzer_alarm = (g_first_cut_alarm != ALARM_NONE) ? g_first_cut_alarm : g_alarm_type;
    if (last_buzzer_alarm != active_buzzer_alarm) {
        last_buzzer_alarm = active_buzzer_alarm;
        buzzer_restart_pattern(active_buzzer_alarm, now);
    }
    buzzer_task(now);
    discrete_outputs_update();
#else
    (void)now;
#endif


    /* Arm sonrasi: yalniz akim alarmi veya sensor hatasi istegi düsürür.
     * Ilk ALERT_UI_ARM_MS icinde latch yok; aksi halde NTC yokken yol hic kapanmaz.
     * Sicaklik alarminda istek düsmez, %20 altina inince yol yeniden kapanabilir. */
    {
        const bool alerts_armed = alerts_are_armed(now);
        if (alerts_armed &&
            ((g_alarm_type == ALARM_CURRENT) || (g_alarm_type == ALARM_BOTH) || g_sensor_fault)) {
            g_motor_run_requested = false;
        }

#if MOTOR_UI_STAGE >= 4U
    {
        bool startup_finished = ((now - g_init_tick) >= RELAY_SAFE_STARTUP_MS);
        bool chatter_guard_passed = ((now - g_last_relay_toggle_tick) >= RELAY_CHATTER_GUARD_MS);
        bool sensor_permission = true;
        bool permitted;

        if (alerts_armed) {
#if RELAY_REQUIRE_VALID_SENSORS
            sensor_permission = g_sensors_valid;
#endif
#if SENSOR_FAULT_CUTS_RELAY
            if (g_sensor_fault) {
                sensor_permission = false;
            }
#endif
        }

        /* Ara kesici: istek varsayilan acik. Alarm/sensor yoksa yol kapanir.
         * Motoru bu kart baslatmaz; yalniz guc yolunu keser veya birakir.
         * Ilk 5s (ALERT_UI_ARM_MS) sensor/ERROR kesmez. */
        permitted = g_motor_run_requested &&
                    startup_finished &&
                    sensor_permission &&
                    (g_alarm_type == ALARM_NONE);

        if (permitted != g_motor_power_permitted) {
            /* Alarm/hata/stop: kontak DERHAL acilir (!permitted).
             * Acilista ilk kapanis: yalniz RELAY_SAFE_STARTUP_MS (chatter yok).
             * Alarm sonrasi yeniden kapanis: en az RELAY_CHATTER_GUARD_MS. */
            if (!permitted || chatter_guard_passed || !g_relay_has_closed) {
                relay_apply_motor_permission(permitted);
            }
        }
    }
#else
    g_motor_power_permitted = false;
#endif
    }
}

static void update_alert_state(void)
{
    alarm_type_t old_alarm_type = g_alarm_type;
    bool on_alert_screen;
    bool in_config_ui;
    const uint32_t now = HAL_GetTick();
    const bool alerts_armed = ((now - g_init_tick) >= ALERT_UI_ARM_MS);

    if (!alerts_armed) {
        g_temp_alarm = false;
        g_temp_alarm_trip_count = 0U;
        g_temp_alarm_clear_count = 0U;
        g_current_alarm = false;
        g_current_alarm_trip_count = 0U;
        g_current_alarm_clear_count = 0U;
        g_alarm_type = ALARM_NONE;
        g_first_cut_alarm = ALARM_NONE;
        g_ui_alarm_candidate = ALARM_NONE;
        return;
    }

    if (!g_temp_sensor_valid) {
        g_temp_alarm = false;
        g_temp_alarm_trip_count = 0U;
        g_temp_alarm_clear_count = 0U;
    } else if (!g_temp_alarm) {
        g_temp_alarm_clear_count = 0U;
        if (g_measured_temp_x10 >= g_set_temp_x10) {
            if (g_temp_alarm_trip_count < ALARM_TRIP_REQUIRED_COUNT) ++g_temp_alarm_trip_count;
            if (g_temp_alarm_trip_count >= ALARM_TRIP_REQUIRED_COUNT) {
                g_temp_alarm = true;
                g_temp_alarm_trip_count = 0U;
            }
        } else {
            g_temp_alarm_trip_count = 0U;
        }
    } else {
        int16_t temp_clear_limit = (int16_t)(((int32_t)g_set_temp_x10 * 80) / 100);
        g_temp_alarm_trip_count = 0U;
        if (g_measured_temp_x10 <= temp_clear_limit) {
            if (g_temp_alarm_clear_count < ALARM_CLEAR_REQUIRED_COUNT) ++g_temp_alarm_clear_count;
            if (g_temp_alarm_clear_count >= ALARM_CLEAR_REQUIRED_COUNT) {
                g_temp_alarm = false;
                g_temp_alarm_clear_count = 0U;
            }
        } else {
            g_temp_alarm_clear_count = 0U;
        }
    }

    if (!g_current_sensor_valid) {
        g_current_alarm = false;
        g_current_alarm_trip_count = 0U;
        g_current_alarm_clear_count = 0U;
    } else if (!g_current_alarm) {
        g_current_alarm_clear_count = 0U;
        if (g_measured_current_x100 >= g_set_current_x100) {
            if (g_current_alarm_trip_count < ALARM_TRIP_REQUIRED_COUNT) ++g_current_alarm_trip_count;
            if (g_current_alarm_trip_count >= ALARM_TRIP_REQUIRED_COUNT) {
                g_current_alarm = true;
                g_current_alarm_trip_count = 0U;
            }
        } else {
            g_current_alarm_trip_count = 0U;
        }
    } else {
        uint16_t clear_limit =
            (g_set_current_x100 > CURRENT_ALERT_HYST_X100)
            ? (uint16_t)(g_set_current_x100 - CURRENT_ALERT_HYST_X100)
            : 0U;
        g_current_alarm_trip_count = 0U;
        if (g_measured_current_x100 <= clear_limit) {
            if (g_current_alarm_clear_count < ALARM_CLEAR_REQUIRED_COUNT) ++g_current_alarm_clear_count;
            if (g_current_alarm_clear_count >= ALARM_CLEAR_REQUIRED_COUNT) {
                g_current_alarm = false;
                g_current_alarm_clear_count = 0U;
            }
        } else {
            g_current_alarm_clear_count = 0U;
        }
    }

    if (g_sensor_fault) {
        g_alarm_type = ALARM_SENSOR_FAULT;
        ui_reset_user_activity(now);
    } else if (g_temp_alarm && g_current_alarm) {
        g_alarm_type = ALARM_BOTH;
        ui_reset_user_activity(now);
    } else if (g_temp_alarm) {
        g_alarm_type = ALARM_TEMPERATURE;
        ui_reset_user_activity(now);
    } else if (g_current_alarm) {
        g_alarm_type = ALARM_CURRENT;
        ui_reset_user_activity(now);
    } else {
        g_alarm_type = ALARM_NONE;
#if MOTOR_UI_STAGE >= 3U
        g_alarm_buzzer_muted = false;
#endif
    }

    if (g_alarm_type == ALARM_NONE) {
        g_first_cut_alarm = ALARM_NONE;
    } else {
        if ((g_first_cut_alarm == ALARM_TEMPERATURE) && !g_temp_alarm) {
            g_first_cut_alarm = ALARM_NONE;
        } else if ((g_first_cut_alarm == ALARM_CURRENT) && !g_current_alarm) {
            g_first_cut_alarm = ALARM_NONE;
        }

        if (g_first_cut_alarm == ALARM_NONE) {
            if (g_temp_alarm && g_current_alarm) {
                g_first_cut_alarm = ALARM_TEMPERATURE;
            } else if (g_temp_alarm) {
                g_first_cut_alarm = ALARM_TEMPERATURE;
            } else if (g_current_alarm) {
                g_first_cut_alarm = ALARM_CURRENT;
            }
        }
    }

    on_alert_screen =
        (g_screen == UI_SCREEN_TEMP_ALERT) ||
        (g_screen == UI_SCREEN_CURRENT_ALERT) ||
        (g_screen == UI_SCREEN_BOTH_ALERT);

    /* While splash / menus / set / confirm are up, pot noise must not yank
     * the UI to alarm and then dump the user on MAIN when it clears. */
    in_config_ui =
        (g_screen == UI_SCREEN_SPLASH) ||
        (g_screen == UI_SCREEN_SETTINGS) ||
        (g_screen == UI_SCREEN_TEMP_SET) ||
        (g_screen == UI_SCREEN_CURRENT_SET) ||
        (g_screen == UI_SCREEN_DEFAULT_CONFIRM) ||
        (g_screen == UI_SCREEN_CONFIRM_1) ||
        (g_screen == UI_SCREEN_CONFIRM_2);

    {
        if (g_alarm_type != ALARM_NONE) {
            ui_screen_t alert_screen = alert_screen_for_type(g_alarm_type);
            bool enter_stable;

            if (g_ui_alarm_candidate != g_alarm_type) {
                g_ui_alarm_candidate = g_alarm_type;
                g_ui_alarm_candidate_tick = now;
            }
            /* First entry needs dwell time; switching TEMP<->CURRENT<->BOTH
             * while already forced is immediate (avoids stuck half-alert UI). */
            enter_stable = g_alert_forced ||
                ((now - g_ui_alarm_candidate_tick) >= ALERT_UI_ENTER_MS);

            if ((!in_config_ui) && enter_stable) {
                if ((!g_alert_forced) ||
                    (g_screen != alert_screen)) {
                    cancel_edit_and_confirmation();
                    g_screen = alert_screen;
                    g_alert_forced = true;
                    g_blink_on = true;
                    g_last_blink_tick = now;
                    g_display_dirty = true;
                }
            }
        } else {
            g_ui_alarm_candidate = ALARM_NONE;
            if (g_alert_forced) {
                g_alert_forced = false;
                cancel_edit_and_confirmation();
                /* Only alert screens return to MAIN on clear — never steal SETTINGS. */
                if (on_alert_screen) {
                    g_screen = UI_SCREEN_MAIN;
                    g_display_dirty = true;
                }
            }
        }
    }

    if (old_alarm_type != g_alarm_type) {
        /* Avoid redrawing settings/confirm just because pots crossed a limit. */
        if (!in_config_ui) {
            g_display_dirty = true;
        }
        safety_update_outputs(now);
    }
}


#if MOTOR_UI_STAGE >= 2U
/* ========================================================================= *
 *                                                                           *
 *                     [ MODULE: BUTTONS & NAVIGATION ]                      *
 *                                                                           *
 * ========================================================================= */

static bool button_pin_pressed(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_PinState state = HAL_GPIO_ReadPin(port, pin);
#if MOTOR_UI_BUTTONS_ACTIVE_LOW
    return (state == GPIO_PIN_RESET);
#else
    return (state == GPIO_PIN_SET);
#endif
}

static void button_state_init(button_state_t *state,
                              bool pressed,
                              uint32_t now)
{
    state->raw_pressed = pressed;
    state->stable_pressed = pressed;
    state->raw_change_tick = now;
    state->press_start_tick = now;
    state->last_repeat_tick = now;
}

static uint8_t hold_multiplier(uint32_t held_ms)
{
    if (held_ms < BUTTON_HOLD_LEVEL_1_MS) {
        return 1U;
    }
    if (held_ms < BUTTON_HOLD_LEVEL_2_MS) {
        return 2U;
    }
    if (held_ms < BUTTON_HOLD_LEVEL_3_MS) {
        return 5U;
    }
    if (held_ms < BUTTON_HOLD_LEVEL_4_MS) {
        return 10U;
    }
    if (held_ms < BUTTON_HOLD_LEVEL_5_MS) {
        return 20U;
    }
    return 50U;
}

static uint32_t hold_repeat_interval(uint32_t held_ms)
{
    if (held_ms < BUTTON_HOLD_LEVEL_1_MS) {
        return BUTTON_REPEAT_LEVEL_0_MS;
    }
    if (held_ms < BUTTON_HOLD_LEVEL_2_MS) {
        return BUTTON_REPEAT_LEVEL_1_MS;
    }
    if (held_ms < BUTTON_HOLD_LEVEL_3_MS) {
        return BUTTON_REPEAT_LEVEL_2_MS;
    }
    if (held_ms < BUTTON_HOLD_LEVEL_4_MS) {
        return BUTTON_REPEAT_LEVEL_3_MS;
    }
    if (held_ms < BUTTON_HOLD_LEVEL_5_MS) {
        return BUTTON_REPEAT_LEVEL_4_MS;
    }
    return BUTTON_REPEAT_LEVEL_5_MS;
}

static void button_poll_one(button_state_t *state,
                            bool raw_pressed,
                            button_id_t button,
                            bool allow_repeat,
                            uint32_t now)
{
    if (raw_pressed != state->raw_pressed) {
        state->raw_pressed = raw_pressed;
        state->raw_change_tick = now;
    }

    if ((state->stable_pressed != state->raw_pressed) &&
        ((now - state->raw_change_tick) >= BUTTON_DEBOUNCE_MS)) {
        state->stable_pressed = state->raw_pressed;

        if (state->stable_pressed) {
            state->press_start_tick = now;
            state->last_repeat_tick = now;
            handle_button_action(button, 1U, false);
        }
    }

    if (allow_repeat && state->stable_pressed) {
        uint32_t held_ms = now - state->press_start_tick;
        if (held_ms >= BUTTON_HOLD_START_MS) {
            uint32_t interval = hold_repeat_interval(held_ms);
            if ((now - state->last_repeat_tick) >= interval) {
                state->last_repeat_tick = now;
                handle_button_action(button,
                                     hold_multiplier(held_ms),
                                     true);
            }
        }
    }
}

static void buttons_task(uint32_t now)
{
    bool ok_pressed;
    bool down_pressed;
    bool up_pressed;
#if MOTOR_UI_USE_BOOT_BUTTON
    bool boot_pressed;
#endif

    ok_pressed = button_pin_pressed(MOTOR_UI_BTN_OK_GPIO_Port,
                                    MOTOR_UI_BTN_OK_Pin);
    down_pressed = button_pin_pressed(MOTOR_UI_BTN_DOWN_GPIO_Port,
                                      MOTOR_UI_BTN_DOWN_Pin);
    up_pressed = button_pin_pressed(MOTOR_UI_BTN_UP_GPIO_Port,
                                    MOTOR_UI_BTN_UP_Pin);
#if MOTOR_UI_USE_BOOT_BUTTON
    boot_pressed = button_pin_pressed(MOTOR_UI_BTN_BOOT_GPIO_Port,
                                      MOTOR_UI_BTN_BOOT_Pin);
#endif

    if (ok_pressed || down_pressed || up_pressed
#if MOTOR_UI_USE_BOOT_BUTTON
        || boot_pressed
#endif
    ) {
        g_display_dirty = true;
    }

    {
        static bool prev_allow_hold = false;
        bool allow_hold;

        /* OK/BOOT first: leaving/entering set must re-sync UP/DOWN before
         * they are polled, or a held UP becomes a ghost EVET toggle / +step. */
        button_poll_one(&g_button_ok, ok_pressed, BUTTON_ID_OK, false, now);
#if MOTOR_UI_USE_BOOT_BUTTON
        button_poll_one(&g_button_boot, boot_pressed, BUTTON_ID_BOOT, false, now);
#endif

        allow_hold =
            (g_screen == UI_SCREEN_TEMP_SET) ||
            (g_screen == UI_SCREEN_CURRENT_SET);
        if (allow_hold != prev_allow_hold) {
            button_state_init(&g_button_up, up_pressed, now);
            button_state_init(&g_button_down, down_pressed, now);
            prev_allow_hold = allow_hold;
        }

        button_poll_one(&g_button_down,
                        down_pressed,
                        BUTTON_ID_DOWN,
                        allow_hold,
                        now);
        button_poll_one(&g_button_up,
                        up_pressed,
                        BUTTON_ID_UP,
                        allow_hold,
                        now);
    }

#if MOTOR_UI_STAGE >= 3U
    if ((g_alarm_type != ALARM_NONE) && !g_alarm_buzzer_muted) {
        if (g_button_ok.stable_pressed && ((now - g_button_ok.press_start_tick) >= ALARM_MUTE_HOLD_MS)) {
            g_alarm_buzzer_muted = true;
            buzzer_write(false);
        }
    }
#endif

#if MOTOR_UI_USE_BOOT_BUTTON
    /* BOOT + OK birlikte 10 saniye basili tutulursa Fabrika Ayarlarina Donulur (Factory Reset) */
    if (g_button_ok.stable_pressed && g_button_boot.stable_pressed) {
        uint32_t ok_hold = now - g_button_ok.press_start_tick;
        uint32_t boot_hold = now - g_button_boot.press_start_tick;
        if ((ok_hold >= FACTORY_RESET_HOLD_MS) && (boot_hold >= FACTORY_RESET_HOLD_MS)) {
            g_set_temp_x10 = TEMP_DEFAULT_X10;
            g_set_current_x100 = CURRENT_DEFAULT_X100;
#if MOTOR_UI_STAGE >= 3U
            (void)settings_save();
#endif
            cancel_edit_and_confirmation();
            g_menu_index = 0U;
            g_screen = UI_SCREEN_SETTINGS;
            g_display_dirty = true;
            ui_reset_user_activity(now);
            update_alert_state();
            g_button_ok.press_start_tick = now;
            g_button_boot.press_start_tick = now;
        }
    }
#endif

    g_button_irq_hint = false;
}

static void enter_settings(void)
{
    cancel_edit_and_confirmation();
    g_screen = UI_SCREEN_SETTINGS;
    g_display_dirty = true;
}

static void enter_settings_from_main(void)
{
    g_menu_index = 0U;
    enter_settings();
}

static void return_to_main(void)
{
    cancel_edit_and_confirmation();
    g_menu_index = 0U;
    g_screen = UI_SCREEN_MAIN;
    g_display_dirty = true;
    /* If pots are still above limits, show alarm instead of a silent main. */
    update_alert_state();
}

static void begin_double_confirmation(pending_change_t change)
{
    g_pending_change = change;
    g_confirm_selection = CONFIRM_NO;
    g_screen = UI_SCREEN_CONFIRM_1;
    g_display_dirty = true;
}

static void commit_pending_change(void)
{
    switch (g_pending_change) {
    case PENDING_TEMP:
        g_set_temp_x10 = snap_temp_whole_x10(g_edit_temp_x10);
        g_menu_index = 0U;
        break;
    case PENDING_CURRENT:
        g_set_current_x100 = g_edit_current_x100;
        g_menu_index = 1U;
        break;
    case PENDING_DEFAULTS:
        g_set_temp_x10 = TEMP_DEFAULT_X10;
        g_set_current_x100 = CURRENT_DEFAULT_X100;
        g_menu_index = 2U;
        break;
    case PENDING_NONE:
    default:
        break;
    }

#if MOTOR_UI_STAGE >= 3U
    (void)settings_save();
#endif

    cancel_edit_and_confirmation();
    g_screen = UI_SCREEN_SETTINGS;
    g_display_dirty = true;
    update_alert_state();
}

static void settings_select_current_item(void)
{
    switch (g_menu_index) {
    case 0U:
        g_edit_temp_x10 = snap_temp_whole_x10(g_set_temp_x10);
        g_edit_mode = true;
        g_blink_on = true;
        g_last_blink_tick = HAL_GetTick();
        g_screen = UI_SCREEN_TEMP_SET;
        break;
    case 1U:
        g_edit_current_x100 = g_set_current_x100;
        g_edit_mode = true;
        g_blink_on = true;
        g_last_blink_tick = HAL_GetTick();
        g_screen = UI_SCREEN_CURRENT_SET;
        break;
    case 2U:
        g_pending_change = PENDING_DEFAULTS;
        g_confirm_selection = CONFIRM_NO;
        g_screen = UI_SCREEN_DEFAULT_CONFIRM;
        break;
    case 3U:
        return_to_main();
        return;
    default:
        break;
    }
    g_display_dirty = true;
}

static void process_confirmation_button(button_id_t button)
{
    if ((button == BUTTON_ID_UP) || (button == BUTTON_ID_DOWN)) {
        g_confirm_selection = (g_confirm_selection == CONFIRM_YES)
                            ? CONFIRM_NO : CONFIRM_YES;
        g_display_dirty = true;
        return;
    }

    if (button == BUTTON_ID_BOOT) {
        enter_settings();
        return;
    }

    /* OK: EVET ise sonraki aşamaya geçer. HAYIR ise Ayarlar menüsüne döner. */
    if (button == BUTTON_ID_OK) {
        if (g_confirm_selection == CONFIRM_NO) {
            cancel_edit_and_confirmation();
            enter_settings();
            return;
        }

        if (g_screen == UI_SCREEN_DEFAULT_CONFIRM) {
            g_confirm_selection = CONFIRM_NO;
            g_screen = UI_SCREEN_CONFIRM_1;
            g_display_dirty = true;
        } else if (g_screen == UI_SCREEN_CONFIRM_1) {
            g_confirm_selection = CONFIRM_NO;
            g_screen = UI_SCREEN_CONFIRM_2;
            g_display_dirty = true;
        } else if (g_screen == UI_SCREEN_CONFIRM_2) {
            commit_pending_change();
        }
    }
}

static void handle_button_action(button_id_t button,
                                 uint8_t acceleration_multiplier,
                                 bool is_repeat)
{
    int32_t delta;
    (void)is_repeat;

    ui_reset_user_activity(HAL_GetTick());

    switch (g_screen) {
    case UI_SCREEN_SPLASH:
        /* Ignore all buttons until boot splash finishes. */
        break;

    case UI_SCREEN_MAIN:
        /* BOOT opens settings from main screen. OK does nothing on main screen. */
        if (button == BUTTON_ID_BOOT) {
            enter_settings_from_main();
        }
        break;

    case UI_SCREEN_SETTINGS:
        if (button == BUTTON_ID_UP) {
            g_menu_index = (g_menu_index == 0U) ? 3U
                                                 : (uint8_t)(g_menu_index - 1U);
            g_display_dirty = true;
        } else if (button == BUTTON_ID_DOWN) {
            g_menu_index = (g_menu_index >= 3U) ? 0U
                                                : (uint8_t)(g_menu_index + 1U);
            g_display_dirty = true;
        } else if (button == BUTTON_ID_OK) {
            settings_select_current_item();
        } else if (button == BUTTON_ID_BOOT) {
            return_to_main();
        }
        break;

    case UI_SCREEN_TEMP_SET:
        if (button == BUTTON_ID_BOOT) {
            enter_settings();
        } else if (button == BUTTON_ID_OK) {
            g_edit_mode = false;
            begin_double_confirmation(PENDING_TEMP);
        } else if ((button == BUTTON_ID_UP) || (button == BUTTON_ID_DOWN)) {
            delta = (int32_t)TEMP_FINE_STEP_X10 *
                    (int32_t)acceleration_multiplier;
            if (button == BUTTON_ID_DOWN) {
                delta = -delta;
            }
            g_edit_temp_x10 =
                snap_temp_whole_x10(
                    clamp_i16((int32_t)g_edit_temp_x10 + delta,
                              TEMP_MIN_X10,
                              TEMP_MAX_X10));
            g_display_dirty = true;
        }
        break;

    case UI_SCREEN_CURRENT_SET:
        if (button == BUTTON_ID_BOOT) {
            enter_settings();
        } else if (button == BUTTON_ID_OK) {
            g_edit_mode = false;
            begin_double_confirmation(PENDING_CURRENT);
        } else if ((button == BUTTON_ID_UP) || (button == BUTTON_ID_DOWN)) {
            delta = (int32_t)CURRENT_FINE_STEP_X100 *
                    (int32_t)acceleration_multiplier;
            if (button == BUTTON_ID_DOWN) {
                delta = -delta;
            }
            g_edit_current_x100 =
                clamp_u16((int32_t)g_edit_current_x100 + delta,
                          CURRENT_MIN_X100,
                          CURRENT_MAX_X100);
            g_display_dirty = true;
        }
        break;

    case UI_SCREEN_DEFAULT_CONFIRM:
    case UI_SCREEN_CONFIRM_1:
    case UI_SCREEN_CONFIRM_2:
        process_confirmation_button(button);
        break;

    case UI_SCREEN_TEMP_ALERT:
    case UI_SCREEN_CURRENT_ALERT:
    case UI_SCREEN_BOTH_ALERT:
        /* Alarm UI: only BOOT opens settings; alarm state stays latched. */
        if (button == BUTTON_ID_BOOT) {
            enter_settings_from_main();
        }
        break;

    default:
        break;
    }
}
#endif

static void board_outputs_safe_init(void)
{
#if MOTOR_UI_SAFE_GPIO_ON_INIT
    /* Stage bagimsiz guvenli baslangic: bobin enerjisiz (NO kontak acik),
     * buzzer/alarm off. ACS sifir bu pencerede alinir; sonra Task yol kapatir.
     * Ana durum makinesini degistirmez; yalniz pin ODR guvenligi. */
    HAL_GPIO_WritePin(MOTOR_UI_BUZZER_GPIO_Port,
                      MOTOR_UI_BUZZER_Pin,
                      (BUZZER_ACTIVE_LEVEL == GPIO_PIN_SET)
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_UI_OVER_TEMP_GPIO_Port,
                      MOTOR_UI_OVER_TEMP_Pin,
                      (OVER_TEMP_ACTIVE_LEVEL == GPIO_PIN_SET)
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_UI_OVER_CURRENT_GPIO_Port,
                      MOTOR_UI_OVER_CURRENT_Pin,
                      (OVER_CURRENT_ACTIVE_LEVEL == GPIO_PIN_SET)
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_UI_STATUS_LED_GPIO_Port,
                      MOTOR_UI_STATUS_LED_Pin,
                      (STATUS_LED_ACTIVE_LEVEL == GPIO_PIN_SET)
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_UI_RELAY_GPIO_Port,
                      MOTOR_UI_RELAY_Pin,
                      MOTOR_POWER_CUT_RELAY_LEVEL);
#endif
}

/* ========================================================================= *
 *                                                                           *
 *                             [ MODULE: CORE API ]                          *
 *                                                                           *
 * ========================================================================= */

void MotorUI_Init(void)
{
    uint32_t now = HAL_GetTick();

    g_init_tick = now;
    g_set_temp_x10 = TEMP_DEFAULT_X10;
    g_set_current_x100 = CURRENT_DEFAULT_X100;
    g_motor_run_requested = (MOTOR_RUN_REQUEST_DEFAULT != 0U);
    g_motor_power_permitted = false;
#if MOTOR_UI_STAGE >= 4U
    g_relay_has_closed = false;
#endif

    board_outputs_safe_init();

#if MOTOR_UI_STAGE >= 3U
    g_alarm_buzzer_muted = false;
    buzzer_write(false);
    alarm_outputs_write(false, false);
    status_led_write(false);
#endif
#if MOTOR_UI_STAGE >= 4U
    relay_apply_motor_permission(false);
#endif

#if MOTOR_UI_STAGE >= 3U
    if (!settings_load()) {
        (void)settings_save();
    }
    (void)HAL_ADCEx_Calibration_Start(&MOTOR_UI_ADC_HANDLE);
    HAL_Delay(100U);
    /* Bilgi: bu sonuc ilk sensors_update() cagrisinda p-p pencere tabanli
     * gecerlilikle yeniden yazilir; yalniz acilis bilgilendirmesidir. */
    g_current_sensor_valid = acs_validate_presence();
    sensors_update();
    /* Ilk pencere burada baslar; ilk 200ms sensor cevriminde sonucu tuketilir. */
    acs_pp_window_start(HAL_GetTick());
    update_alert_state();
#endif

    u8g2_Setup_ssd1306_i2c_128x64_noname_1(&g_u8g2,
                                            U8G2_R0,
                                            u8x8_byte_stm32_hw_i2c,
                                            u8x8_stm32_gpio_and_delay);
    u8g2_SetI2CAddress(&g_u8g2, (uint8_t)(OLED_I2C_ADDRESS_7BIT << 1));
    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0U);
    u8g2_SetFontMode(&g_u8g2, 1U);
    u8g2_SetBitmapMode(&g_u8g2, 1U);

#if MOTOR_UI_STAGE >= 2U
    button_state_init(&g_button_ok, button_pin_pressed(MOTOR_UI_BTN_OK_GPIO_Port, MOTOR_UI_BTN_OK_Pin), now);
    button_state_init(&g_button_down, button_pin_pressed(MOTOR_UI_BTN_DOWN_GPIO_Port, MOTOR_UI_BTN_DOWN_Pin), now);
    button_state_init(&g_button_up, button_pin_pressed(MOTOR_UI_BTN_UP_GPIO_Port, MOTOR_UI_BTN_UP_Pin), now);
#if MOTOR_UI_USE_BOOT_BUTTON
    button_state_init(&g_button_boot, button_pin_pressed(MOTOR_UI_BTN_BOOT_GPIO_Port, MOTOR_UI_BTN_BOOT_Pin), now);
#endif
#endif

    g_screen = UI_SCREEN_SPLASH;
    g_display_dirty = true;
    g_last_sensor_tick = now;
    g_last_display_tick = now;
    g_last_blink_tick = now;
    g_last_user_activity_tick = now;
    g_oled_dimmed = false;
#if MOTOR_UI_STAGE >= 1U
    u8g2_SetContrast(&g_u8g2, OLED_CONTRAST_HIGH);
#endif
    /* Do not evaluate alerts during splash; sensors settle in the background. */
    safety_update_outputs(now);
    render_display();
}

void MotorUI_Task(void)
{
    uint32_t now = HAL_GetTick();

#if MOTOR_UI_STAGE >= 3U
    /* ADC sahipligi: ACS penceresi tek ornekli ilerler ve pencere bitince
     * ADC durur. NTC okumasina ait 200ms cevrim ancak bu durgun durumda baslar. */
    acs_pp_window_task(now);
#endif

    if (g_screen == UI_SCREEN_SPLASH) {
        if ((now - g_init_tick) >= UI_SPLASH_MS) {
            g_screen = UI_SCREEN_MAIN;
            g_display_dirty = true;
            update_alert_state();
        }
    }

#if MOTOR_UI_STAGE >= 2U
    buttons_task(now);
#endif

#if MOTOR_UI_STAGE >= 3U
    /* Guvenlik: her 200ms'de sensoru oku ve alarm degerlendir.
     * Boylece pompa stall durumu 200ms icinde fark edilir. */
    if ((now - g_last_sensor_tick) >= SENSOR_UPDATE_MS) {
        g_last_sensor_tick = now;
        sensors_update();
        /* Tamamlanan son ACS penceresi sensors_update() icinde tuketildi.
         * Sonraki pencere NTC okumasindan sonra baslar; iki kanal cakismiyor. */
        acs_pp_window_start(HAL_GetTick());
        update_alert_state();
    }
#else
    if ((now - g_last_sensor_tick) >= SENSOR_UPDATE_MS) {
        g_last_sensor_tick = now;
        if (g_screen != UI_SCREEN_SPLASH) {
            update_alert_state();
        }
    }
#endif

    safety_update_outputs(now);

#if MOTOR_UI_STAGE >= 3U && EEPROM_ENABLE
    /* EEPROM yazimi alarm/role yolundan sonra, tek I2C adimiyla ilerler. */
    settings_task(now);
#endif

    if (g_alert_forced || g_edit_mode) {
        uint32_t blink_interval = g_alert_forced
                                ? ALERT_BLINK_MS
                                : EDIT_VALUE_BLINK_MS;
        if ((now - g_last_blink_tick) >= blink_interval) {
            g_last_blink_tick = now;
            g_blink_on = !g_blink_on;
            g_display_dirty = true;
        }
    }

#if MOTOR_UI_STAGE >= 3U
    /* Ekran yenileme: ana ekran ve alarm ekrani 1s'de 1 deger gunceller.
     * Alarm, buton, blink olaylari aninda g_display_dirty=true yapar
     * bu nedenle gecikme yalnizca "sessiz" deger degisimlerini etkiler. */
    if ((now - g_last_display_tick) >= DISPLAY_VALUE_UPDATE_MS) {
        g_last_display_tick = now;
        if ((g_screen == UI_SCREEN_MAIN) || g_alert_forced) {
            g_display_dirty = true;
        }
    }
#endif

    /* OLED Auto-Dimmer: 5dk boyunca tus basilmazsa ve alarm yoksa parlaklik %5'e dusur.
     * Alarm geldiginde veya tusa basildiginda aninda %100'e cik. */
    if (!g_oled_dimmed && (g_alarm_type == ALARM_NONE) && !g_sensor_fault) {
        if ((now - g_last_user_activity_tick) >= OLED_AUTO_DIM_TIMEOUT_MS) {
            g_oled_dimmed = true;
#if MOTOR_UI_STAGE >= 1U
            u8g2_SetContrast(&g_u8g2, OLED_CONTRAST_DIM);
            g_display_dirty = true;
#endif
        }
    }

    if (g_display_dirty) {
        render_display();
    }
}

void MotorUI_ForceRedraw(void)
{
    g_display_dirty = true;
}

void MotorUI_SetSimulatedValues(int16_t temperature_x10,
                                uint16_t current_x100)
{
    bool measured_changed;

    if (temperature_x10 < TEMP_MIN_X10) {
        temperature_x10 = TEMP_MIN_X10;
    } else if (temperature_x10 > TEMP_MAX_X10) {
        temperature_x10 = TEMP_MAX_X10;
    }
    if (current_x100 > CURRENT_MAX_X100) {
        current_x100 = CURRENT_MAX_X100;
    }

#if MOTOR_UI_STAGE < 3U
    /* Stage 1/2'de gercek sensor yok: simule degerler gecerli sayilir.
     * Yoksa formatlayicilar "---"/"ERROR" gosterir ve simule alarm
     * hic tetiklenmez (sensor gecerlilik bayraklari yalniz Stage >= 3
     * sensors_update() icinde set edilir). */
    g_temp_sensor_valid = true;
    g_current_sensor_valid = true;
#endif

    measured_changed =
        (g_measured_temp_x10 != temperature_x10) ||
        (g_measured_current_x100 != current_x100);

    g_measured_temp_x10 = temperature_x10;
    g_measured_current_x100 = current_x100;
    update_display_hysteresis_values();
    if (g_screen != UI_SCREEN_SPLASH) {
        update_alert_state();
    }

    /* Only measured-value screens need a redraw from pot noise.
     * Settings/confirm screens show setpoints; redrawing them every ADC
     * tick freezes Wokwi (heavy UTF-8 + I2C). Alarm transitions already
     * mark dirty inside update_alert_state().
     */
    if (measured_changed &&
        ((g_screen == UI_SCREEN_MAIN) ||
         (g_screen == UI_SCREEN_TEMP_ALERT) ||
         (g_screen == UI_SCREEN_CURRENT_ALERT) ||
         (g_screen == UI_SCREEN_BOTH_ALERT))) {
        g_display_dirty = true;
    }
}

void MotorUI_SetMotorRunRequest(bool run_requested)
{
    g_motor_run_requested = run_requested;
    safety_update_outputs(HAL_GetTick());
}

bool MotorUI_IsMotorRunRequested(void)
{
    return g_motor_run_requested;
}

bool MotorUI_IsMotorPowerPermitted(void)
{
    return g_motor_power_permitted;
}

int16_t MotorUI_GetTemperatureX10(void)
{
    return g_measured_temp_x10;
}

uint16_t MotorUI_GetCurrentX100(void)
{
    return g_measured_current_x100;
}

int16_t MotorUI_GetTemperatureSetX10(void)
{
    return g_set_temp_x10;
}

uint16_t MotorUI_GetCurrentSetX100(void)
{
    return g_set_current_x100;
}

void MotorUI_ButtonIRQ(uint16_t gpio_pin)
{
#if MOTOR_UI_STAGE >= 2U
    if ((gpio_pin == MOTOR_UI_BTN_OK_Pin) ||
        (gpio_pin == MOTOR_UI_BTN_DOWN_Pin) ||
        (gpio_pin == MOTOR_UI_BTN_UP_Pin)
#if MOTOR_UI_USE_BOOT_BUTTON
        || (gpio_pin == MOTOR_UI_BTN_BOOT_Pin)
#endif
       ) {
        g_button_irq_hint = true;
    }
#else
    (void)gpio_pin;
#endif
}

#if MOTOR_UI_STAGE >= 2U && MOTOR_UI_DEFINE_HAL_EXTI_CALLBACK
/*
 * Projede tek HAL_GPIO_EXTI_Callback bu olmalidir.
 * CubeMX urettigi EXTI0_1_IRQHandler / EXTI4_15_IRQHandler -> HAL_GPIO_EXTI_IRQHandler
 * zinciri bu callback'e duser. Sadece flag: OLED / EEPROM / ADC yok.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    MotorUI_ButtonIRQ(GPIO_Pin);
}
#endif

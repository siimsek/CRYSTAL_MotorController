#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* Include the production unit so the host test can exercise its static state
 * machines with deterministic HAL responses. */
#include "../src/motor_ui.c"

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
ADC_TypeDef test_adc_instance;
ADC_HandleTypeDef hadc = { .Instance = &test_adc_instance };
GPIO_TypeDef gpio_a;
GPIO_TypeDef gpio_b;
GPIO_TypeDef gpio_c;
uint32_t SystemCoreClock;

static uint32_t test_tick;
static GPIO_PinState relay_level;
static HAL_StatusTypeDef adc_config_status = HAL_OK;
static HAL_StatusTypeDef adc_start_status = HAL_OK;
static HAL_StatusTypeDef adc_poll_status = HAL_OK;
static uint16_t adc_samples[256];
static size_t adc_sample_count;
static size_t adc_sample_index;
static uint8_t eeprom_memory[256];
static HAL_StatusTypeDef eeprom_write_start_status = HAL_OK;
static struct {
    uint16_t address;
    uint8_t *data;
    uint16_t length;
    bool active;
} eeprom_write_pending;

uint32_t HAL_GetTick(void) { return test_tick; }
void HAL_Delay(uint32_t ms) { test_tick += ms; }
void u8g2_SetContrast(u8g2_t *u8g2, uint8_t contrast)
{ (void)u8g2; (void)contrast; }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t a, uint32_t t, uint32_t r)
{ (void)h; (void)a; (void)t; (void)r; return HAL_ERROR; }
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h, uint16_t a, uint16_t m, uint16_t s, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h; (void)a; (void)s; (void)t; memcpy(d, &eeprom_memory[m], n); return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h, uint16_t a, uint16_t m, uint16_t s, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h; (void)a; (void)m; (void)s; (void)d; (void)n; (void)t; return HAL_ERROR; }
HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *h, uint16_t a, uint16_t m, uint16_t s, uint8_t *d, uint16_t n)
{
    (void)h; (void)a; (void)s;
    if (eeprom_write_start_status != HAL_OK) return eeprom_write_start_status;
    eeprom_write_pending.address = m;
    eeprom_write_pending.data = d;
    eeprom_write_pending.length = n;
    eeprom_write_pending.active = true;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t a, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h; (void)a; (void)d; (void)n; (void)t; return HAL_ERROR; }
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *h, ADC_ChannelConfTypeDef *c)
{ (void)h; (void)c; return adc_config_status; }
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h) { (void)h; return adc_start_status; }
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h, uint32_t timeout)
{ (void)h; (void)timeout; return adc_poll_status; }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h)
{
    (void)h;
    assert(adc_sample_index < adc_sample_count);
    return adc_samples[adc_sample_index++];
}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h) { (void)h; return HAL_OK; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin) { (void)p; (void)pin; return GPIO_PIN_RESET; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState level)
{
    if ((p == MOTOR_UI_RELAY_GPIO_Port) && (pin == MOTOR_UI_RELAY_Pin)) {
        relay_level = level;
    }
}

static void reset_safety_state(void)
{
    test_tick = 0U;
    relay_level = MOTOR_POWER_CUT_RELAY_LEVEL;
    g_init_tick = 0U;
    g_last_relay_toggle_tick = 0U;
    g_relay_has_closed = false;
    g_motor_power_permitted = false;
    g_motor_run_requested = true;
    g_sensors_valid = true;
    g_sensor_fault = false;
    g_alarm_type = ALARM_NONE;
    g_first_cut_alarm = ALARM_NONE;
}

static void eeprom_complete_write(void)
{
    assert(eeprom_write_pending.active);
    memcpy(&eeprom_memory[eeprom_write_pending.address],
           eeprom_write_pending.data, eeprom_write_pending.length);
    eeprom_write_pending.active = false;
    HAL_I2C_MemTxCpltCallback(&hi2c1);
}

static void reset_settings_state(void)
{
    memset(eeprom_memory, 0xFF, sizeof(eeprom_memory));
    eeprom_write_start_status = HAL_OK;
    eeprom_write_pending.active = false;
    g_settings_save_state = EEPROM_SAVE_IDLE;
    g_settings_save_pending = false;
    g_settings_i2c_done = false;
    g_settings_i2c_error = false;
    g_settings_sequence = 0U;
    g_settings_save_address = EEPROM_SLOT_0_ADDRESS;
    g_set_temp_x10 = TEMP_DEFAULT_X10;
    g_set_current_x100 = CURRENT_DEFAULT_X100;
}

static void settings_run_until_idle(void)
{
    size_t guard;
    for (guard = 0U; guard < 100U; ++guard) {
        settings_task(test_tick);
        if (eeprom_write_pending.active) eeprom_complete_write();
        test_tick += EEPROM_WRITE_TIMEOUT_MS;
        if ((g_settings_save_state == EEPROM_SAVE_IDLE) && !g_settings_save_pending) return;
    }
    assert(false);
}

static void test_eeprom_recovery(void)
{
    uint8_t legacy[SETTINGS_V2_RECORD_SIZE] = {0U};
    uint16_t crc;
    size_t cut_step;

    reset_settings_state();
    assert(!settings_load());
    assert(settings_save());
    settings_run_until_idle();
    g_set_temp_x10 = TEMP_DEFAULT_X10;
    g_set_current_x100 = CURRENT_DEFAULT_X100;
    assert(settings_load());
    assert(g_set_temp_x10 == TEMP_DEFAULT_X10);
    assert(g_set_current_x100 == CURRENT_DEFAULT_X100);
    g_set_temp_x10 = 800;
    assert(settings_save());
    settings_run_until_idle();
    g_set_temp_x10 = TEMP_DEFAULT_X10;
    assert(settings_load());
    assert(g_set_temp_x10 == 800);

    reset_settings_state();
    legacy[0] = SETTINGS_MAGIC_0; legacy[1] = SETTINGS_MAGIC_1;
    legacy[2] = SETTINGS_MAGIC_2; legacy[3] = SETTINGS_MAGIC_3;
    legacy[4] = SETTINGS_VERSION_V2;
    legacy[6] = (uint8_t)(800U & 0xFFU); legacy[7] = (uint8_t)(800U >> 8);
    legacy[8] = 180U; legacy[9] = 0U;
    crc = crc16_ccitt(legacy, 10U);
    legacy[10] = (uint8_t)crc; legacy[11] = (uint8_t)(crc >> 8);
    memcpy(eeprom_memory, legacy, sizeof(legacy));
    assert(settings_load());
    assert(g_set_temp_x10 == 800);
    assert(g_set_current_x100 == 180U);
    assert(g_settings_save_address == EEPROM_SLOT_1_ADDRESS);
    assert(settings_save());
    settings_run_until_idle();
    assert(settings_load());

    /* Every interrupted write leaves the previously committed slot bootable. */
    reset_settings_state();
    g_set_temp_x10 = 750;
    assert(settings_save());
    settings_run_until_idle();
    for (cut_step = 0U; cut_step < 3U; ++cut_step) {
        int16_t before_temp = g_set_temp_x10;
        reset_settings_state();
        /* Re-create the known valid record in slot 0 for each cut point. */
        g_set_temp_x10 = 750;
        assert(settings_save());
        settings_run_until_idle();
        g_set_temp_x10 = (int16_t)(760 + (int16_t)cut_step * 10);
        assert(settings_save());
        for (size_t completed = 0U; completed <= cut_step; ++completed) {
            settings_task(test_tick);
            eeprom_complete_write();
            test_tick += EEPROM_WRITE_TIMEOUT_MS;
            settings_task(test_tick);
            test_tick += EEPROM_WRITE_TIMEOUT_MS;
            settings_task(test_tick);
        }
        g_set_temp_x10 = 0;
        assert(settings_load());
        assert(g_set_temp_x10 == before_temp);
    }
}

static void test_eeprom_retry_and_safety(void)
{
    reset_settings_state();
    assert(settings_save());
    eeprom_write_start_status = HAL_ERROR;
    settings_task(test_tick);
    assert(g_settings_save_state == EEPROM_SAVE_RETRY_WAIT);
    test_tick += EEPROM_RETRY_DELAY_MS;
    settings_task(test_tick);
    eeprom_write_start_status = HAL_OK;
    settings_run_until_idle();

    /* IRQ error and missing completion both leave RAM settings intact and retry. */
    reset_settings_state();
    assert(settings_save());
    settings_task(test_tick);
    assert(eeprom_write_pending.active);
    HAL_I2C_ErrorCallback(&hi2c1);
    settings_task(test_tick);
    assert(g_settings_save_state == EEPROM_SAVE_RETRY_WAIT);

    reset_settings_state();
    assert(settings_save());
    settings_task(test_tick);
    assert(eeprom_write_pending.active);
    test_tick += EEPROM_WRITE_TIMEOUT_MS;
    settings_task(test_tick);
    assert(g_settings_save_state == EEPROM_SAVE_RETRY_WAIT);

    reset_safety_state();
    test_tick = ALERT_UI_ARM_MS;
    g_alarm_type = ALARM_CURRENT;
    g_motor_run_requested = true;
    g_settings_save_state = EEPROM_SAVE_RETRY_WAIT;
    g_settings_retry_tick = test_tick + EEPROM_RETRY_DELAY_MS;
    safety_update_outputs(test_tick);
    settings_task(test_tick);
    assert(!g_motor_power_permitted);
    assert(!g_motor_run_requested);
}

static void test_relay_interlocks(void)
{
    reset_safety_state();

    test_tick = RELAY_SAFE_STARTUP_MS;
    safety_update_outputs(test_tick);
    assert(g_motor_power_permitted);
    assert(relay_level == MOTOR_POWER_ALLOW_RELAY_LEVEL);

    /* Validity is deliberately ignored only during the five-second arm window. */
    g_sensors_valid = false;
    test_tick = ALERT_UI_ARM_MS - 1U;
    safety_update_outputs(test_tick);
    assert(g_motor_power_permitted);

    test_tick = ALERT_UI_ARM_MS;
    safety_update_outputs(test_tick);
    assert(!g_motor_power_permitted);
    assert(relay_level == MOTOR_POWER_CUT_RELAY_LEVEL);

    /* A run request cannot override an armed invalid-sensor interlock. */
    MotorUI_SetMotorRunRequest(true);
    assert(!g_motor_power_permitted);

    g_sensors_valid = true;
    g_sensor_fault = true;
    MotorUI_SetMotorRunRequest(true);
    assert(!g_motor_power_permitted);

    g_sensor_fault = false;
    g_alarm_type = ALARM_CURRENT;
    g_motor_run_requested = true;
    safety_update_outputs(test_tick);
    assert(!g_motor_run_requested);
    assert(!g_motor_power_permitted);

    /* Temperature trips cut the relay but do not latch the run request low. */
    g_alarm_type = ALARM_TEMPERATURE;
    g_motor_run_requested = true;
    safety_update_outputs(test_tick);
    assert(g_motor_run_requested);
    assert(!g_motor_power_permitted);
}

static void test_acs_window(void)
{
    size_t i;
    uint16_t current_x100 = 0U;

    adc_config_status = HAL_OK;
    adc_start_status = HAL_OK;
    adc_poll_status = HAL_OK;
    adc_sample_count = ADC_DUMMY_CONVERSIONS + ACS_PP_SAMPLE_MIN;
    adc_sample_index = 0U;
    for (i = 0U; i < ADC_DUMMY_CONVERSIONS; ++i) adc_samples[i] = 2000U;
    for (i = 0U; i < ACS_PP_SAMPLE_MIN; ++i) {
        adc_samples[ADC_DUMMY_CONVERSIONS + i] = (i & 1U) ? 2500U : 1500U;
    }

    test_tick = 100U;
    acs_pp_window_start(test_tick);
    for (i = 0U; i < ACS_PP_SAMPLE_MIN; ++i) acs_pp_window_task(test_tick + 1U);
    acs_pp_window_task(test_tick + ACS_PP_WINDOW_MS);
    assert(acs_pp_window_consume(&current_x100));
    assert(current_x100 == 285U);

    /* ADC timeouts and insufficient samples are consumed as invalid readings. */
    adc_poll_status = HAL_ERROR;
    test_tick += SENSOR_UPDATE_MS;
    acs_pp_window_start(test_tick);
    acs_pp_window_task(test_tick + 1U);
    assert(!acs_pp_window_consume(&current_x100));

    assert(!acs_pp_to_current_x100(1500U, 2500U, ACS_PP_SAMPLE_MIN, 39U,
                                   &current_x100));
}

static void test_oled_defers_during_acs_window(void)
{
    size_t i;
    uint16_t current_x100 = 0U;

    adc_config_status = HAL_OK;
    adc_start_status = HAL_OK;
    adc_poll_status = HAL_OK;
    adc_sample_count = ADC_DUMMY_CONVERSIONS + ACS_PP_SAMPLE_MIN;
    adc_sample_index = 0U;
    for (i = 0U; i < ADC_DUMMY_CONVERSIONS; ++i) adc_samples[i] = 2000U;
    for (i = 0U; i < ACS_PP_SAMPLE_MIN; ++i) {
        adc_samples[ADC_DUMMY_CONVERSIONS + i] = (i & 1U) ? 2500U : 1500U;
    }

    test_tick = 500U;
    acs_pp_window_start(test_tick);
    g_display_dirty = true;
    /* A dirty OLED must wait; a page transfer may itself exceed 40 ms. */
    assert(display_render_is_deferred());
    for (i = 0U; i < ACS_PP_SAMPLE_MIN; ++i) {
        acs_pp_window_task(test_tick + 1U);
    }
    assert(g_acs_pp_window.sample_count >= ACS_PP_SAMPLE_MIN);

    acs_pp_window_task(test_tick + ACS_PP_WINDOW_MS);
    assert(!display_render_is_deferred());
    assert(acs_pp_window_consume(&current_x100));
    assert(current_x100 == 285U);
}

static void test_irq_is_flag_only(void)
{
    g_button_irq_hint = false;
    MotorUI_ButtonIRQ(MOTOR_UI_BTN_OK_Pin);
    assert(g_button_irq_hint);
    g_button_irq_hint = false;
    MotorUI_ButtonIRQ(GPIO_PIN_5);
    assert(!g_button_irq_hint);
}

static void test_i2c_irq_is_eeprom_flag_only(void)
{
    g_settings_i2c_done = false;
    g_settings_i2c_error = false;
    g_button_irq_hint = false;

    /* A callback for the OLED I2C bus cannot affect EEPROM state or EXTI. */
    HAL_I2C_MemTxCpltCallback(&hi2c2);
    HAL_I2C_ErrorCallback(&hi2c2);
    assert(!g_settings_i2c_done);
    assert(!g_settings_i2c_error);
    assert(!g_button_irq_hint);

    HAL_I2C_MemTxCpltCallback(&hi2c1);
    assert(g_settings_i2c_done);
    assert(!g_settings_i2c_error);
    assert(!g_button_irq_hint);

    HAL_I2C_ErrorCallback(&hi2c1);
    assert(g_settings_i2c_error);
    assert(!g_button_irq_hint);
}

static void test_button_safety_paths(void)
{
    g_screen = UI_SCREEN_MAIN;
    g_menu_index = 0U;
    handle_button_action(BUTTON_ID_BOOT, 1U, false);
    assert(g_screen == UI_SCREEN_SETTINGS);

    /* Set-screen BOOT always cancels editing instead of committing it. */
    g_screen = UI_SCREEN_TEMP_SET;
    g_edit_mode = true;
    handle_button_action(BUTTON_ID_BOOT, 1U, false);
    assert(g_screen == UI_SCREEN_SETTINGS);
    assert(!g_edit_mode);

    /* The set flow reaches both confirmation screens before any commit path. */
    g_screen = UI_SCREEN_TEMP_SET;
    g_edit_mode = true;
    handle_button_action(BUTTON_ID_OK, 1U, false);
    assert(g_screen == UI_SCREEN_CONFIRM_1);
    handle_button_action(BUTTON_ID_UP, 1U, false);
    handle_button_action(BUTTON_ID_OK, 1U, false);
    assert(g_screen == UI_SCREEN_CONFIRM_2);
    handle_button_action(BUTTON_ID_BOOT, 1U, false);
    assert(g_screen == UI_SCREEN_SETTINGS);

    assert(hold_multiplier(0U) == 1U);
    assert(hold_multiplier(BUTTON_HOLD_LEVEL_1_MS) == 2U);
    assert(hold_multiplier(BUTTON_HOLD_LEVEL_2_MS) == 5U);
    assert(hold_multiplier(BUTTON_HOLD_LEVEL_3_MS) == 10U);
    assert(hold_multiplier(BUTTON_HOLD_LEVEL_4_MS) == 20U);
    assert(hold_multiplier(BUTTON_HOLD_LEVEL_5_MS) == 50U);

    /* Alarm UI navigation cannot modify the alarm/interlock state. */
    g_screen = UI_SCREEN_CURRENT_ALERT;
    g_alarm_type = ALARM_CURRENT;
    handle_button_action(BUTTON_ID_OK, 1U, false);
    assert(g_alarm_type == ALARM_CURRENT);
}

int main(void)
{
    test_relay_interlocks();
    test_eeprom_recovery();
    test_eeprom_retry_and_safety();
    test_acs_window();
    test_oled_defers_during_acs_window();
    test_irq_is_flag_only();
    test_i2c_irq_is_eeprom_flag_only();
    test_button_safety_paths();
    return 0;
}

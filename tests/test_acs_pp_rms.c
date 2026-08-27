#include <assert.h>
#include <stdint.h>

/* Source is included so the host test exercises the production conversion
 * helper, including its configured sample/presence safety checks. */
#include "../src/motor_ui.c"

int main(void)
{
    uint16_t current_x100 = 0U;

    /* Sabit ADC: Vpp = 0, dolayisiyla akim = 0. */
    assert(acs_pp_to_current_x100(2000U, 2000U, 80U, 80U,
                                  &current_x100));
    assert(current_x100 == 0U);

    /* 1000 ADC sayimi p-p: 3300/4095 * 1000 / 2 * 0.707 / 100mV/A
     * = yaklasik 2.85A. */
    assert(acs_pp_to_current_x100(1500U, 2500U, 80U, 80U,
                                  &current_x100));
    assert(current_x100 == 285U);

    /* 0.45A ve alti bastirilir. */
    assert(acs_pp_to_current_x100(2000U, 2100U, 80U, 80U,
                                  &current_x100));
    assert(current_x100 == 0U);

    /* Yetersiz/gecersiz pencere sensor hatasina donusmek uzere reddedilir. */
    assert(!acs_pp_to_current_x100(1500U, 2500U, 79U, 79U,
                                   &current_x100));
    assert(!acs_pp_to_current_x100(1500U, 2500U, 80U, 39U,
                                   &current_x100));
    return 0;
}

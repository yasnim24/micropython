/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 - 2018 Glenn Ruben Bakke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// includes <mpconfigport.h> -> "bluetooth_conf.h"
#include "py/mpconfig.h"

#if BLUETOOTH_SD

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "nrf_sdm.h"
#include "flash.h" // flash_operation_finished
#include "usb_cdc.h" // usb_cdc_sd_event_handler
#include "ble_drv.h" // ble_drv_irq_handler()
#include "modules/bluetooth/modbluetooth.h" // modbluetooth_irq_handler()

#include "softdevice_drv.h"

#if (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)
#include "nrf_nvic.h"
nrf_nvic_state_t nrf_nvic_state = {{0}, 0};
#endif


#define DEBUG_printf(...) // printf("modbluetooth: " __VA_ARGS__)

#if (BLUETOOTH_SD == 110)
void softdevice_assert_handler(uint32_t pc, uint16_t line_number, const uint8_t * p_file_name) {
    DEBUG_printf("SoftDevice ASSERTION FAILED: pc: %04x, line_number: %u, file_name: %s", pc, line_number, p_file_name);
}
#else
void softdevice_assert_handler(uint32_t id, uint32_t pc, uint32_t info) {
    DEBUG_printf("SoftDevice ASSERTION FAILED: id: %04lx, pc: %04lx, info: %04lx", id, pc, info);
}
#endif

uint32_t softdevice_drv_enable(void) {

#if (BLUETOOTH_SD == 110)
  #if BLUETOOTH_LFCLK_RC
    uint32_t err_code = sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_RC_250_PPM_250MS_CALIBRATION,
                                             softdevice_assert_handler);
  #else
    uint32_t err_code = sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM,
                                             softdevice_assert_handler);
  #endif // BLUETOOTH_LFCLK_RC
#endif // (BLUETOOTH_SD == 110)

#if (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)
  #if BLUETOOTH_LFCLK_RC
    nrf_clock_lf_cfg_t clock_config = {
        .source = NRF_CLOCK_LF_SRC_RC,
        .rc_ctiv = 16,
        .rc_temp_ctiv = 2,
        .accuracy = NRF_CLOCK_LF_ACCURACY_250_PPM
    };
  #else
    nrf_clock_lf_cfg_t clock_config = {
        .source = NRF_CLOCK_LF_SRC_XTAL,
        .rc_ctiv = 0,
        .rc_temp_ctiv = 0,
        .accuracy = NRF_CLOCK_LF_ACCURACY_20_PPM
    };
  #endif // BLUETOOTH_LFCLK_RC

    uint32_t err_code = sd_softdevice_enable(&clock_config,
                                             softdevice_assert_handler);
#endif // (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)

    DEBUG_printf("SoftDevice enable status: %lu\n", err_code);

    if(err_code == NRF_SUCCESS) {
        err_code = sd_nvic_EnableIRQ(SD_EVT_IRQn);
    }

    DEBUG_printf("IRQ enable status: %lu\n", err_code);

    return err_code;
}

void softdevice_drv_disable(void) {
    (void)sd_softdevice_disable();
}

bool softdevice_drv_enabled(void) {
    uint8_t is_enabled;
    (void)sd_softdevice_is_enabled(&is_enabled);

    DEBUG_printf("softdevice_drv_enabled: %u\n", (uint16_t)is_enabled);

    return is_enabled ? true : false;
}

static void softdevice_evt_handler(uint32_t evt_id) {

#if MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE || MICROPY_MBFS
    // Forward SD event to flash driver.
    switch (evt_id) {
        case NRF_EVT_FLASH_OPERATION_SUCCESS:
            flash_operation_finished(FLASH_STATE_SUCCESS);
            break;
        case NRF_EVT_FLASH_OPERATION_ERROR:
            flash_operation_finished(FLASH_STATE_ERROR);
            break;
        default:
            // unhandled event!
            break;
    }
#endif // MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE || MICROPY_MBFS

#if MICROPY_HW_USB_CDC
    // Forward SD event to USB CDC driver.
    usb_cdc_sd_event_handler(evt_id);
#endif
}

#ifdef NRF51
void SWI2_IRQHandler(void) {
#else
void SWI2_EGU2_IRQHandler(void) {
#endif

    uint32_t evt_id;
    while (sd_evt_get(&evt_id) != NRF_ERROR_NOT_FOUND) {
        softdevice_evt_handler(evt_id);
    }

#if BLUETOOTH_BLE_DRV
    // Forward IRQ to BLE driver.
    ble_drv_irq_handler(evt_id);
#endif

#if MICROPY_PY_BLUETOOTH
    // Forward IRQ to bluetooth module.
    modbluetooth_irq_handler();
#endif
}

#endif // BLUETOOTH_SD

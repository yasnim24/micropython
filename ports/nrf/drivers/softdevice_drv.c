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
#include "nrf_soc.h" // NRF_EVT_FLASH_OPERATION_*

#if USE_NRF_SDK_COMPONENTS
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"

#if NRF_MODULE_ENABLED(NRF_LOG)
#include "nrf_log_ctrl.h"
#endif

#else // #if USE_NRF_SDK_COMPONENTS
#include "nrf_sdm.h"
#endif // #if USE_NRF_SDK_COMPONENTS

#include "flash.h" // flash_operation_finished
#include "usb_cdc.h" // usb_cdc_sd_event_handler
#include "ble_drv.h" // ble_drv_irq_handler()
#include "modules/bluetooth/modbluetooth.h" // modbluetooth_irq_handler()

#include "softdevice_drv.h"

#if !USE_NRF_SDK_COMPONENTS && ((BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140))
#include "nrf_nvic.h"
nrf_nvic_state_t nrf_nvic_state = {{0}, 0};
#endif


#define DEBUG_printf(...) // printf("softdevice_drv: " __VA_ARGS__)


#if USE_NRF_SDK_COMPONENTS
// Prevent parentheses around values because otherwise the macros will fail.
// TODO: Experimental
//#define BLE_OBSERVER_PRIO 3
// See also NRF_BLE_GATT_BLE_OBSERVER_PRIO in nRF5 SDK 17.1.0 examples ble_app_multirole_lesc.
#define BLE_OBSERVER_PRIO 1
#define SOC_OBSERVER_PRIO 1
#endif


#if (BLUETOOTH_SD == 110) && !USE_NRF_SDK_COMPONENTS
void app_error_fault_handler(uint32_t pc, uint16_t line_number, const uint8_t * p_file_name) {
    DEBUG_printf("SoftDevice ASSERTION FAILED: pc: %04x, line_number: %u, file_name: %s", pc, line_number, p_file_name);
}
#else
void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info) {
    DEBUG_printf("SoftDevice ASSERTION FAILED: id: %04lx, pc: %04lx, info: %04lx", id, pc, info);
}
#endif


static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context) {

#if BLUETOOTH_BLE_DRV
    // Forward IRQ to BLE driver.
    ble_drv_ble_evt_handler(p_ble_evt);
#endif

#if MICROPY_PY_BLUETOOTH
    // Forward IRQ to bluetooth module.
    modbluetooth_ble_evt_handler(p_ble_evt);
#endif
}


static void soc_evt_handler(uint32_t evt_id, void *p_context) {

#if MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE || MICROPY_MBFS
    // Forward SD event to flash driver.
    switch (evt_id) {
        case NRF_EVT_FLASH_OPERATION_SUCCESS:
            // TODO: Use NRF_SDH_SOC_LOG_ENABLED instead.
            DEBUG_printf("soc_evt_handler(NRF_EVT_FLASH_OPERATION_SUCCESS)\n");
            flash_operation_finished(FLASH_STATE_SUCCESS);
            break;
        case NRF_EVT_FLASH_OPERATION_ERROR:
            // TODO: Use NRF_SDH_SOC_LOG_ENABLED instead.
            DEBUG_printf("soc_evt_handler(NRF_EVT_FLASH_OPERATION_ERROR)\n");
            flash_operation_finished(FLASH_STATE_ERROR);
            break;
        default:
            break;
    }
#endif // MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE || MICROPY_MBFS

#if MICROPY_HW_USB_CDC
    // Forward SD event to USB CDC driver.
    usb_cdc_sd_event_handler(evt_id);
#endif
}


#if USE_NRF_SDK_COMPONENTS

// Register handlers for BLE and SoC events.
NRF_SDH_BLE_OBSERVER(m_softdevice_drv_ble_observer, BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
NRF_SDH_SOC_OBSERVER(m_softdevice_drv_soc_observer, SOC_OBSERVER_PRIO, soc_evt_handler, NULL);

uint32_t softdevice_drv_enable(void) {

    #if NRF_MODULE_ENABLED(NRF_LOG)
    uint32_t log_init_err_code = NRF_LOG_INIT(NULL);
    DEBUG_printf("NRF_LOG_INIT(): 0x%lx\n", log_init_err_code);
    (void)log_init_err_code;
    #endif // NRF_MODULE_ENABLED(NRF_LOG)

    // app_error_fault_handler() is hard coded into nrf_sdh_enable_request().
    uint32_t err_code = nrf_sdh_enable_request();

    DEBUG_printf("nrf_sdh_enable_request() result: 0x%lx\n", err_code);

    return err_code;
}

uint32_t softdevice_drv_disable(void) {

    uint32_t err_code = nrf_sdh_disable_request();

    DEBUG_printf("nrf_sdh_disable_request() result: 0x%lx\n", err_code);

    return err_code;
}

bool softdevice_drv_enabled(void) {

    bool result = nrf_sdh_is_enabled();

    DEBUG_printf("softdevice_drv_enabled: %u\n", (uint16_t)result);

    return result;
}


#else // #if USE_NRF_SDK_COMPONENTS


uint32_t softdevice_drv_enable(void) {

#if (BLUETOOTH_SD == 110)
  #if BLUETOOTH_LFCLK_RC
    uint32_t err_code = sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_RC_250_PPM_250MS_CALIBRATION,
                                             app_error_fault_handler);
  #else
    uint32_t err_code = sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM,
                                             app_error_fault_handler);
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
                                             app_error_fault_handler);
#endif // (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)

    DEBUG_printf("SoftDevice enable status: %lu\n", err_code);

    if(err_code == NRF_SUCCESS) {
        err_code = sd_nvic_EnableIRQ(SD_EVT_IRQn);
    }

    DEBUG_printf("IRQ enable status: %lu\n", err_code);

    return err_code;
}

uint32_t softdevice_drv_disable(void) {
    uint32_t err_code = sd_softdevice_disable();

    return err_code;
}

bool softdevice_drv_enabled(void) {
    uint8_t is_enabled;
    (void)sd_softdevice_is_enabled(&is_enabled);

    //DEBUG_printf("softdevice_drv_enabled: %u\n", (uint16_t)is_enabled);

    return is_enabled ? true : false;
}


#if (BLUETOOTH_SD == 110) || !MICROPY_PY_BLUETOOTH
static uint8_t m_ble_evt_buf[sizeof(ble_evt_t) + (NRF_SDH_BLE_GATT_MAX_MTU_SIZE)] __attribute__ ((aligned(4)));
#else // #if (BLUETOOTH_SD == 110) || !MICROPY_PY_BLUETOOTH
#define BLE_EVT_BUF_SIZE BLE_EVT_LEN_MAX(NRF_SDH_BLE_GATT_MAX_MTU_SIZE)
static uint8_t m_ble_evt_buf[BLE_EVT_BUF_SIZE] __attribute__ ((aligned(BLE_EVT_PTR_ALIGNMENT)));
#endif // #if (BLUETOOTH_SD == 110) || !MICROPY_PY_BLUETOOTH

#ifdef NRF51
void SWI2_IRQHandler(void) {
#else
void SWI2_EGU2_IRQHandler(void) {
#endif

    uint32_t evt_id;
    while (sd_evt_get(&evt_id) != NRF_ERROR_NOT_FOUND) {
        soc_evt_handler(evt_id, NULL);
    }

    while (1) {
        uint16_t evt_len  = sizeof(m_ble_evt_buf);
        uint32_t err_code = sd_ble_evt_get(m_ble_evt_buf, &evt_len);

        if (err_code != NRF_SUCCESS) {
            // Possible error conditions:
            //  * NRF_ERROR_NOT_FOUND: no events left, break
            //  * NRF_ERROR_DATA_SIZE: retry with a bigger data buffer
            //    (currently not handled, TODO)
            //  * NRF_ERROR_INVALID_ADDR: pointer is not aligned, should
            //    not happen.
            // In all cases, it's best to simply stop now.
            if (err_code == NRF_ERROR_DATA_SIZE) {
                DEBUG_printf("NRF_ERROR_DATA_SIZE\n");
            }
            break;
        }

        ble_evt_handler((ble_evt_t *)m_ble_evt_buf, NULL);
    }
}

#endif // #if USE_NRF_SDK_COMPONENTS

#endif // BLUETOOTH_SD

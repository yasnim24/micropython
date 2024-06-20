/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 - 2018 Glenn Ruben Bakke
 * Copyright (c) 2019-2020 Jim Mussared
 * Copyright (c) 2024 Yasmin Bosch
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


#if MICROPY_PY_BLUETOOTH
#if (BLUETOOTH_SD == 110) && USE_NRF_SDK_COMPONENTS
#error "SoftDevice 110 not supported with USE_NRF_SDK_COMPONENTS."
#endif

#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING && !USE_NRF_SDK_COMPONENTS
#error "MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING requires USE_NRF_SDK_COMPONENTS."
#endif

#if MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE || MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT
#if (BLUETOOTH_SD != 132) && (BLUETOOTH_SD != 140)
#error "MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE and MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT are only supported for SD 132 and SD 140."
#endif
#endif

#if MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS
#error "MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS is not supported."
#endif

#if MICROPY_PY_BLUETOOTH_ENABLE_HCI_CMD
#error "MICROPY_PY_BLUETOOTH_ENABLE_HCI_CMD is not supported."
#endif


#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#if USE_NRF_SDK_COMPONENTS
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_strerror.h" // nrf_strerror_get
#else // USE_NRF_SDK_COMPONENTS
#include "nrf_sdm.h"
#endif // USE_NRF_SDK_COMPONENTS


#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

#include "peer_manager.h"
#include "peer_manager_handler.h"

#if NRF_MODULE_ENABLED(NRF_BLE_LESC)

#include "nrf_ble_lesc.h" // nrf_ble_lesc_request_handler

#endif // NRF_MODULE_ENABLED(NRF_BLE_LESC)
#endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING


#include "nrf_error.h"
#include "ble_gap.h"
#include "ble.h" // sd_ble_uuid_encode

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#include "softdevice_drv.h"
#include "extmod/modbluetooth.h"
#include "modbluetooth.h"

// Set DEBUG_MODBLUETOOTH for debug output.
//
// ATTENTION
// ---------
// With debug output the changed timing could result in wrong behavior.
// This has been seen e.g. with BLE_GAP_EVT_ADV_REPORT - see there.

// #define DEBUG_MODBLUETOOTH (1)

#ifndef DEBUG_MODBLUETOOTH
#define DEBUG_MODBLUETOOTH (0)
#endif

#if DEBUG_MODBLUETOOTH
#define DEBUG_printf(...)  printf("modbluetooth: " __VA_ARGS__)
#else
#define DEBUG_printf(...)
#endif


#if !USE_NRF_SDK_COMPONENTS
#define MSEC_TO_UNITS(TIME_MS, RESOLUTION) (((TIME_MS) * 1000) / (RESOLUTION))
#endif

#define USEC_TO_UNITS(TIME_US, RESOLUTION) ((TIME_US) / (RESOLUTION))
#define UNIT_625_US (625)
#define UNIT_1250_US (1250)
#define UNIT_10_MS (10000)


#define ERRNO_BLUETOOTH_NOT_ACTIVE MP_ENODEV


// Maximum size of ATT packet that can be sent or received.
//
// Note:
// This is the maximum value that can be set with
// mp_bluetooth_set_preferred_mtu().
#ifndef NRF_SDH_BLE_GATT_MAX_MTU_SIZE

#error "NRF_SDH_BLE_GATT_MAX_MTU_SIZE not set."
#else // NRF_SDH_BLE_GATT_MAX_MTU_SIZE

#if (BLUETOOTH_SD == 110) && (NRF_SDH_BLE_GATT_MAX_MTU_SIZE != BLE_GATT_ATT_MTU_DEFAULT)
#error "NRF_SDH_BLE_GATT_MAX_MTU_SIZE have to be BLE_GATT_ATT_MTU_DEFAULT for SD110."
#endif

#if NRF_SDH_BLE_GATT_MAX_MTU_SIZE < BLE_GATT_ATT_MTU_DEFAULT
#error "NRF_SDH_BLE_GATT_MAX_MTU_SIZE is less than BLE_GATT_ATT_MTU_DEFAULT."
#endif

#endif // NRF_SDH_BLE_GATT_MAX_MTU_SIZE


#ifndef MICROPY_PY_BLUETOOTH_DEFAULT_GAP_NAME
#define MICROPY_PY_BLUETOOTH_DEFAULT_GAP_NAME "MPY NRF"
#endif

#ifndef MICROPY_PY_BLUETOOTH_MAX_GAP_NAME_LEN
#define MICROPY_PY_BLUETOOTH_MAX_GAP_NAME_LEN 32
#endif

#if MICROPY_PY_BLUETOOTH_MAX_GAP_NAME_LEN > BLE_GAP_DEVNAME_MAX_LEN
#error "MICROPY_PY_BLUETOOTH_MAX_GAP_NAME_LEN is greater than BLE_GAP_DEVNAME_MAX_LEN."
#endif

#ifndef MICROPY_PY_BLUETOOTH_MIN_CONN_INTERVAL
// Values taken from modbluetooth_btstack.c - mp_bluetooth_gap_peripheral_connect()
#define MICROPY_PY_BLUETOOTH_MIN_CONN_INTERVAL MSEC_TO_UNITS(10, UNIT_1250_US)
#endif

#ifndef MICROPY_PY_BLUETOOTH_MAX_CONN_INTERVAL
// Value taken from modbluetooth_btstack.c - mp_bluetooth_gap_peripheral_connect()
#define MICROPY_PY_BLUETOOTH_MAX_CONN_INTERVAL MSEC_TO_UNITS(30, UNIT_1250_US)
#endif

#if MICROPY_PY_BLUETOOTH_MIN_CONN_INTERVAL > MICROPY_PY_BLUETOOTH_MAX_CONN_INTERVAL
#error "MICROPY_PY_BLUETOOTH_MIN_CONN_INTERVAL is greater than MICROPY_PY_BLUETOOTH_MAX_CONN_INTERVAL."
#endif

// Peripheral Latency:
// Skip that number of connection events when there is no data to send.
#ifndef MICROPY_PY_BLUETOOTH_CONN_SLAVE_LATENCY
// Value taken from modbluetooth_btstack.c - mp_bluetooth_gap_peripheral_connect()
#define MICROPY_PY_BLUETOOTH_CONN_SLAVE_LATENCY 4
#endif

#ifndef MICROPY_PY_BLUETOOTH_CONN_SUP_TIMEOUT
#define MICROPY_PY_BLUETOOTH_CONN_SUP_TIMEOUT MSEC_TO_UNITS(4000, UNIT_10_MS)
#endif

// Default length of a Characteristic Attribute value.
#ifndef MP_BLUETOOTH_DEFAULT_CHAR_ATTR_LEN
#define MP_BLUETOOTH_DEFAULT_CHAR_ATTR_LEN MP_BLUETOOTH_DEFAULT_ATTR_LEN
#endif

// Maximum length of a Characteristic Attribute value.
#ifndef MP_BLUETOOTH_MAX_CHAR_ATTR_LEN
#define MP_BLUETOOTH_MAX_CHAR_ATTR_LEN MP_BLUETOOTH_DEFAULT_CHAR_ATTR_LEN
#endif

#if MP_BLUETOOTH_MAX_CHAR_ATTR_LEN < MP_BLUETOOTH_DEFAULT_CHAR_ATTR_LEN
#error "MP_BLUETOOTH_MAX_CHAR_ATTR_LEN is less than MP_BLUETOOTH_DEFAULT_CHAR_ATTR_LEN."
#endif

// Default length of a Descriptor Attribute value.
#ifndef MP_BLUETOOTH_DEFAULT_DESC_ATTR_LEN
#define MP_BLUETOOTH_DEFAULT_DESC_ATTR_LEN MP_BLUETOOTH_DEFAULT_ATTR_LEN
#endif

// Maximum length of a Descriptor Attribute value.
#ifndef MP_BLUETOOTH_MAX_DESC_ATTR_LEN
#define MP_BLUETOOTH_MAX_DESC_ATTR_LEN MP_BLUETOOTH_DEFAULT_DESC_ATTR_LEN
#endif

#if MP_BLUETOOTH_MAX_DESC_ATTR_LEN < MP_BLUETOOTH_DEFAULT_DESC_ATTR_LEN
#error "MP_BLUETOOTH_MAX_DESC_ATTR_LEN is less than MP_BLUETOOTH_DEFAULT_DESC_ATTR_LEN."
#endif


#define BLE_INVALID_CONN_HANDLE = (0xffff)

#define BLE_MAX_CONN_COUNT           2

#if (BLUETOOTH_SD == 110)
  #define HVN_TX_QUEUE_SIZE         (6)
#else
  #define HVN_TX_QUEUE_SIZE         (10)
#endif

extern uint32_t _ram_start;

typedef struct _mp_bluetooth_nrf_root_pointers_t {
    // Device name buffer for mp_bluetooth_gap_get_device_name().
    uint8_t *device_name;
    // Allocated length of device_name.
    size_t device_name_alloc;
    // Current length of device name.
    size_t device_name_len;

    // Advertising data set by mp_bluetooth_gap_advertise_start().
    uint8_t *adv_data;
    // Allocated length of adv_data.
    size_t adv_data_alloc;
    // Temporary pointer to new advertising data when updating an
    // ongoing advertisement.
    // It prevents that adv_data is garbage collected to early.
    uint8_t *new_adv_data;

    // Scan response data set by mp_bluetooth_gap_advertise_start().
    uint8_t *sr_data;
    // Allocated length of sr_data.
    size_t sr_data_alloc;
    // Temporary pointer to new scan response data when updating an
    // ongoing advertisement.
    // It prevents that sr_data is garbage collected to early.
    uint8_t *new_sr_data;

    // Buffer for mp_bluetooth_gatts_read().
    uint8_t *gatts_read_buffer;
    // Allocated length of gatts_read_buffer.
    size_t gatts_read_buffer_alloc;

    // The SoftDevice doesn't allow to change the pointer to the attribute
    // value storage even if we use BLE_GATTS_VLOC_USER as value location.
    // So we use BLE_GATTS_VLOC_STACK as value location and live with the
    // constraint that mp_bluetooth_gatts_set_buffer() could not be supported.
//    mp_gatts_db_t gatts_db;
} mp_bluetooth_nrf_root_pointers_t;

// Info structure for a connection.
typedef struct {
    // Role of this device from BLE_GAP_EVT_CONNECTED.
    // BLE_GAP_ROLE_INVALID (0): not connected
    // BLE_GAP_ROLE_PERIPH: This device is in the peripheral role.
    // BLE_GAP_ROLE_CENTRAL: This device is in the central role.
    uint8_t role;

    // Service UUID that has been supplied to
    // mp_bluetooth_gattc_discover_primary_services().
    // 'type' is BLE_UUID_TYPE_UNKNOWN (0) if no UUID has been supplied
    // or no primary service discovery is ongoing.
    ble_uuid_t service_discovery_uuid;

    // Characteristic UUID that has been supplied to
    // mp_bluetooth_gattc_discover_characteristics().
    // 'type' is BLE_UUID_TYPE_UNKNOWN (0) if no UUID has been supplied
    // or no characteristic discovery is ongoing.
    ble_uuid_t characteristic_discovery_uuid;

    // Handle range that has been supplied to
    // sd_ble_gattc_characteristics_discover().
    // 'start_handle' and 'end_handle' are 0 if no characteristics discover is
    // ongoing.
    ble_gattc_handle_range_t char_discovery_range;

    // Handle range that has been supplied to
    // sd_ble_gattc_descriptors_discover().
    // 'start_handle' and 'end_handle' are 0 if no characteristics discover is
    // ongoing.
    ble_gattc_handle_range_t desc_discovery_range;

} m_connection_info_t;


// Info per connection handle.
//
// Index:
//   Connection handle.
//   See
//   "https://devzone.nordicsemi.com/f/nordic-q-a/41946/what-is-the-point-of-connection-handles":
//   "... E.g. if you are connected to four different central devices, their
//    connection handles will be assigned the values 0x0, 0x1, 0x2 and 0x3 respectively. ..."
//
// Item:
//   Info structure.
static m_connection_info_t m_connection_handle_info[BLE_MAX_CONN_COUNT];


// Structure for variable scan parameters.
//
// This structure contains the members of ble_gap_scan_params_t that could be set
// by arguments of mp_bluetooth_gap_scan_start().
typedef struct {
    // 1: A scan started by mp_bluetooth_gap_scan_start() is in progress.
    // 0: Otherwise.
    uint8_t scan_in_progress : 1;

    // The following struct members are from ble_gap_scan_params_t.
    uint8_t active : 1;
    uint16_t interval;
    uint16_t window;
    uint16_t timeout;
} m_variable_scan_params_t;

// Variable scan parameters - see m_variable_scan_params_t.
static m_variable_scan_params_t m_variable_scan_params;

// Structure for static flags.
// It contains bit fields to save memory space.
typedef struct {
    uint8_t ble_enabled : 1;
    uint8_t adv_in_progress : 1;
    uint8_t connect_in_progress : 1;
    uint8_t services_registered : 1;

    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
    // See mp_bluetooth_set_bonding().
    uint8_t bonding : 1;
    // See mp_bluetooth_set_mitm_protection().
    uint8_t mitm_protection : 1;
    // See mp_bluetooth_set_le_secure().
    uint8_t le_secure : 1;
    // See mp_bluetooth_set_io_capability().
    uint8_t io_capability : 3;

    #if MICROPY_PY_BLUETOOTH_EXTENDED
    // See mp_bluetooth_set_force_pairing().
    uint8_t force_pairing : 1;
    // See mp_bluetooth_set_privacy().
    uint8_t privacy_mode : 3;

    // mp_bluetooth_delete_bonds() is in progress.
    uint8_t delete_bonds_in_progress : 1;
    #endif // MICROPY_PY_BLUETOOTH_EXTENDED

    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
} m_static_flags_t;

// Static flags - see m_static_flags_t.
static m_static_flags_t m_static_flags;


#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

// Structure to hold Peer Manager related state.
// Please note that the Peer Manager could not be de-initialized.
// Therefore this state will not be reset once it was set.
typedef struct {
    // Flag: pm_init() has been called successfully.
    uint8_t is_peer_manager_initialized : 1;
    // Flag: peer_manager_event_handler() is registered.
    uint8_t is_peer_manager_event_handler_registered : 1;
}m_peer_manager_state_t;

static m_peer_manager_state_t m_peer_manager_state;


// Key lengths per connection handle.
//
// Index:
//   Connection handle.
//   See
//   "https://devzone.nordicsemi.com/f/nordic-q-a/41946/what-is-the-point-of-connection-handles":
//   "... E.g. if you are connected to four different central devices, their
//    connection handles will be assigned the values 0x0, 0x1, 0x2 and 0x3 respectively. ..."
//
// Item:
//   Key length set from BLE_GAP_EVT_CONN_SEC_UPDATE.
//   0 if BLE_GAP_EVT_CONN_SEC_UPDATE has not been triggered.
static uint8_t m_connection_handle_key_lengths[BLE_MAX_CONN_COUNT];

#endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING


// Value used by mp_bluetooth_get_preferred_mtu(), mp_bluetooth_set_preferred_mtu(),
// mp_bluetooth_gattc_exchange_mtu(), mp_bluetooth_init().
//
// The maximum allowed value is NRF_SDH_BLE_GATT_MAX_MTU_SIZE
static uint16_t m_prefered_mtu = BLE_GATT_ATT_MTU_DEFAULT;

// hvx-types used by mp_bluetooth_gatts_write()
static const uint8_t m_hvx_types[] = {BLE_GATT_HVX_NOTIFICATION, BLE_GATT_HVX_INDICATION};


#if (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)
#define BLE_MODBLUETOOTH_CONN_CONFIG_TAG 2
static uint8_t m_adv_handle;
// BLE_GAP_SCAN_BUFFER_MIN and BLE_GAP_SCAN_BUFFER_MAX are both 31
static uint8_t m_scan_buffer[BLE_GAP_SCAN_BUFFER_MIN];
#endif // (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)


// Convert a NRF_ERROR_* value to a MP_* errno value.
static int nrf_error_to_errno(int nrf_error) {
    switch (nrf_error) {
        case NRF_SUCCESS:
            return 0;
        case NRF_ERROR_NO_MEM:
            return MP_ENOMEM;
        case NRF_ERROR_NOT_SUPPORTED:
            return MP_EOPNOTSUPP;
        case NRF_ERROR_INVALID_PARAM:
        case NRF_ERROR_INVALID_LENGTH:
        case NRF_ERROR_INVALID_FLAGS:
        case NRF_ERROR_INVALID_DATA:
        case NRF_ERROR_DATA_SIZE:
            return MP_EINVAL;
        case NRF_ERROR_NULL:
        case NRF_ERROR_INVALID_ADDR:
            return MP_EFAULT;
        case NRF_ERROR_TIMEOUT:
            return MP_ETIMEDOUT;
        case NRF_ERROR_FORBIDDEN:
            return MP_EPERM;
        case NRF_ERROR_BUSY:
            return MP_EBUSY;
        case NRF_ERROR_RESOURCES:
        case NRF_ERROR_CONN_COUNT:
            return MP_ENOSPC;
        case NRF_ERROR_SOFTDEVICE_NOT_ENABLED:
            return ERRNO_BLUETOOTH_NOT_ACTIVE;
        case NRF_ERROR_SVC_HANDLER_MISSING:
        case NRF_ERROR_INTERNAL:
        case NRF_ERROR_NOT_FOUND:
        case NRF_ERROR_INVALID_STATE:
            return MP_EIO;
        default:
            // BLE_ERROR_INVALID_*_HANDLE
            // BLE_ERROR_INVALID_ROLE
            // BLE_ERROR_NOT_ENABLED
            return MP_EINVAL;
    }
}

// Resize an allocated data buffer if needed.
//
// Note that the data buffer pointer should be a part of a
// _mp_bluetooth_nrf_root_pointers_t struct in order to be
// garbage collected when resized.
//
// len_needed:
// Minimum needed length of the allocated data buffer.
//
// data:
// *data is a pointer to the allocated data buffer.
// It could be NULL if *alloc_len is 0.
// It will be set to a new pointer if the data buffer has been resized.
//
// alloc_len:
// *alloc_len is the allocated length of the data buffer.
// It will be set to a new length if the data buffer has been resized.
//
// Result:
// true if the data buffer has been resized i.e. if *data and *alloc_len
// has been changed.
// false otherwise.
static bool resize_data_alloc_if_needed(size_t len_needed, uint8_t **data, size_t *alloc_len) {
    bool result = false;

    if (len_needed > *alloc_len) {
        // Resize data buffer.
        *data = m_new(uint8_t, len_needed);
        *alloc_len = len_needed;

        result = true;
    }

    return result;
}

// Copy a byte array in reverse order to another byte array.
void reverse_bytes(const uint8_t *source_bytes, uint8_t *target_bytes, size_t byte_count) {
    for (size_t target_index = 0; target_index < byte_count; target_index++) {
        target_bytes[target_index] = source_bytes[byte_count - target_index - 1];
    }
}

// Convert a MP bluetooth address including address type to a ble_gap_addr_t.
//
// mp_addr_type:
//   MP bluetooth address type.
// mp_addr:
//   Pointer to MP bluetooth address array.
//   Its length shall be 'sizeof(ble_gap_addr->addr)' i.e. 6.
// ble_gap_addr:
//   Pointer to the target ble_gap_addr_t structure.
void mp_addr_to_ble_gap_addr(uint8_t mp_addr_type, const uint8_t *mp_addr, ble_gap_addr_t *ble_gap_addr) {
    ble_gap_addr->addr_id_peer = 0;
    ble_gap_addr->addr_type = mp_addr_type;
    reverse_bytes(mp_addr, ble_gap_addr->addr, sizeof(ble_gap_addr->addr));
}

// Convert a ble_gap_addr_t to a MP bluetooth address including address type.
//
// ble_gap_addr:
//   Pointer to the source ble_gap_addr_t structure.
// mp_addr_type:
//   Pointer to target MP bluetooth address type.
// mp_addr:
//   Pointer to target MP bluetooth address array.
//   Its length shall be 'sizeof(ble_gap_addr->addr)' i.e. 6.
void ble_gap_addr_to_mp_addr(const ble_gap_addr_t *ble_gap_addr, uint8_t *mp_addr_type, uint8_t *mp_addr) {
    *mp_addr_type = ble_gap_addr->addr_type;
    reverse_bytes(ble_gap_addr->addr, mp_addr, sizeof(ble_gap_addr->addr));
}

// Initialize device name in 'bluetooth_nrf_root_pointers'.
void initialize_device_name_in_root_pointers(size_t device_name_len, const uint8_t *device_name, size_t min_device_name_alloc) {
    uint8_t **pp_device_name = &(MP_STATE_PORT(bluetooth_nrf_root_pointers)->device_name);
    size_t *p_device_name_alloc = &(MP_STATE_PORT(bluetooth_nrf_root_pointers)->device_name_alloc);

    size_t alloc_needed = MAX(device_name_len, min_device_name_alloc);

    resize_data_alloc_if_needed(alloc_needed, pp_device_name, p_device_name_alloc);

    memcpy(*pp_device_name, device_name, device_name_len);
    MP_STATE_PORT(bluetooth_nrf_root_pointers)->device_name_len = device_name_len;
}


#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

// get ble_gap_sec_params_t according to 'm_static_flags'.
static void get_ble_gap_sec_params(ble_gap_sec_params_t *p_sec_params) {

    memset(p_sec_params, 0, sizeof(ble_gap_sec_params_t));

    p_sec_params->bond = m_static_flags.bonding;
    p_sec_params->mitm = m_static_flags.mitm_protection;
    p_sec_params->lesc = m_static_flags.le_secure;
    p_sec_params->io_caps = m_static_flags.io_capability;

    // sec_param.keypress = 0;
    // sec_param.oob = 0;
    p_sec_params->min_key_size = 7;
    p_sec_params->max_key_size = 16;

    p_sec_params->kdist_own.enc = m_static_flags.bonding;
    p_sec_params->kdist_own.id = m_static_flags.bonding;
    p_sec_params->kdist_peer.enc = m_static_flags.bonding;
    p_sec_params->kdist_peer.id = m_static_flags.bonding;
}

// Call pm_sec_params_set() according to 'm_static_flags'.
static uint32_t set_peer_manager_sec_params(void) {

    ble_gap_sec_params_t sec_params;
    get_ble_gap_sec_params(&sec_params);

    uint32_t err_code = pm_sec_params_set(&sec_params);

    DEBUG_printf("pm_sec_params_set - " \
        "bond: %u, " \
        "mitm: %u, " \
        "lesc: %u, " \
        "io_caps: %u " \
        "- result: 0x%02lx\n",
        (uint16_t)sec_params.bond,
        (uint16_t)sec_params.mitm,
        (uint16_t)sec_params.lesc,
        (uint16_t)sec_params.io_caps,
        err_code);

    return err_code;
}

// Peer Manager event handler.
static void peer_manager_event_handler(pm_evt_t const *p_evt) {
    DEBUG_printf("peer_manager_event_handler - p_evt->evt_id: %d\n", p_evt->evt_id);

    pm_handler_on_pm_evt(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        case PM_EVT_CONN_SEC_SUCCEEDED: {
            uint16_t conn_handle = p_evt->conn_handle;

            pm_conn_sec_status_t pm_conn_sec_status;
            uint32_t err_code = pm_conn_sec_status_get(conn_handle, &pm_conn_sec_status);

            if (err_code == NRF_SUCCESS) {
                bool encrypted = pm_conn_sec_status.encrypted ? true : false;
                bool authenticated = pm_conn_sec_status.mitm_protected ? true : false;
                bool bonded = pm_conn_sec_status.bonded ? true : false;

                // It is assumed that BLE_GAP_EVT_CONN_SEC_UPDATE is handled first.
                uint8_t key_size = m_connection_handle_key_lengths[conn_handle];

                // Call this when any connection encryption has been changed (e.g. during pairing).
                mp_bluetooth_gatts_on_encryption_update(
                    conn_handle, encrypted, authenticated, bonded, key_size);
            } else {
                DEBUG_printf("pm_conn_sec_status_get() returned %s\n", nrf_strerror_get(err_code));
            }

            break;
        }

            #if MICROPY_PY_BLUETOOTH_EXTENDED

        case PM_EVT_PEERS_DELETE_SUCCEEDED: {
            m_static_flags.delete_bonds_in_progress = 0;

            mp_bluetooth_on_bonds_deleted(0);
            break;
        }
        case PM_EVT_PEERS_DELETE_FAILED: {
            m_static_flags.delete_bonds_in_progress = 0;

            const pm_failure_evt_t *p_peers_delete_failed_evt = &p_evt->params.peers_delete_failed_evt;
            DEBUG_printf("PM_EVT_PEERS_DELETE_FAILED: %s\n", nrf_strerror_get(p_peers_delete_failed_evt->error));
            int status_errno = nrf_error_to_errno(p_peers_delete_failed_evt->error);
            mp_bluetooth_on_bonds_deleted(status_errno);
            break;
        }

            #endif // MICROPY_PY_BLUETOOTH_EXTENDED

        default: {
            break;
        }
    }
}


#if NRF_MODULE_ENABLED(NRF_BLE_LESC)

// Defines how often nrf_ble_lesc_request_handler() will be called.
// See mp_bluetooth_poll_hook().
#define M_NRF_BLE_LESC_REQUEST_HANDLER_COUNT (10)

static uint8_t m_nrf_ble_lesc_request_handler_divisor = M_NRF_BLE_LESC_REQUEST_HANDLER_COUNT;

#endif // NRF_MODULE_ENABLED(NRF_BLE_LESC)


// A BLE_GAP_EVT_LESC_DHKEY_REQUEST event is pending.
static bool m_is_lesc_dhkey_request_pendig = false;

// Poll hook that will be called from mp_event_wait_indefinite().
//
// The hook is defined by the MICROPY_EVENT_POLL_HOOK macro in 'mpconfigport.h'.
void mp_bluetooth_poll_hook(void) {
    #if NRF_MODULE_ENABLED(NRF_BLE_LESC)

    if (m_static_flags.ble_enabled && m_peer_manager_state.is_peer_manager_initialized) {
        if (--m_nrf_ble_lesc_request_handler_divisor == 0 || m_is_lesc_dhkey_request_pendig) {
            m_nrf_ble_lesc_request_handler_divisor = M_NRF_BLE_LESC_REQUEST_HANDLER_COUNT;

            // nRF SDK Peer manager: handle any pending DH key requests.
            uint32_t err_code = nrf_ble_lesc_request_handler();

            if (err_code != NRF_SUCCESS || m_is_lesc_dhkey_request_pendig) {
                DEBUG_printf("nrf_ble_lesc_request_handler(): 0x%lx\n", err_code);
            }

            m_is_lesc_dhkey_request_pendig = false;
        }
    }
    #endif
}

// De-configure the Peer Manager so that security requests are rejected.
static void deconfigure_peer_manager(void) {
    DEBUG_printf("deconfigure_peer_manager\n");

    (void)pm_sec_params_set(NULL);

    DEBUG_printf("return\n");
}

#endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING


#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

static uint32_t mbt_peer_manager_init(void) {

    uint32_t err_code = NRF_SUCCESS;

    if (!m_peer_manager_state.is_peer_manager_initialized) {
        // pm_init() shall be called only once because it doesn't
        // check if it is already initialized.
        //
        // All pairing and bonding procedures that are initiated by the peer
        // are rejected until pm_sec_params_set() is called.
        err_code = pm_init();

        if (err_code == NRF_SUCCESS) {
            m_peer_manager_state.is_peer_manager_initialized = 1;
        }

        DEBUG_printf("pm_init(): 0x%lx\n", err_code);

        if (err_code == NRF_SUCCESS) {
            // It is not possible to unregister a peer manager event handler.
            // So register the event handler only once.
            if (!m_peer_manager_state.is_peer_manager_event_handler_registered) {
                err_code = pm_register(peer_manager_event_handler);

                if (err_code == NRF_SUCCESS) {
                    m_peer_manager_state.is_peer_manager_event_handler_registered = 1;
                }

                DEBUG_printf("pm_register(): 0x%lx\n", err_code);
            }
        }
    }

    return err_code;
}

#endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING


//////////////////////////////////////////////////////////////
// API implemented by ports (i.e. called from modbluetooth.c):

// Any 'mp_*' method returning an int returns errno on failure, otherwise zero.

// Note: All 'mp_*' methods dealing with addresses (as 6-byte uint8 pointers) are in big-endian format.
// (i.e. the same way they would be printed on a device sticker or in a UI), so the user sees
// addresses in a way that looks like what they'd expect.
// This means that the lower level implementation will likely need to reorder them (e.g. Nimble
// works in little-endian, as does BLE itself).

// Enables the Bluetooth stack.
int mp_bluetooth_init(void) {
    DEBUG_printf("mp_bluetooth_init\n");

    if (mp_bluetooth_is_active()) {
        // already initialized
        return 0;
    }

    // Initialize state
    MP_STATE_PORT(bluetooth_nrf_root_pointers) = m_new0(mp_bluetooth_nrf_root_pointers_t, 1);

    memset(&m_static_flags, 0, sizeof(m_static_flags));
    memset(m_connection_handle_info, 0, sizeof(m_connection_handle_info));
    memset(m_scan_buffer, 0, sizeof(m_scan_buffer));

    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
    m_static_flags.io_capability = MP_BLUETOOTH_IO_CAPABILITY_NO_INPUT_OUTPUT;
    memset(m_connection_handle_key_lengths, 0, sizeof(m_connection_handle_key_lengths));
    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

    m_prefered_mtu = BLE_GATT_ATT_MTU_DEFAULT;
    m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;

    // Initialize device name
    {
        const uint8_t *device_name = (const uint8_t *)MICROPY_PY_BLUETOOTH_DEFAULT_GAP_NAME;
        size_t device_name_len = strlen((char *)device_name);
        initialize_device_name_in_root_pointers(device_name_len, device_name, MICROPY_PY_BLUETOOTH_MAX_GAP_NAME_LEN);
    }

    uint32_t err_code = NRF_SUCCESS;

    if (!softdevice_drv_enabled()) {
        err_code = softdevice_drv_enable();
    }

    #if (BLUETOOTH_SD == 140 && defined(BLUETOOTH_SD_7_2_0))
    sd_ble_opt_set(BLE_GATTC_OPT_UUID_DISC, &(ble_opt_t) {.gattc_opt.uuid_disc.auto_add_vs_enable = 1});
    #endif

    #if (BLUETOOTH_SD == 110)
    if (err_code == NRF_SUCCESS) {
        ble_enable_params_t ble_enable_params;
        memset(&ble_enable_params, 0x00, sizeof(ble_enable_params));
        ble_enable_params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT;
        ble_enable_params.gatts_enable_params.service_changed = 0;
    }
    #else // (BLUETOOTH_SD == 110)
    ble_cfg_t ble_conf;

    const uint32_t app_ram_start_cfg = (uint32_t)&_ram_start;

    if (err_code == NRF_SUCCESS) {
        DEBUG_printf("app_ram_start_cfg: 0x%lx\n", app_ram_start_cfg);

        memset(&ble_conf, 0, sizeof(ble_conf));

        ble_conf.conn_cfg.conn_cfg_tag = BLE_MODBLUETOOTH_CONN_CONFIG_TAG;
        ble_conf.conn_cfg.params.gap_conn_cfg.conn_count = BLE_MAX_CONN_COUNT;
        ble_conf.conn_cfg.params.gap_conn_cfg.event_length = BLE_GAP_EVENT_LENGTH_DEFAULT;
        err_code = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_conf, app_ram_start_cfg);

        DEBUG_printf("sd_ble_cfg_set(BLE_CONN_CFG_GAP): %lu\n", err_code);
    }

    if (err_code == NRF_SUCCESS) {
        memset(&ble_conf, 0, sizeof(ble_conf));

        ble_conf.gap_cfg.role_count_cfg.adv_set_count = BLE_GAP_ADV_SET_COUNT_DEFAULT;
        ble_conf.gap_cfg.role_count_cfg.periph_role_count = 1;
        ble_conf.gap_cfg.role_count_cfg.central_role_count = 1;
        ble_conf.gap_cfg.role_count_cfg.central_sec_count = 1;
        // set to 0 by memset()
        // ble_conf.gap_cfg.role_count_cfg.qos_channel_survey_role_available = 0;
        err_code = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_conf, app_ram_start_cfg);

        DEBUG_printf("sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT): %lu\n", err_code);
    }

    if (err_code == NRF_SUCCESS) {
        memset(&ble_conf, 0, sizeof(ble_conf));

        uint8_t *device_name = MP_STATE_PORT(bluetooth_nrf_root_pointers)->device_name;
        size_t device_name_alloc = MP_STATE_PORT(bluetooth_nrf_root_pointers)->device_name_alloc;
        size_t device_name_len = MP_STATE_PORT(bluetooth_nrf_root_pointers)->device_name_len;

        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ble_conf.gap_cfg.device_name_cfg.write_perm);
        ble_conf.gap_cfg.device_name_cfg.vloc = BLE_GATTS_VLOC_USER;
        ble_conf.gap_cfg.device_name_cfg.p_value = device_name;
        ble_conf.gap_cfg.device_name_cfg.current_len = (uint16_t)device_name_len;
        ble_conf.gap_cfg.device_name_cfg.max_len = (uint16_t)device_name_alloc;
        err_code = sd_ble_cfg_set(BLE_GAP_CFG_DEVICE_NAME, &ble_conf, app_ram_start_cfg);

        DEBUG_printf("sd_ble_cfg_set(BLE_GAP_CFG_DEVICE_NAME): %lu\n", err_code);
    }

    if (err_code == NRF_SUCCESS) {
        memset(&ble_conf, 0, sizeof(ble_conf));

        ble_conf.conn_cfg.conn_cfg_tag = BLE_MODBLUETOOTH_CONN_CONFIG_TAG;
        ble_conf.conn_cfg.params.gatts_conn_cfg.hvn_tx_queue_size = HVN_TX_QUEUE_SIZE;
        err_code = sd_ble_cfg_set(BLE_CONN_CFG_GATTS, &ble_conf, app_ram_start_cfg);

        DEBUG_printf("sd_ble_cfg_set(BLE_CONN_CFG_GATTS): %lu\n", err_code);
    }

    if (err_code == NRF_SUCCESS) {
        // Configure the maximum ATT MTU.
        memset(&ble_conf, 0, sizeof(ble_conf));

        ble_conf.conn_cfg.conn_cfg_tag = BLE_MODBLUETOOTH_CONN_CONFIG_TAG;
        ble_conf.conn_cfg.params.gatt_conn_cfg.att_mtu = NRF_SDH_BLE_GATT_MAX_MTU_SIZE;
        err_code = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_conf, app_ram_start_cfg);

        DEBUG_printf("sd_ble_cfg_set(BLE_CONN_CFG_GATT): %lu\n", err_code);
    }

    #endif // (BLUETOOTH_SD == 110)

    #if (BLUETOOTH_SD == 110)
    if (err_code == NRF_SUCCESS) {
        err_code = sd_ble_enable(&ble_enable_params);
    }
    #else
    if (err_code == NRF_SUCCESS) {
        uint32_t app_ram_start = app_ram_start_cfg;

        #if USE_NRF_SDK_COMPONENTS
        err_code = nrf_sdh_ble_enable(&app_ram_start);
        #else // USE_NRF_SDK_COMPONENTS
        err_code = sd_ble_enable(&app_ram_start);
        #endif // USE_NRF_SDK_COMPONENTS

        DEBUG_printf("app_ram_start: 0x%lx\n", app_ram_start);
    }
    #endif

    DEBUG_printf("sd_ble_enable(): 0x%lx\n", err_code);

    if (err_code == NRF_SUCCESS) {
        m_static_flags.ble_enabled = 1;

        // set connection parameters
        // sd_ble_enable() has to be called before
        ble_gap_conn_params_t gap_conn_params;
        memset(&gap_conn_params, 0, sizeof(gap_conn_params));

        gap_conn_params.min_conn_interval = MICROPY_PY_BLUETOOTH_MIN_CONN_INTERVAL;
        gap_conn_params.max_conn_interval = MICROPY_PY_BLUETOOTH_MAX_CONN_INTERVAL;
        gap_conn_params.slave_latency = MICROPY_PY_BLUETOOTH_CONN_SLAVE_LATENCY;
        gap_conn_params.conn_sup_timeout = MICROPY_PY_BLUETOOTH_CONN_SUP_TIMEOUT;

        err_code = sd_ble_gap_ppcp_set(&gap_conn_params);

        DEBUG_printf("sd_ble_gap_ppcp_set(): %lu\n", err_code);
    }

    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
    if (err_code == NRF_SUCCESS) {
        err_code = mbt_peer_manager_init();
    }
    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Disables the Bluetooth stack. Is a no-op when not enabled.
//
// Note:
// The whole SoftDevice will be disabled.
// This is because the SoftDevice doesn't support disabling only the BLE stack.
void mp_bluetooth_deinit(void) {
    DEBUG_printf("mp_bluetooth_deinit\n");

    if (!m_static_flags.ble_enabled) {
        return;
    }

    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
    deconfigure_peer_manager();
    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

    softdevice_drv_disable();

    m_static_flags.ble_enabled = 0;
    MP_STATE_PORT(bluetooth_nrf_root_pointers) = NULL;

    DEBUG_printf("return");
}

// Returns true when the Bluetooth stack is active.
bool mp_bluetooth_is_active(void) {
    return m_static_flags.ble_enabled ? true : false;
}

// Gets the current address of this device in big-endian format.
void mp_bluetooth_get_current_address(uint8_t *addr_type, uint8_t *addr) {
    if (!mp_bluetooth_is_active()) {
        mp_raise_OSError(ERRNO_BLUETOOTH_NOT_ACTIVE);
    }

    ble_gap_addr_t local_ble_addr;
    #if (BLUETOOTH_SD == 110)
    uint32_t err_code = sd_ble_gap_address_get(&local_ble_addr);
    DEBUG_printf("sd_ble_gap_address_get(): %lu", err_code);
    #else
    uint32_t err_code = sd_ble_gap_addr_get(&local_ble_addr);
    DEBUG_printf("sd_ble_gap_addr_get(): %lu", err_code);
    #endif

    if (err_code != NRF_SUCCESS) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("mp_bluetooth_get_current_address() failed"));
    }

    ble_gap_addr_to_mp_addr(&local_ble_addr, addr_type, addr);

    DEBUG_printf("mp_bluetooth_get_current_address: *addr_type: %02x, " \
        "addr: %02x:%02x:%02x:%02x:%02x:%02x\n", \
        *addr_type, \
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}


#if MICROPY_PY_BLUETOOTH_EXTENDED

// Sets the current address of this device in big-endian format.
int mp_bluetooth_set_current_address(const uint8_t *addr) {
    DEBUG_printf("mp_bluetooth_set_current_address - " \
        "addr: %02x:%02x:%02x:%02x:%02x:%02x\n", \
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = NRF_SUCCESS;

    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

    // Change address.

    ble_gap_addr_t ble_gap_addr;

    err_code = pm_id_addr_get(&ble_gap_addr);

    if (err_code == NRF_SUCCESS) {
        reverse_bytes(addr, ble_gap_addr.addr, sizeof(ble_gap_addr.addr));

        err_code = pm_id_addr_set(&ble_gap_addr);

        DEBUG_printf("pm_id_addr_set() returned %s\n", nrf_strerror_get(err_code));
    }

    #else // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

    // Change address.

    ble_gap_addr_t ble_gap_addr;

    err_code = sd_ble_gap_addr_get(&ble_gap_addr);

    if (err_code == NRF_SUCCESS) {
        reverse_bytes(addr, ble_gap_addr.addr, sizeof(ble_gap_addr.addr));

        err_code = sd_ble_gap_addr_set(&ble_gap_addr);

        DEBUG_printf("sd_ble_gap_addr_set() returned %s\n", nrf_strerror_get(err_code));
    }

    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

    return nrf_error_to_errno(err_code);
}

#endif // MICROPY_PY_BLUETOOTH_EXTENDED


// Sets the addressing mode to use.
//
// MP_BLUETOOTH_ADDRESS_MODE_PUBLIC is only supported if
// MICROPY_PY_BLUETOOTH_EXTENDED is set. The address has to be set with
// mp_bluetooth_set_current_address() in that case.
//
// MP_BLUETOOTH_ADDRESS_MODE_RPA and MP_BLUETOOTH_ADDRESS_MODE_NRPA
// are only supported if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING is set.
// If supported MP_BLUETOOTH_ADDRESS_MODE_PUBLIC could be set before
// (see above).
void mp_bluetooth_set_address_mode(uint8_t addr_mode) {
    DEBUG_printf("mp_bluetooth_set_address_mode - addr_mode: %u\n", addr_mode);

    if (!mp_bluetooth_is_active()) {
        mp_raise_OSError(ERRNO_BLUETOOTH_NOT_ACTIVE);
    }

    // See description of sd_ble_gap_addr_set():
    // ...
    // By default the SoftDevice will set an address of type
    // BLE_GAP_ADDR_TYPE_RANDOM_STATIC upon being enabled.
    // The address is a random number populated during the
    // IC manufacturing process and remains unchanged for the
    // lifetime of each IC.
    //
    // See also description of pm_id_addr_set():
    // "... The SoftDevice functions sd_ble_gap_addr_set and sd_ble_gap_privacy_set
    // must not be called when using the Peer Manager. Use the Peer Manager
    // equivalents instead. ..."

    // Check if addr_mode is supported.

    switch (addr_mode) {
        #if MICROPY_PY_BLUETOOTH_EXTENDED
        case MP_BLUETOOTH_ADDRESS_MODE_PUBLIC:
            break;
        #endif // MICROPY_PY_BLUETOOTH_EXTENDED
        case MP_BLUETOOTH_ADDRESS_MODE_RANDOM:
            break;
        #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
        case MP_BLUETOOTH_ADDRESS_MODE_RPA:
        case MP_BLUETOOTH_ADDRESS_MODE_NRPA:
            break;
        #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
        default:
            mp_raise_OSError(MP_EOPNOTSUPP);
            break;
    }

    uint32_t err_code = NRF_SUCCESS;

    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
    // Process addr_mode.

    pm_privacy_params_t privacy_params;
    memset(&privacy_params, 0, sizeof(privacy_params));

    privacy_params.private_addr_type = addr_mode;

    if (addr_mode == MP_BLUETOOTH_ADDRESS_MODE_RANDOM ||
        addr_mode == MP_BLUETOOTH_ADDRESS_MODE_PUBLIC) {

        privacy_params.privacy_mode = BLE_GAP_PRIVACY_MODE_OFF;

        // Change address mode.

        ble_gap_addr_t ble_gap_addr;

        err_code = pm_id_addr_get(&ble_gap_addr);

        if (err_code == NRF_SUCCESS) {
            ble_gap_addr.addr_type = addr_mode;

            err_code = pm_id_addr_set(&ble_gap_addr);

            DEBUG_printf("pm_id_addr_set() returned %s\n", nrf_strerror_get(err_code));
        }
    } else {
        privacy_params.privacy_mode = BLE_GAP_PRIVACY_MODE_DEVICE_PRIVACY;
    }

    if (err_code == NRF_SUCCESS) {
        // Set privacy parameters.

        // Default private_addr_cycle_s is 900 (15 minutes)
        // privacy_params.private_addr_cycle_s = 0;

        err_code = pm_privacy_set(&privacy_params);

        DEBUG_printf("pm_privacy_set() returned %s\n", nrf_strerror_get(err_code));
    }

    #else // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

    // Change address mode.

    ble_gap_addr_t ble_gap_addr;

    err_code = sd_ble_gap_addr_get(&ble_gap_addr);

    if (err_code == NRF_SUCCESS) {
        ble_gap_addr.addr_mode = addr_mode;

        err_code = sd_ble_gap_addr_set(&ble_gap_addr);

        DEBUG_printf("sd_ble_gap_addr_set() returned %s\n", nrf_strerror_get(err_code));
    }

    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

    if (err_code != NRF_SUCCESS) {
        int errno_result = nrf_error_to_errno(err_code);
        mp_raise_OSError(errno_result);
    }
}


#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
// Set bonding flag in pairing requests (i.e. persist security keys).
void mp_bluetooth_set_bonding(bool enabled) {
    DEBUG_printf("mp_bluetooth_set_bonding: %u\n", (uint16_t)enabled);

    if (!mp_bluetooth_is_active()) {
        mp_raise_OSError(ERRNO_BLUETOOTH_NOT_ACTIVE);
    }

    m_static_flags.bonding = (enabled ? 1 : 0);
    set_peer_manager_sec_params();
}

// Require MITM protection.
void mp_bluetooth_set_mitm_protection(bool enabled) {
    DEBUG_printf("mp_bluetooth_set_mitm_protection: %u\n", (uint16_t)enabled);

    if (!mp_bluetooth_is_active()) {
        mp_raise_OSError(ERRNO_BLUETOOTH_NOT_ACTIVE);
    }

    m_static_flags.mitm_protection = (enabled ? 1 : 0);
    set_peer_manager_sec_params();
}

// Require LE Secure pairing (rather than Legacy Pairing)
void mp_bluetooth_set_le_secure(bool enabled) {
    DEBUG_printf("mp_bluetooth_set_le_secure: %u\n", (uint16_t)enabled);

    if (!mp_bluetooth_is_active()) {
        mp_raise_OSError(ERRNO_BLUETOOTH_NOT_ACTIVE);
    }

    #if NRF_MODULE_ENABLED(NRF_BLE_LESC)
    m_static_flags.le_secure = (enabled ? 1 : 0);
    set_peer_manager_sec_params();
    #else // NRF_MODULE_ENABLED(NRF_BLE_LESC)
    if (enabled) {
        mp_raise_OSError(MP_EOPNOTSUPP);
    }
    #endif // NRF_MODULE_ENABLED(NRF_BLE_LESC)
}

// I/O capabilities for authentication (see MP_BLUETOOTH_IO_CAPABILITY_*).
void mp_bluetooth_set_io_capability(uint8_t capability) {
    DEBUG_printf("mp_bluetooth_set_io_capability: %u\n", (uint16_t)capability);

    if (!mp_bluetooth_is_active()) {
        mp_raise_OSError(ERRNO_BLUETOOTH_NOT_ACTIVE);
    }

    m_static_flags.io_capability = capability;
    set_peer_manager_sec_params();
}

#if MICROPY_PY_BLUETOOTH_EXTENDED

// Force a pairing procedure even if there is an existing bond.
void mp_bluetooth_set_force_pairing(bool enabled) {
    DEBUG_printf("mp_bluetooth_set_force_pairing: %u\n", (uint16_t)enabled);

    if (!mp_bluetooth_is_active()) {
        mp_raise_OSError(ERRNO_BLUETOOTH_NOT_ACTIVE);
    }

    m_static_flags.force_pairing = (enabled ? 1 : 0);
}

static uint16_t get_connected_count() {
    uint16_t result = 0;

    for (uint16_t conn_handle = 0; conn_handle < BLE_MAX_CONN_COUNT; conn_handle++) {
        if (m_connection_handle_info[conn_handle].role != BLE_GAP_ROLE_INVALID) {
            result++;
        }
    }

    return result;
}

// Delete all stored bonding information.
// This will result in the event MP_BLUETOOTH_IRQ_BONDS_DELETED.
int mp_bluetooth_delete_bonds(void) {

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = NRF_SUCCESS;

    uint16_t connected_count = get_connected_count();

    // pm_peers_delete() shall not be called if connected or able to connect.
    if (connected_count > 0 ||
        m_static_flags.adv_in_progress ||
        m_static_flags.connect_in_progress) {

        err_code = NRF_ERROR_INVALID_STATE;
    }

    if (err_code == NRF_SUCCESS) {
        // PM_EVT_PEERS_DELETE_SUCCEEDED will be sent by pm_peers_delete() directly
        // if there are no bonds.
        // For that case and other cases were PM_EVT_PEERS_DELETE_SUCCEEDED or
        // PM_EVT_PEERS_DELETE_FAILED are handled before the code behind
        // pm_peers_delete() could be executed delete_bonds_in_progress is
        // set before pm_peers_delete() and not after.
        m_static_flags.delete_bonds_in_progress = 1;

        err_code = pm_peers_delete();

        if (err_code != NRF_SUCCESS) {
            m_static_flags.delete_bonds_in_progress = 0;
        }
    }

    DEBUG_printf("mp_bluetooth_delete_bonds: %s\n", nrf_strerror_get(err_code));

    return nrf_error_to_errno(err_code);
}

#endif // MICROPY_PY_BLUETOOTH_EXTENDED

#endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING


// Get the GAP device name that will be used by service 0x1800, characteristic 0x2a00.
size_t mp_bluetooth_gap_get_device_name(const uint8_t **buf) {
    DEBUG_printf("mp_bluetooth_gap_get_device_name\n");

    *buf = MP_STATE_PORT(bluetooth_nrf_root_pointers)->device_name;
    size_t len = MP_STATE_PORT(bluetooth_nrf_root_pointers)->device_name_len;

    DEBUG_printf("mp_bluetooth_gap_get_device_name: *buf: '%.*s', return len: %u\n", (int)len, (char *)*buf, len);

    return len;
}

// Set the GAP device name that will be used by service 0x1800, characteristic 0x2a00.
int mp_bluetooth_gap_set_device_name(const uint8_t *buf, size_t len) {
    DEBUG_printf("mp_bluetooth_gap_set_device_name\n");

    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    uint32_t err_code = sd_ble_gap_device_name_set(&sec_mode, buf, len);

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Start advertisement. Will re-start advertisement when already enabled.
int mp_bluetooth_gap_advertise_start(bool connectable, int32_t interval_us, const uint8_t *adv_data, size_t adv_data_len, const uint8_t *sr_data, size_t sr_data_len) {
    DEBUG_printf("mp_bluetooth_gap_advertise_start\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING && MICROPY_PY_BLUETOOTH_EXTENDED
    if (m_static_flags.delete_bonds_in_progress) {
        return MP_EIO;
    }
    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING && MICROPY_PY_BLUETOOTH_EXTENDED

    #if (BLUETOOTH_SD == 110)
    if (m_static_flags.adv_in_progress) {
        return MP_EOPNOTSUPP;
    }
    #endif

    // Persistent buffer for adv_data (which is findable via the root pointers).
    //
    // Use temporary pointer to new advertising data to prevent to early garbage
    // collection when updating an ongoing advertisement.
    uint8_t **pp_new_adv_data = &(MP_STATE_PORT(bluetooth_nrf_root_pointers)->new_adv_data);
    *pp_new_adv_data = NULL;
    size_t new_adv_data_alloc = 0;

    if (adv_data) {
        resize_data_alloc_if_needed(adv_data_len, pp_new_adv_data, &new_adv_data_alloc);
        memcpy(*pp_new_adv_data, adv_data, adv_data_len);
    }

    // Persistent buffer for sr_data (which is findable via the root pointers).
    //
    // Use temporary pointer to new scan response data to prevent to early garbage
    // collection when updating an ongoing advertisement.
    uint8_t **pp_new_sr_data = &(MP_STATE_PORT(bluetooth_nrf_root_pointers)->new_sr_data);
    *pp_new_sr_data = NULL;
    size_t new_sr_data_alloc = 0;

    if (sr_data) {
        resize_data_alloc_if_needed(sr_data_len, pp_new_sr_data, &new_sr_data_alloc);
        memcpy(*pp_new_sr_data, sr_data, sr_data_len);
    }

    ble_gap_adv_params_t adv_params;
    ble_gap_adv_params_t *p_m_adv_params = NULL;

    if (!m_static_flags.adv_in_progress) {
        // initialize advertising params

        p_m_adv_params = &adv_params;
        memset(&adv_params, 0, sizeof(adv_params));

        #if (BLUETOOTH_SD == 110)
        adv_params.type = connectable ? BLE_GAP_ADV_TYPE_ADV_IND : BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
        adv_params.fp = BLE_GAP_ADV_FP_ANY;
        // adv_params.timeout                     = 0; // infinite advertisement
        #else
        adv_params.properties.type = connectable ? BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED : BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;
        // adv_params.properties.anonymous        = 0;
        // include_tx_power is not supported on this SoftDevice.
        // adv_params.properties.include_tx_power = 0;
        adv_params.duration = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
        adv_params.primary_phy = BLE_GAP_PHY_AUTO;
        adv_params.secondary_phy = BLE_GAP_PHY_AUTO;
        // adv_params.filter_policy               = 0;
        // adv_params.max_adv_evts                = 0; // infinite advertisement
        // adv_params.scan_req_notification       = 0; // Do not raise scan request notifications when scanned.
        // adv_params.channel_mask set to 0 with memset()
        // adv_params.set_id is ignored if no extended advertising type is used.
        #endif
        // adv_params.p_peer_addr                 = NULL; // undirected advertisement
        adv_params.interval = USEC_TO_UNITS(interval_us, UNIT_625_US);
    }

    #if (BLUETOOTH_SD == 110)
    uint32_t err_code = sd_ble_gap_adv_data_set(*pp_new_adv_data, adv_data_len, *pp_new_sr_data, sr_data_len);
    #else
    ble_gap_adv_data_t ble_gap_adv_data;
    memset(&ble_gap_adv_data, 0, sizeof(ble_gap_adv_data));

    ble_gap_adv_data.adv_data.p_data = *pp_new_adv_data;
    ble_gap_adv_data.adv_data.len = adv_data_len;
    ble_gap_adv_data.scan_rsp_data.p_data = *pp_new_sr_data;
    ble_gap_adv_data.scan_rsp_data.len = sr_data_len;

    uint32_t err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &ble_gap_adv_data, p_m_adv_params);
    #endif

    if (err_code == NRF_SUCCESS) {
        // New advertising data and/or scan response data has been set.
        // Update the root pointers.

        MP_STATE_PORT(bluetooth_nrf_root_pointers)->adv_data = *pp_new_adv_data;
        MP_STATE_PORT(bluetooth_nrf_root_pointers)->adv_data_alloc = new_adv_data_alloc;

        MP_STATE_PORT(bluetooth_nrf_root_pointers)->sr_data = *pp_new_sr_data;
        MP_STATE_PORT(bluetooth_nrf_root_pointers)->sr_data_alloc = new_sr_data_alloc;
    }

    // Clear temporary root pointers.
    *pp_new_adv_data = NULL;
    *pp_new_sr_data = NULL;

    #if (BLUETOOTH_SD == 110)
    if (err_code == NRF_SUCCESS) {
        err_code = sd_ble_gap_adv_start(p_m_adv_params);
    }
    #else
    if (err_code == NRF_SUCCESS) {
        if (!m_static_flags.adv_in_progress) {
            uint8_t conf_tag = BLE_MODBLUETOOTH_CONN_CONFIG_TAG;
            err_code = sd_ble_gap_adv_start(m_adv_handle, conf_tag);
        }
    }
    #endif

    if (err_code == NRF_SUCCESS) {
        m_static_flags.adv_in_progress = 1;
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Stop advertisement. No-op when already stopped.
void mp_bluetooth_gap_advertise_stop(void) {
    DEBUG_printf("mp_bluetooth_gap_advertise_stop\n");

    if (m_static_flags.adv_in_progress) {
        uint32_t err_code;

        #if (BLUETOOTH_SD == 110)
        err_code = sd_ble_gap_adv_stop();
        #else
        err_code = sd_ble_gap_adv_stop(m_adv_handle);
        #endif
        if (err_code != NRF_SUCCESS) {
            mp_raise_OSError(nrf_error_to_errno(err_code));
        }

        m_static_flags.adv_in_progress = 0;

        MP_STATE_PORT(bluetooth_nrf_root_pointers)->adv_data_alloc = 0;
        MP_STATE_PORT(bluetooth_nrf_root_pointers)->adv_data = NULL;

        MP_STATE_PORT(bluetooth_nrf_root_pointers)->sr_data_alloc = 0;
        MP_STATE_PORT(bluetooth_nrf_root_pointers)->sr_data = NULL;
    }

    DEBUG_printf("return");
}


#if (BLUETOOTH_SD == 140)

// Start adding services. Must be called before mp_bluetooth_register_service.
int mp_bluetooth_gatts_register_service_begin(bool append) {
    DEBUG_printf("mp_bluetooth_gatts_register_service_begin\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    int errno_result = 0;

    if (!append) {
        if (m_static_flags.services_registered) {
            // SoftDevice doesn't support reset of registry.
            errno_result = MP_EOPNOTSUPP;
        }
    }

    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

uint32_t mp_uuid_to_ble_uuid_with_vs_add(const mp_obj_bluetooth_uuid_t *mp_uuid, ble_uuid_t *ble_uuid) {
    uint32_t err_code = NRF_SUCCESS;

    if (mp_uuid->type == MP_BLUETOOTH_UUID_TYPE_16) {
        err_code = sd_ble_uuid_decode(2, mp_uuid->data, ble_uuid);
    } else if (mp_uuid->type == MP_BLUETOOTH_UUID_TYPE_128) {
        err_code = sd_ble_uuid_decode(16, mp_uuid->data, ble_uuid);

        if (err_code == NRF_ERROR_NOT_FOUND) {
            ble_uuid128_t ble_uuid128;
            memcpy(&ble_uuid128.uuid128, mp_uuid->data, sizeof(ble_uuid128.uuid128));

            uint8_t dummy_uuid_type;
            err_code = sd_ble_uuid_vs_add(&ble_uuid128, &(dummy_uuid_type));

            DEBUG_printf("sd_ble_uuid_vs_add(): 0x%04lx\n", err_code);

            if (err_code == NRF_SUCCESS) {
                err_code = sd_ble_uuid_decode(sizeof(ble_uuid128.uuid128), ble_uuid128.uuid128, ble_uuid);

                DEBUG_printf("sd_ble_uuid_decode(): 0x%04lx\n", err_code);
            }
        }
    } else {
        err_code = NRF_ERROR_INVALID_PARAM;
    }

    return err_code;
}

static uint32_t ble_uuid_to_mp_uuid(const ble_uuid_t *ble_uuid, mp_obj_bluetooth_uuid_t *mp_uuid) {
    mp_uuid->base.type = &mp_type_bluetooth_uuid;
    mp_uuid->type = BLE_UUID_TYPE_UNKNOWN;
    memset(mp_uuid->data, 0, sizeof(mp_uuid->data));

    uint8_t ble_uuid_len;
    uint32_t err_code = sd_ble_uuid_encode(ble_uuid, &ble_uuid_len, mp_uuid->data);

    if (err_code == NRF_SUCCESS) {
        if (ble_uuid_len == 2) {
            mp_uuid->type = MP_BLUETOOTH_UUID_TYPE_16;
        } else {
            mp_uuid->type = MP_BLUETOOTH_UUID_TYPE_128;
        }
    }

    return err_code;
}

static void mp_characteristic_flags_to_ble_gatt_char_props(
    uint16_t mp_characteristic_flags,
    ble_gatt_char_props_t *ble_gatt_char_props) {

    ble_gatt_char_props->broadcast = (mp_characteristic_flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_BROADCAST) ? 1 : 0;
    ble_gatt_char_props->read = (mp_characteristic_flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ) ? 1 : 0;
    ble_gatt_char_props->write_wo_resp = (mp_characteristic_flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_NO_RESPONSE) ? 1 : 0;
    ble_gatt_char_props->write = (mp_characteristic_flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE) ? 1 : 0;
    ble_gatt_char_props->notify = (mp_characteristic_flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_NOTIFY) ? 1 : 0;
    ble_gatt_char_props->indicate = (mp_characteristic_flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_INDICATE) ? 1 : 0;
    ble_gatt_char_props->auth_signed_wr = (mp_characteristic_flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_AUTHENTICATED_SIGNED_WRITE) ? 1 : 0;
}

static uint8_t ble_gatt_char_props_to_mp_characteristic_properties(
    const ble_gatt_char_props_t *ble_gatt_char_props) {

    uint8_t result = 0;

    if (ble_gatt_char_props->broadcast) {
        result |= MP_BLUETOOTH_CHARACTERISTIC_FLAG_BROADCAST;
    }

    if (ble_gatt_char_props->read) {
        result |= MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ;
    }

    if (ble_gatt_char_props->write_wo_resp) {
        result |= MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_NO_RESPONSE;
    }

    if (ble_gatt_char_props->write) {
        result |= MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE;
    }

    if (ble_gatt_char_props->notify) {
        result |= MP_BLUETOOTH_CHARACTERISTIC_FLAG_NOTIFY;
    }

    if (ble_gatt_char_props->indicate) {
        result |= MP_BLUETOOTH_CHARACTERISTIC_FLAG_INDICATE;
    }

    if (ble_gatt_char_props->auth_signed_wr) {
        result |= MP_BLUETOOTH_CHARACTERISTIC_FLAG_AUTHENTICATED_SIGNED_WRITE;
    }

    return result;
}

static void mp_characteristic_flags_to_ble_gatt_ext_char_props(
    uint16_t mp_characteristic_flags,
    ble_gatt_char_ext_props_t *ble_gatt_char_ext_props) {

    ble_gatt_char_ext_props->reliable_wr = 0;
    ble_gatt_char_ext_props->wr_aux = (mp_characteristic_flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_AUX_WRITE) ? 1 : 0;
}

// Map MP_BLUETOOTH_CHARACTERISTIC_FLAG_ values to SoftDevice read/write permission values.
static uint32_t mp_characteristic_flags_to_ble_sec_modes(uint16_t flags, ble_gap_conn_sec_mode_t *read_permission, ble_gap_conn_sec_mode_t *write_permission) {
    uint32_t err_code = NRF_SUCCESS;

    if (flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ_ENCRYPTED) {
        // ATT_SECURITY_ENCRYPTED;
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(read_permission);
    } else if (flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ_AUTHENTICATED) {
        // ATT_SECURITY_AUTHENTICATED;
        BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(read_permission);
    } else if (flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ_AUTHORIZED) {
        // ATT_SECURITY_AUTHORIZED;
        err_code = NRF_ERROR_NOT_SUPPORTED;
    } else {
        // ATT_SECURITY_NONE;
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(read_permission);
    }

    if (flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_ENCRYPTED) {
        // ATT_SECURITY_ENCRYPTED;
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(write_permission);
    } else if (flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_AUTHENTICATED) {
        // ATT_SECURITY_AUTHENTICATED;
        BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(write_permission);
    } else if (flags & MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_AUTHORIZED) {
        // ATT_SECURITY_AUTHORIZED;
        err_code = NRF_ERROR_NOT_SUPPORTED;
    } else {
        // ATT_SECURITY_NONE;
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(write_permission);
    }

    return err_code;
}

// Add a service with the given list of characteristics to the queue to be registered.
// The value_handles won't be valid until after mp_bluetooth_register_service_end is called.
int mp_bluetooth_gatts_register_service(mp_obj_bluetooth_uuid_t *service_uuid, mp_obj_bluetooth_uuid_t **characteristic_uuids, uint16_t *characteristic_flags, mp_obj_bluetooth_uuid_t **descriptor_uuids, uint16_t *descriptor_flags, uint8_t *num_descriptors, uint16_t *handles, size_t num_characteristics) {
    DEBUG_printf("mp_bluetooth_gatts_register_service\n");

    uint32_t err_code;

    ble_uuid_t ble_service_uuid;
    err_code = mp_uuid_to_ble_uuid_with_vs_add(service_uuid, &ble_service_uuid);

    uint16_t service_handle = 0xffff;

    if (err_code == NRF_SUCCESS) {
        err_code = sd_ble_gatts_service_add(
            BLE_GATTS_SRVC_TYPE_PRIMARY,
            &ble_service_uuid,
            &service_handle);
    }

    if (err_code == NRF_SUCCESS) {
        m_static_flags.services_registered = 1;
    }

    // characteristics with optional descriptors

    size_t handle_index = 0;

    size_t characteristic_index = 0;
    size_t descriptor_index = 0;

    while (err_code == NRF_SUCCESS && characteristic_index < num_characteristics) {

        // p_char_md argument of sd_ble_gatts_characteristic_add()
        ble_gatts_char_md_t char_md;
        memset(&char_md, 0, sizeof(char_md));

        mp_characteristic_flags_to_ble_gatt_char_props(characteristic_flags[characteristic_index], &(char_md.char_props));
        mp_characteristic_flags_to_ble_gatt_ext_char_props(characteristic_flags[characteristic_index], &(char_md.char_ext_props));

        ble_gatts_attr_md_t cccd_md;

        if (char_md.char_props.notify || char_md.char_props.indicate) {
            memset(&cccd_md, 0, sizeof(cccd_md));
            BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
            BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
            cccd_md.vloc = BLE_GATTS_VLOC_STACK;
            // cccd_md.vlen = 0;
            // cccd_md.rd_auth = 0;
            // cccd_md.wr_auth = 0;

            char_md.p_cccd_md = &cccd_md;
        }

        // p_attr_char_value argument of sd_ble_gatts_characteristic_add()
        ble_gatts_attr_t attr_char_value;
        memset(&attr_char_value, 0, sizeof(attr_char_value));

        ble_uuid_t ble_characteristic_uuid;
        err_code = mp_uuid_to_ble_uuid_with_vs_add(characteristic_uuids[characteristic_index], &ble_characteristic_uuid);

        // p_attr_char_value.p_attr_md
        ble_gatts_attr_md_t attr_char_value_md;
        memset(&attr_char_value_md, 0, sizeof(attr_char_value_md));

        if (err_code == NRF_SUCCESS) {
            attr_char_value.p_uuid = &ble_characteristic_uuid;

            err_code = mp_characteristic_flags_to_ble_sec_modes(
                characteristic_flags[characteristic_index],
                &(attr_char_value_md.read_perm),
                &(attr_char_value_md.write_perm));
        }

        attr_char_value_md.vlen = 1;
        attr_char_value_md.vloc = BLE_GATTS_VLOC_STACK;
        // attr_char_value_md.rd_auth = 0;
        // attr_char_value_md.wr_auth = 0;

        attr_char_value.p_attr_md = &attr_char_value_md;

        attr_char_value.init_len = MP_BLUETOOTH_DEFAULT_CHAR_ATTR_LEN;
        // attr_char_value.init_offs = 0;
        attr_char_value.max_len = MP_BLUETOOTH_MAX_CHAR_ATTR_LEN;
        // attr_char_value.p_value   = NULL;

        // p_handles argument of sd_ble_gatts_characteristic_add()
        ble_gatts_char_handles_t char_handles;

        if (err_code == NRF_SUCCESS) {
            err_code = sd_ble_gatts_characteristic_add(
                service_handle,
                &char_md,
                &attr_char_value,
                &char_handles);
        }

        if (err_code == NRF_SUCCESS) {
            handles[handle_index] = char_handles.value_handle;
        }

        handle_index++;

        // descriptors

        uint8_t characteristic_num_descriptors = num_descriptors[characteristic_index];

        size_t characteristic_descriptor_index = 0;

        while (err_code == NRF_SUCCESS && characteristic_descriptor_index < characteristic_num_descriptors) {

            // p_attr argument of sd_ble_gatts_descriptor_add()
            ble_gatts_attr_t attr_descriptor;
            memset(&attr_descriptor, 0, sizeof(attr_descriptor));

            ble_uuid_t ble_descriptor_uuid;
            err_code = mp_uuid_to_ble_uuid_with_vs_add(descriptor_uuids[descriptor_index], &ble_descriptor_uuid);

            // p_attr.p_attr_md
            ble_gatts_attr_md_t attr_descriptor_md;
            memset(&attr_descriptor_md, 0, sizeof(attr_descriptor_md));

            if (err_code == NRF_SUCCESS) {
                attr_descriptor.p_uuid = &ble_descriptor_uuid;

                err_code = mp_characteristic_flags_to_ble_sec_modes(
                    descriptor_flags[descriptor_index],
                    &(attr_descriptor_md.read_perm),
                    &(attr_descriptor_md.write_perm));
            }

            attr_descriptor_md.vlen = 1;
            attr_descriptor_md.vloc = BLE_GATTS_VLOC_STACK;
            // attr_descriptor_md.rd_auth = 0;
            // attr_descriptor_md.wr_auth = 0;

            attr_descriptor.p_attr_md = &attr_descriptor_md;

            attr_descriptor.init_len = MP_BLUETOOTH_DEFAULT_DESC_ATTR_LEN;
            // attr_descriptor.init_offs = 0;
            attr_descriptor.max_len = MP_BLUETOOTH_MAX_DESC_ATTR_LEN;
            // attr_descriptor.p_value   = NULL;

            // p_handle argument of sd_ble_gatts_descriptor_add()
            uint16_t descriptor_handle;

            if (err_code == NRF_SUCCESS) {
                err_code = sd_ble_gatts_descriptor_add(
                    char_handles.value_handle,
                    &attr_descriptor,
                    &descriptor_handle);
            }

            if (err_code == NRF_SUCCESS) {
                handles[handle_index] = descriptor_handle;
            }

            handle_index++;

            characteristic_descriptor_index++;
            descriptor_index++;
        }

        characteristic_index++;
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Register any queued services.
int mp_bluetooth_gatts_register_service_end(void) {
    DEBUG_printf("mp_bluetooth_gatts_register_service_end: 0\n");

    return 0;
}

#else // #if (BLUETOOTH_SD == 140)

// Start adding services. Must be called before mp_bluetooth_register_service.
int mp_bluetooth_gatts_register_service_begin(bool append) {
    DEBUG_printf("mp_bluetooth_gatts_register_service_begin: MP_EOPNOTSUPP\n");

    return MP_EOPNOTSUPP;
}

// Add a service with the given list of characteristics to the queue to be registered.
// The value_handles won't be valid until after mp_bluetooth_register_service_end is called.
int mp_bluetooth_gatts_register_service(mp_obj_bluetooth_uuid_t *service_uuid, mp_obj_bluetooth_uuid_t **characteristic_uuids, uint16_t *characteristic_flags, mp_obj_bluetooth_uuid_t **descriptor_uuids, uint16_t *descriptor_flags, uint8_t *num_descriptors, uint16_t *handles, size_t num_characteristics) {
    DEBUG_printf("mp_bluetooth_gatts_register_service: MP_EOPNOTSUPP\n");

    return MP_EOPNOTSUPP;
}

// Register any queued services.
int mp_bluetooth_gatts_register_service_end(void) {
    DEBUG_printf("mp_bluetooth_gatts_register_service_end: MP_EOPNOTSUPP\n");

    return MP_EOPNOTSUPP;
}

#endif // #if (BLUETOOTH_SD == 140)


// Read the value from the local gatts db (likely this has been written by a central).
int mp_bluetooth_gatts_read(uint16_t value_handle, const uint8_t **value, size_t *value_len) {
    DEBUG_printf("mp_bluetooth_gatts_read: value_handle: %u\n", value_handle);

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint8_t **pp_gatts_read_buffer = &(MP_STATE_PORT(bluetooth_nrf_root_pointers)->gatts_read_buffer);
    size_t *p_gatts_read_buffer_alloc = &(MP_STATE_PORT(bluetooth_nrf_root_pointers)->gatts_read_buffer_alloc);

    ble_gatts_value_t ble_gatts_value;
    memset(&ble_gatts_value, 0, sizeof(ble_gatts_value));

    ble_gatts_value.len = *p_gatts_read_buffer_alloc;
    // ble_gatts_value.offset = 0;
    ble_gatts_value.p_value = *pp_gatts_read_buffer;

    // First argument: Connection handle. Ignored if the value does not belong to a system attribute.
    uint32_t err_code = sd_ble_gatts_value_get(0, value_handle, &ble_gatts_value);

    if (err_code == NRF_SUCCESS) {
        // Resize pp_gatts_read_buffer if to small and read again.
        bool has_resized = resize_data_alloc_if_needed(ble_gatts_value.len, pp_gatts_read_buffer, p_gatts_read_buffer_alloc);

        if (has_resized) {
            ble_gatts_value.p_value = *pp_gatts_read_buffer;

            err_code = sd_ble_gatts_value_get(0, value_handle, &ble_gatts_value);
        }
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Write a value to the local gatts db (ready to be queried by a central). Optionally send notifications/indications.
int mp_bluetooth_gatts_write(uint16_t value_handle, const uint8_t *value, size_t value_len, bool send_update) {
    DEBUG_printf("mp_bluetooth_gatts_write: value_handle: %u\n", value_handle);

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code;

    if (send_update) {
        // Set the characteristic value and do a notify/indicate
        // for all connections to Centrals.

        ble_gatts_hvx_params_t hvx_params;
        memset(&hvx_params, 0, sizeof(hvx_params));

        uint16_t hvx_len;

        hvx_params.handle = value_handle;
        // hvx_params.offset = 0;
        hvx_params.p_len = &hvx_len;
        hvx_params.p_data = value;

        bool do_break = false;

        uint16_t conn_handle = 0;

        while (!do_break && conn_handle < BLE_MAX_CONN_COUNT) {
            if (m_connection_handle_info[conn_handle].role == BLE_GAP_ROLE_PERIPH) {
                // This device is in the peripheral role.

                size_t hvx_type_index = 0;

                // call sd_ble_gatts_hvx() with BLE_GATT_HVX_NOTIFICATION and
                // BLE_GATT_HVX_INDICATION in turn
                while (!do_break && hvx_type_index < (sizeof(m_hvx_types) / sizeof(m_hvx_types[0]))) {
                    hvx_len = value_len;
                    hvx_params.type = m_hvx_types[hvx_type_index];

                    uint32_t hvx_err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);

                    if (hvx_len == value_len) {
                        // characteristic value has been updated - don't update it again
                        hvx_params.p_data = NULL;
                    }

                    if (err_code == NRF_SUCCESS) {
                        // report the first error
                        err_code = hvx_err_code;
                    }

                    if (hvx_err_code != NRF_SUCCESS
                        && hvx_err_code != BLE_ERROR_INVALID_CONN_HANDLE
                        && hvx_err_code != NRF_ERROR_INVALID_STATE
                        && hvx_err_code != NRF_ERROR_FORBIDDEN) {
                        do_break = true;
                    }

                    hvx_type_index++;
                }
            }

            conn_handle++;
        }
    } else {
        ble_gatts_value_t ble_gatts_value;
        memset(&ble_gatts_value, 0, sizeof(ble_gatts_value));

        ble_gatts_value.len = value_len;
        // ble_gatts_value.offset = 0;
        // remove const from value
        ble_gatts_value.p_value = (uint8_t *)value;

        // First argument: Connection handle. Ignored if the value does not belong to a system attribute.
        err_code = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, value_handle, &ble_gatts_value);
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Send a notification/indication to the central, optionally with custom payload (otherwise the DB value is used).
//
// Note:
// The MicroPython documentation states for BLE.gatts_indicate():
// "The indication will be sent regardless of the subscription status of the
//  client to this characteristic."
// - But this is not true for this implementation because sd_ble_gatts_hvx()
//   does always check the 'subscription status'.
// - Please note also that a supplied 'value' will be written to the 'DB' as
//   it would with mp_bluetooth_gatts_write().
int mp_bluetooth_gatts_notify_indicate(uint16_t conn_handle, uint16_t value_handle, int gatts_op, const uint8_t *value, size_t value_len) {
    DEBUG_printf("mp_bluetooth_gatts_notify_indicate\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    ble_gatts_hvx_params_t hvx_params;
    memset(&hvx_params, 0, sizeof(hvx_params));

    uint16_t hvx_len = value_len;

    hvx_params.handle = value_handle;
    // hvx_params.offset = 0;
    hvx_params.p_len = &hvx_len;
    hvx_params.p_data = value;

    uint32_t err_code = NRF_SUCCESS;

    if (gatts_op == MP_BLUETOOTH_GATTS_OP_NOTIFY) {
        hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    } else if (gatts_op == MP_BLUETOOTH_GATTS_OP_INDICATE) {
        hvx_params.type = BLE_GATT_HVX_INDICATION;
    } else {
        err_code = NRF_ERROR_INVALID_PARAM;
    }

    if (err_code == NRF_SUCCESS) {
        err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Resize and enable/disable append-mode on a value.
// Append-mode means that remote writes will append and local reads will clear after reading.
//
// The SoftDevice doesn't allow to change the pointer to the value storage
// even if we use BLE_GATTS_VLOC_USER as value location.
// So mp_bluetooth_gatts_set_buffer() could not be supported.

int mp_bluetooth_gatts_set_buffer(uint16_t value_handle, size_t len, bool append) {
    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    return MP_EOPNOTSUPP;
}

// Disconnect from a central or peripheral.
int mp_bluetooth_gap_disconnect(uint16_t conn_handle) {
    DEBUG_printf("mp_bluetooth_gap_disconnect\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Get the MTU that we will respond to a MTU exchange with.
int mp_bluetooth_get_preferred_mtu(void) {
    return m_prefered_mtu;
}

// Set the MTU that we will respond to a MTU exchange with.
int mp_bluetooth_set_preferred_mtu(uint16_t mtu) {
    DEBUG_printf("mp_bluetooth_set_preferred_mtu\n");

    int errno_result = 0;

    if (mtu < BLE_GATT_ATT_MTU_DEFAULT || mtu > NRF_SDH_BLE_GATT_MAX_MTU_SIZE) {
        errno_result = MP_EINVAL;
    } else {
        m_prefered_mtu = mtu;
    }

    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}


#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

// Initiate pairing on the specified connection.
int mp_bluetooth_gap_pair(uint16_t conn_handle) {
    DEBUG_printf("mp_bluetooth_gap_pair - conn_handle: %u\n", conn_handle);

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    bool force_repairing = false;

    #if MICROPY_PY_BLUETOOTH_EXTENDED
    force_repairing = m_static_flags.force_pairing ? true : false;
    #endif // MICROPY_PY_BLUETOOTH_EXTENDED

    uint32_t err_code = pm_conn_secure(conn_handle, force_repairing);
    DEBUG_printf("pm_conn_secure() returned %s\n", nrf_strerror_get(err_code));
    return nrf_error_to_errno(err_code);
}

// Respond to a pairing request.
int mp_bluetooth_gap_passkey(uint16_t conn_handle, uint8_t action, mp_int_t passkey) {

    DEBUG_printf("mp_bluetooth_gap_passkey: conn_handle=%u, action=%u, passkey=%d\n",
        conn_handle, (uint16_t)action, passkey);

    uint32_t err_code = NRF_SUCCESS;

    uint8_t key_type = BLE_GAP_AUTH_KEY_TYPE_NONE;
    uint8_t *p_key = NULL;

    char passkey_string[7];
    memset(passkey_string, 0, sizeof(passkey_string));

    switch (action) {
        case MP_BLUETOOTH_PASSKEY_ACTION_NONE: {
            // Reject authentication.
            break;
        }

        case MP_BLUETOOTH_PASSKEY_ACTION_INPUT: {

            int passkey_char_count_needed = snprintf(
                passkey_string, sizeof(passkey_string), "%06u", (unsigned)passkey);

            if (passkey_char_count_needed >= sizeof(passkey_string)) {
                err_code = NRF_ERROR_INVALID_PARAM;
            } else {
                key_type = BLE_GAP_AUTH_KEY_TYPE_PASSKEY;
                p_key = (uint8_t *)passkey_string;
            }

            break;
        }

        case MP_BLUETOOTH_PASSKEY_ACTION_NUMERIC_COMPARISON: {
            if (passkey != 0) {
                key_type = BLE_GAP_AUTH_KEY_TYPE_PASSKEY;
            }
            break;
        }

        default: {
            err_code = NRF_ERROR_INVALID_PARAM;
        }
    }

    if (err_code == NRF_SUCCESS) {
        err_code = sd_ble_gap_auth_key_reply(conn_handle, key_type, p_key);
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

#endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING


#if MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE

// Initialize m_variable_scan_params_t.
//
// Note that variable_scan_params->scan_in_progress will not be set.
void init_variable_scan_params(
    int32_t duration_ms, int32_t interval_us, int32_t window_us, bool active_scan,
    m_variable_scan_params_t *variable_scan_params) {
    variable_scan_params->active = active_scan ? 1 : 0;
    variable_scan_params->interval = USEC_TO_UNITS(interval_us, UNIT_625_US);
    variable_scan_params->window = USEC_TO_UNITS(window_us, UNIT_625_US);
    variable_scan_params->timeout = MSEC_TO_UNITS(duration_ms, UNIT_10_MS);
}

// Initialize ble_gap_scan_params_t.
//
// variable_scan_params:
//   Variable scan parameters that will be copied to scan_params.
// scan_params:
//   ble_gap_scan_params_t that will be initialized.
void init_scan_params(const m_variable_scan_params_t *variable_scan_params, ble_gap_scan_params_t *scan_params) {

    memset(scan_params, 0, sizeof(ble_gap_scan_params_t));

    scan_params->active = variable_scan_params->active;
    scan_params->interval = variable_scan_params->interval;
    scan_params->window = variable_scan_params->window;
    scan_params->timeout = variable_scan_params->timeout;

    // scan_params->extended = 0;
    // report_incomplete_evts is not supported by this SoftDevice.
    // scan_params->report_incomplete_evts = 0;
    scan_params->filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
    // If 'extended' is set to 0 the only accepted value is BLE_GAP_PHY_1MBPS.
    scan_params->scan_phys = BLE_GAP_PHY_1MBPS;
    // scan_params->channel_mask is cleared: use all channels
}

// Start or continue scan with m_scan_buffer.
//
// scan_params:
// Set it to NULL to continue scanning.
static uint32_t ble_gap_scan_start_continue(ble_gap_scan_params_t *scan_params) {

    ble_data_t scan_buffer;
    memset(&scan_buffer, 0, sizeof(scan_buffer));

    scan_buffer.p_data = m_scan_buffer,
    // BLE_GAP_SCAN_BUFFER_MIN and BLE_GAP_SCAN_BUFFER_MAX are both 31
    scan_buffer.len = sizeof(m_scan_buffer);

    uint32_t err_code = sd_ble_gap_scan_start(scan_params, &scan_buffer);

    return err_code;
}

// Start a discovery (scan). Set duration to zero to run continuously.
int mp_bluetooth_gap_scan_start(int32_t duration_ms, int32_t interval_us, int32_t window_us, bool active_scan) {
    DEBUG_printf("mp_bluetooth_gap_scan_start\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = NRF_SUCCESS;

    if (m_variable_scan_params.scan_in_progress) {
        // A scan is already in progress.
        err_code = NRF_ERROR_INVALID_STATE;
    }

    if (err_code == NRF_SUCCESS) {
        // Initialize m_variable_scan_params.
        init_variable_scan_params(duration_ms, interval_us, window_us, active_scan, &m_variable_scan_params);

        ble_gap_scan_params_t scan_params;
        init_scan_params(&m_variable_scan_params, &scan_params);

        err_code = ble_gap_scan_start_continue(&scan_params);

        DEBUG_printf("ble_gap_scan_start_continue(&scan_params) - active: %u, filter_policy: 0x%02x, interval: %u, window: %u, timeout: %u result: %lu\n",
            (uint16_t)scan_params.active,
            (uint16_t)scan_params.filter_policy,
            scan_params.interval,
            scan_params.window,
            scan_params.timeout,
            err_code);
    }

    if (err_code == NRF_SUCCESS) {
        m_variable_scan_params.scan_in_progress = 1;
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Stop discovery (if currently active).
int mp_bluetooth_gap_scan_stop(void) {
    DEBUG_printf("mp_bluetooth_gap_scan_stop\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = sd_ble_gap_scan_stop();

    m_variable_scan_params.scan_in_progress = 0;

    if (err_code == NRF_SUCCESS) {
        mp_bluetooth_gap_on_scan_complete();
    } else if (err_code == NRF_ERROR_INVALID_STATE) {
        // Was not in the scanning state.
        err_code = NRF_SUCCESS;
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Connect to a found peripheral.
int mp_bluetooth_gap_peripheral_connect(uint8_t addr_type, const uint8_t *addr, int32_t duration_ms, int32_t min_conn_interval_us, int32_t max_conn_interval_us) {
    DEBUG_printf("mp_bluetooth_gap_peripheral_connect: "HEX2_FMT ":"HEX2_FMT ":"HEX2_FMT ":"HEX2_FMT ":"HEX2_FMT ":"HEX2_FMT ", type: %d\n",
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr_type);

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING && MICROPY_PY_BLUETOOTH_EXTENDED
    if (m_static_flags.delete_bonds_in_progress) {
        return MP_EIO;
    }
    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING && MICROPY_PY_BLUETOOTH_EXTENDED

    ble_gap_addr_t ble_gap_addr;
    mp_addr_to_ble_gap_addr(addr_type, addr, &ble_gap_addr);

    // Scan parameters.
    ble_gap_scan_params_t scan_params;

    if (m_variable_scan_params.scan_in_progress) {
        init_scan_params(&m_variable_scan_params, &scan_params);
    } else {
        // Values taken from modbluetooth_btstack.c
        const int32_t interval_us = 60000;
        const int32_t window_us = 30000;
        const bool active_scan = false;

        m_variable_scan_params_t variable_scan_params;
        init_variable_scan_params(duration_ms, interval_us, window_us, active_scan, &variable_scan_params);

        init_scan_params(&variable_scan_params, &scan_params);
    }

    // Connection parameters.
    ble_gap_conn_params_t conn_params;
    memset(&conn_params, 0, sizeof(conn_params));

    conn_params.min_conn_interval = (min_conn_interval_us ?
        USEC_TO_UNITS(min_conn_interval_us, UNIT_1250_US) : MICROPY_PY_BLUETOOTH_MIN_CONN_INTERVAL);

    conn_params.max_conn_interval = (max_conn_interval_us ?
        USEC_TO_UNITS(max_conn_interval_us, UNIT_1250_US) : MICROPY_PY_BLUETOOTH_MAX_CONN_INTERVAL);

    conn_params.slave_latency = MICROPY_PY_BLUETOOTH_CONN_SLAVE_LATENCY;

    // Set supervision timeout that is used to monitor link loss.
    // It is set to duration_ms like in 'modbluetooth_btstack.c'.
    // It is limited to the allowed minimum and maximum.
    uint16_t supervision_timeout = MSEC_TO_UNITS(duration_ms, UNIT_10_MS);
    supervision_timeout = MAX(supervision_timeout, BLE_GAP_CP_CONN_SUP_TIMEOUT_MIN);
    supervision_timeout = MIN(supervision_timeout, BLE_GAP_CP_CONN_SUP_TIMEOUT_MAX);

    conn_params.conn_sup_timeout = supervision_timeout;

    DEBUG_printf("min_conn_interval: %u, max_conn_interval: %u, slave_latency: %u, conn_sup_timeout: %u\n",
        conn_params.min_conn_interval,
        conn_params.max_conn_interval,
        conn_params.slave_latency,
        conn_params.conn_sup_timeout);

    uint32_t err_code = sd_ble_gap_connect(&ble_gap_addr, &scan_params, &conn_params, BLE_MODBLUETOOTH_CONN_CONFIG_TAG);

    if (err_code == NRF_SUCCESS) {
        m_static_flags.connect_in_progress = 1;
        m_variable_scan_params.scan_in_progress = 0;
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Cancel in-progress connection to a peripheral.
int mp_bluetooth_gap_peripheral_connect_cancel(void) {
    DEBUG_printf("mp_bluetooth_gap_peripheral_connect_cancel\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = sd_ble_gap_connect_cancel();

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

#endif // MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE


#if MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT

// Find all primary services on the connected peripheral.
int mp_bluetooth_gattc_discover_primary_services(uint16_t conn_handle, const mp_obj_bluetooth_uuid_t *uuid) {
    DEBUG_printf("mp_bluetooth_gattc_discover_primary_services\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = NRF_SUCCESS;

    ble_uuid_t *p_service_discovery_uuid = NULL;

    if (conn_handle >= BLE_MAX_CONN_COUNT) {
        err_code = BLE_ERROR_INVALID_CONN_HANDLE;
    } else {
        p_service_discovery_uuid = &(m_connection_handle_info[conn_handle].service_discovery_uuid);
    }

    if (err_code == NRF_SUCCESS && p_service_discovery_uuid->type != BLE_UUID_TYPE_UNKNOWN) {
        // A discovery with service UUID is already in progress.
        err_code = NRF_ERROR_BUSY;
    }

    ble_uuid_t *p_ble_service_uuid = NULL;

    if (err_code == NRF_SUCCESS && uuid != NULL) {
        // Set m_connection_handle_info[conn_handle].service_discovery_uuid.
        err_code = mp_uuid_to_ble_uuid_with_vs_add(uuid, p_service_discovery_uuid);
        p_ble_service_uuid = p_service_discovery_uuid;
    }

    if (err_code == NRF_SUCCESS) {
        const uint16_t start_handle = 1;
        err_code = sd_ble_gattc_primary_services_discover(conn_handle, start_handle, p_ble_service_uuid);
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Find all characteristics on the specified service on a connected peripheral.
int mp_bluetooth_gattc_discover_characteristics(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle, const mp_obj_bluetooth_uuid_t *uuid) {
    DEBUG_printf("mp_bluetooth_gattc_discover_characteristics\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = NRF_SUCCESS;

    ble_gattc_handle_range_t *p_char_discovery_range = NULL;
    ble_uuid_t *p_characteristic_discovery_uuid = NULL;

    if (conn_handle >= BLE_MAX_CONN_COUNT) {
        err_code = BLE_ERROR_INVALID_CONN_HANDLE;
    } else {
        p_char_discovery_range = &(m_connection_handle_info[conn_handle].char_discovery_range);
        p_characteristic_discovery_uuid = &(m_connection_handle_info[conn_handle].characteristic_discovery_uuid);
    }

    if (err_code == NRF_SUCCESS &&
        (p_char_discovery_range->start_handle != 0 ||
         p_char_discovery_range->end_handle != 0)) {
        // A discovery is already in progress.
        err_code = NRF_ERROR_BUSY;
    }

    if (err_code == NRF_SUCCESS && p_characteristic_discovery_uuid->type != BLE_UUID_TYPE_UNKNOWN) {
        // A discovery with characteristic UUID is already in progress.
        err_code = NRF_ERROR_BUSY;
    }

    if (err_code == NRF_SUCCESS && uuid != NULL) {
        // Set m_connection_handle_info[conn_handle].characteristic_discovery_uuid.
        err_code = mp_uuid_to_ble_uuid_with_vs_add(uuid, p_characteristic_discovery_uuid);
    }

    if (err_code == NRF_SUCCESS) {
        // Set m_connection_handle_info[conn_handle].char_discovery_range.
        p_char_discovery_range->start_handle = start_handle;
        p_char_discovery_range->end_handle = end_handle;

        err_code = sd_ble_gattc_characteristics_discover(conn_handle, p_char_discovery_range);
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Find all descriptors on the specified characteristic on a connected peripheral.
int mp_bluetooth_gattc_discover_descriptors(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle) {
    DEBUG_printf("mp_bluetooth_gattc_discover_descriptors\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = NRF_SUCCESS;

    ble_gattc_handle_range_t *p_desc_discovery_range = NULL;

    if (conn_handle >= BLE_MAX_CONN_COUNT) {
        err_code = BLE_ERROR_INVALID_CONN_HANDLE;
    } else {
        p_desc_discovery_range = &(m_connection_handle_info[conn_handle].desc_discovery_range);
    }

    if (err_code == NRF_SUCCESS &&
        (p_desc_discovery_range->start_handle != 0 ||
         p_desc_discovery_range->end_handle != 0)) {
        // A discovery is already in progress.
        err_code = NRF_ERROR_BUSY;
    }

    if (err_code == NRF_SUCCESS) {
        // Set m_connection_handle_info[conn_handle].desc_discovery_range.
        p_desc_discovery_range->start_handle = start_handle;
        p_desc_discovery_range->end_handle = end_handle;

        err_code = sd_ble_gattc_descriptors_discover(conn_handle, p_desc_discovery_range);
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Initiate read of a value from the remote peripheral.
//
// Note:
// Reading Long Characteristic Values is not supported.
// I.e. for characteristics values that are longer than (ATT_MTU - 1)
// only (ATT_MTU - 1) bytes will be read.
int mp_bluetooth_gattc_read(uint16_t conn_handle, uint16_t value_handle) {
    DEBUG_printf("mp_bluetooth_gattc_read\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    // offset=0 means 'Read Characteristic Value' otherwise
    // 'Read Long Characteristic Value' will be done.
    const uint16_t offset = 0;
    uint32_t err_code = sd_ble_gattc_read(conn_handle, value_handle, offset);

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Write the value to the remote peripheral.
int mp_bluetooth_gattc_write(uint16_t conn_handle, uint16_t value_handle, const uint8_t *value, size_t value_len, unsigned int mode) {
    DEBUG_printf("mp_bluetooth_gattc_write\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = NRF_SUCCESS;

    ble_gattc_write_params_t write_params;
    memset(&write_params, 0, sizeof(write_params));

    if (mode == MP_BLUETOOTH_WRITE_MODE_NO_RESPONSE) {
        write_params.write_op = BLE_GATT_OP_WRITE_CMD;
    } else if (mode == MP_BLUETOOTH_WRITE_MODE_WITH_RESPONSE) {
        write_params.write_op = BLE_GATT_OP_WRITE_REQ;
    } else {
        err_code = NRF_ERROR_INVALID_PARAM;
    }

    if (err_code == NRF_SUCCESS) {
        // only used for BLE_GATT_OP_EXEC_WRITE_REQ
        // write_params.flags    = BLE_GATT_EXEC_WRITE_FLAG_PREPARED_CANCEL;
        write_params.handle = value_handle;
        // write_params.offset   = 0;
        write_params.len = value_len;
        write_params.p_value = value;

        err_code = sd_ble_gattc_write(conn_handle, &write_params);
    }

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}

// Initiate MTU exchange for a specific connection using the preferred MTU.
int mp_bluetooth_gattc_exchange_mtu(uint16_t conn_handle) {
    DEBUG_printf("mp_bluetooth_gattc_exchange_mtu\n");

    if (!mp_bluetooth_is_active()) {
        return ERRNO_BLUETOOTH_NOT_ACTIVE;
    }

    uint32_t err_code = sd_ble_gattc_exchange_mtu_request(conn_handle, m_prefered_mtu);

    int errno_result = nrf_error_to_errno(err_code);
    DEBUG_printf("return %d\n", errno_result);
    return errno_result;
}
#endif // MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT


#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

// Convert a pass key array from a ble_gap_evt_passkey_display_t struct
// to an integer value.
void passkey_to_value(const uint8_t passkey[6], int *p_result_value) {

    int result_value = 0;

    for (int passkey_index = 0; passkey_index < 6; passkey_index++) {
        int digit_value = (char)passkey[passkey_index] - '0';

        if (digit_value >= 0 && digit_value <= 9) {
            result_value = result_value * 10 + digit_value;
        }
    }

    *p_result_value = result_value;
}

#endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING


#define MP_ADV_TYPE_CONN_IND (0)
#define MP_ADV_TYPE_DIRECT_IND (1)
#define MP_ADV_TYPE_SCAN_IND (2)
#define MP_ADV_TYPE_NONCONN_IND (3)
#define MP_ADV_TYPE_SCAN_RSP (4)

// Convert ble_gap_adv_report_type_t to MP_ADV_TYPE_* value
static uint8_t ble_gap_adv_report_type_to_mp_adv_type(ble_gap_adv_report_type_t ble_gap_adv_report_type) {
    uint8_t result;

    // connectable and scannable is set together with scan_response.
    // So check scan_response first.
    if (ble_gap_adv_report_type.scan_response) {
        result = MP_ADV_TYPE_SCAN_RSP;
    } else if (ble_gap_adv_report_type.directed) {
        result = MP_ADV_TYPE_DIRECT_IND;
    } else if (ble_gap_adv_report_type.connectable) {
        result = MP_ADV_TYPE_CONN_IND;
    } else if (ble_gap_adv_report_type.scannable) {
        result = MP_ADV_TYPE_SCAN_IND;
    } else {
        result = MP_ADV_TYPE_NONCONN_IND;
    }

    return result;
}

// called from softdevice_drv.c
void modbluetooth_ble_evt_handler(const ble_evt_t *p_ble_evt) {

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED: {
            DEBUG_printf("BLE_GAP_EVT_CONNECTED - min_conn_interval: %u, max_conn_interval: %u, slave_latency: %u, conn_sup_timeout: %u\n",
                p_ble_evt->evt.gap_evt.params.connected.conn_params.min_conn_interval,
                p_ble_evt->evt.gap_evt.params.connected.conn_params.max_conn_interval,
                p_ble_evt->evt.gap_evt.params.connected.conn_params.slave_latency,
                p_ble_evt->evt.gap_evt.params.connected.conn_params.conn_sup_timeout);


            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            uint8_t role = p_ble_evt->evt.gap_evt.params.connected.role;

            m_connection_handle_info[conn_handle].role = role;

            uint16_t irq_event;

            if (role == BLE_GAP_ROLE_CENTRAL) {
                m_static_flags.connect_in_progress = 0;
                irq_event = MP_BLUETOOTH_IRQ_PERIPHERAL_CONNECT;
            } else {
                m_static_flags.adv_in_progress = 0;
                irq_event = MP_BLUETOOTH_IRQ_CENTRAL_CONNECT;
            }

            uint8_t mp_addr_type;
            uint8_t mp_addr[6];
            ble_gap_addr_to_mp_addr(&p_ble_evt->evt.gap_evt.params.connected.peer_addr, &mp_addr_type, mp_addr);

            mp_bluetooth_gap_on_connected_disconnected(irq_event, conn_handle, mp_addr_type, mp_addr);
            break;
        }

        case BLE_GAP_EVT_DISCONNECTED: {
            DEBUG_printf("BLE_GAP_EVT_DISCONNECTED - reason: 0x%02x\n",
                (uint16_t)p_ble_evt->evt.gap_evt.params.disconnected.reason);

            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            uint8_t role = m_connection_handle_info[conn_handle].role;

            // Clear connection related infos.
            memset(&(m_connection_handle_info[conn_handle]), 0, sizeof(m_connection_info_t));
            m_connection_handle_key_lengths[conn_handle] = 0;

            uint16_t irq_event;

            if (role == BLE_GAP_ROLE_CENTRAL) {
                irq_event = MP_BLUETOOTH_IRQ_PERIPHERAL_DISCONNECT;
            } else {
                irq_event = MP_BLUETOOTH_IRQ_CENTRAL_DISCONNECT;
            }

            // We don't know the peer address at this point. So supply an empty address.
            // See modbluetooth_btstack.c - btstack_packet_handler_generic().
            uint8_t mp_addr_type = 0xff;
            uint8_t mp_addr[6];
            memset(mp_addr, 0, sizeof(mp_addr));

            mp_bluetooth_gap_on_connected_disconnected(irq_event, conn_handle, mp_addr_type, mp_addr);
            break;
        }

        case BLE_GAP_EVT_CONN_PARAM_UPDATE: {
            DEBUG_printf("BLE_GAP_EVT_CONN_PARAM_UPDATE\n");

            const ble_gap_conn_params_t *p_conn_params = &(p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params);

            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            // min_conn_interval is equal max_conn_interval for that event
            uint16_t conn_interval = p_conn_params->min_conn_interval;
            uint16_t conn_latency = p_conn_params->slave_latency;
            uint16_t supervision_timeout = p_conn_params->conn_sup_timeout;

            const uint16_t status = 0;
            mp_bluetooth_gap_on_connection_update(conn_handle, conn_interval, conn_latency, supervision_timeout, status);
            break;
        }

        case BLE_GATTS_EVT_WRITE: {
            DEBUG_printf("BLE_GATTS_EVT_WRITE\n");

            uint16_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
            uint16_t attr_handle = p_ble_evt->evt.gatts_evt.params.write.handle;

            // A client has written to this characteristic or descriptor.
            mp_bluetooth_gatts_on_write(conn_handle, attr_handle);
            break;
        }

        case BLE_GATTS_EVT_HVC: {
            DEBUG_printf("BLE_GATTS_EVT_HVC\n");

            uint16_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
            uint16_t value_handle = p_ble_evt->evt.gatts_evt.params.hvc.handle;
            uint8_t status = 0;

            // A client has acknowledged a value indication.
            // Note: Status will be zero on successful acknowledgment, implementation-specific value otherwise.
            mp_bluetooth_gatts_on_indicate_complete(conn_handle, value_handle, status);
            break;
        }

        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST: {
            // Client requests a MTU.
            uint16_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;

            uint16_t client_mtu = p_ble_evt->evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;
            client_mtu = MAX(client_mtu, BLE_GATT_ATT_MTU_DEFAULT);
            uint16_t effective_att_mtu = MIN(client_mtu, m_prefered_mtu);

            DEBUG_printf("BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST - conn_handle: %u, client_mtu: %u, effective_att_mtu: %u\n",
                conn_handle, client_mtu, effective_att_mtu);

            uint32_t err_code = sd_ble_gatts_exchange_mtu_reply(conn_handle, effective_att_mtu);

            if (err_code == NRF_SUCCESS) {
                mp_bluetooth_gatts_on_mtu_exchanged(conn_handle, effective_att_mtu);
            } else {
                DEBUG_printf("sd_ble_gatts_exchange_mtu_reply() returned %s.\n",
                    nrf_strerror_get(err_code));
            }

            break;
        }

        case BLE_GATTC_EVT_EXCHANGE_MTU_RSP: {
            // Server responded to mp_bluetooth_gattc_exchange_mtu().
            DEBUG_printf("BLE_GATTC_EVT_EXCHANGE_MTU_RSP\n");

            uint16_t conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;

            uint16_t server_rx_mtu = p_ble_evt->evt.gattc_evt.params.exchange_mtu_rsp.server_rx_mtu;

            mp_bluetooth_gatts_on_mtu_exchanged(conn_handle, server_rx_mtu);
            break;
        }

            #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
            #if DEBUG_MODBLUETOOTH

        // The following events are handled by the Peer Manager.
        // They are handled here to print debug information.

        case BLE_GAP_EVT_AUTH_STATUS: {
            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            const ble_gap_evt_auth_status_t *p_auth_status = &(p_ble_evt->evt.gap_evt.params.auth_status);

            DEBUG_printf("BLE_GAP_EVT_AUTH_STATUS - conn_handle: %u, auth_status: 0x%02x, error_src: 0x%02x, bonded: %u, lesc: %u, " \
                "kdist_own: enc=%u, id=%u, sign=%u, link=%u, kdist_peer: enc=%u, id=%u, sign=%u, link=%u\n",
                conn_handle, (uint16_t)p_auth_status->auth_status, (uint16_t)p_auth_status->error_src,
                (uint16_t)p_auth_status->bonded, (uint16_t)p_auth_status->lesc,
                (uint16_t)p_auth_status->kdist_own.enc, (uint16_t)p_auth_status->kdist_own.id,
                (uint16_t)p_auth_status->kdist_own.sign, (uint16_t)p_auth_status->kdist_own.link,
                (uint16_t)p_auth_status->kdist_peer.enc, (uint16_t)p_auth_status->kdist_peer.id,
                (uint16_t)p_auth_status->kdist_peer.sign, (uint16_t)p_auth_status->kdist_peer.link);
            break;
        }

        case BLE_GAP_EVT_SEC_REQUEST: {
            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            const ble_gap_evt_sec_request_t *p_sec_request = &(p_ble_evt->evt.gap_evt.params.sec_request);

            DEBUG_printf("BLE_GAP_EVT_SEC_REQUEST - conn_handle: %u, bond: %u, mitm: %u, lesc: %u, keypress: %u\n",
                conn_handle, (uint16_t)p_sec_request->bond, (uint16_t)p_sec_request->mitm,
                (uint16_t)p_sec_request->lesc, (uint16_t)p_sec_request->keypress);
            break;
        }

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST: {
            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            const ble_gap_sec_params_t *p_peer_params =
                &(p_ble_evt->evt.gap_evt.params.sec_params_request.peer_params);

            DEBUG_printf("BLE_GAP_EVT_SEC_PARAMS_REQUEST - conn_handle: %u, bond: %u, mitm: %u, lesc: %u, keypress: %u, io_caps: 0x%02x\n" \
                "kdist_own: enc=%u, id=%u, sign=%u, link=%u, kdist_peer: enc=%u, id=%u, sign=%u, link=%u\n",
                conn_handle,
                (uint16_t)p_peer_params->bond,
                (uint16_t)p_peer_params->mitm,
                (uint16_t)p_peer_params->lesc,
                (uint16_t)p_peer_params->keypress,
                (uint16_t)p_peer_params->io_caps,
                (uint16_t)p_peer_params->kdist_own.enc,
                (uint16_t)p_peer_params->kdist_own.id,
                (uint16_t)p_peer_params->kdist_own.sign,
                (uint16_t)p_peer_params->kdist_own.link,
                (uint16_t)p_peer_params->kdist_peer.enc,
                (uint16_t)p_peer_params->kdist_peer.id,
                (uint16_t)p_peer_params->kdist_peer.sign,
                (uint16_t)p_peer_params->kdist_peer.link);
            break;
        }

            #endif     // DEBUG_MODBLUETOOTH

        case BLE_GAP_EVT_CONN_SEC_UPDATE: {
            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            const ble_gap_conn_sec_t *p_conn_sec =
                &(p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec);

            DEBUG_printf("BLE_GAP_EVT_CONN_SEC_UPDATE - conn_handle: %u, sm: %u, lv: %u, encr_key_size: %u\n",
                conn_handle,
                (uint16_t)p_conn_sec->sec_mode.sm,
                (uint16_t)p_conn_sec->sec_mode.lv,
                (uint16_t)p_conn_sec->encr_key_size);

            m_connection_handle_key_lengths[conn_handle] = p_conn_sec->encr_key_size;

            break;
        }

        case BLE_GAP_EVT_PASSKEY_DISPLAY: {
            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            const ble_gap_evt_passkey_display_t *p_passkey_display =
                &(p_ble_evt->evt.gap_evt.params.passkey_display);

            int passkey_value = 0;

            passkey_to_value(p_passkey_display->passkey, &passkey_value);

            DEBUG_printf(
                "BLE_GAP_EVT_PASSKEY_DISPLAY - conn_handle: %u, " \
                "passkey: %.6s,  match_request: %u, " \
                "passkey_value: %u\n",
                conn_handle,
                (char *)p_passkey_display->passkey,
                (uint16_t)p_passkey_display->match_request,
                (uint16_t)passkey_value);

            uint8_t action = p_passkey_display->match_request ?
                MP_BLUETOOTH_PASSKEY_ACTION_NUMERIC_COMPARISON :
                MP_BLUETOOTH_PASSKEY_ACTION_DISPLAY;

            mp_bluetooth_gap_on_passkey_action(conn_handle, action, passkey_value);

            break;
        }

        case BLE_GAP_EVT_AUTH_KEY_REQUEST: {
            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            const ble_gap_evt_auth_key_request_t *p_auth_key_request =
                &(p_ble_evt->evt.gap_evt.params.auth_key_request);

            DEBUG_printf(
                "BLE_GAP_EVT_AUTH_KEY_REQUEST - conn_handle: %u, " \
                "key_type: %u\n",
                conn_handle,
                (uint16_t)p_auth_key_request->key_type);

            if (p_auth_key_request->key_type == BLE_GAP_AUTH_KEY_TYPE_PASSKEY) {
                mp_bluetooth_gap_on_passkey_action(
                    conn_handle, MP_BLUETOOTH_PASSKEY_ACTION_INPUT, 0);
            }

            break;
        }

        case BLE_GAP_EVT_LESC_DHKEY_REQUEST: {
            #if DEBUG_MODBLUETOOTH
            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            DEBUG_printf(
                "BLE_GAP_EVT_LESC_DHKEY_REQUEST - conn_handle: %u\n",
                conn_handle);
            #endif // DEBUG_MODBLUETOOTH

            m_is_lesc_dhkey_request_pendig = true;
            break;
        }

            #else // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST: {
            DEBUG_printf("BLE_GAP_EVT_SEC_PARAMS_REQUEST\n");

            uint16_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
            // pairing not supported
            (void)sd_ble_gap_sec_params_reply(conn_handle,
                BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
                NULL, NULL);
            break;
        }

            #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING


            #if DEBUG_MODBLUETOOTH
        #if (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)
        case BLE_GATTS_EVT_HVN_TX_COMPLETE: {
            DEBUG_printf("BLE_GATTS_EVT_HVN_TX_COMPLETE, count: %u\n", p_ble_evt->evt.gatts_evt.params.hvn_tx_complete.count);
            break;
        }
        #else
        case BLE_EVT_TX_COMPLETE: {
            DEBUG_printf("BLE_EVT_TX_COMPLETE, count: %u\n", p_ble_evt->evt.common_evt.params.tx_complete.count);
            break;
        }
        #endif // (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)
            #endif // DEBUG_MODBLUETOOTH

            #if MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE

        case BLE_GAP_EVT_ADV_REPORT: {
            const ble_gap_evt_adv_report_t *p_adv_report = &(p_ble_evt->evt.gap_evt.params.adv_report);

            // ATTENTION
            // ---------
            // With debug output a scan response that normally follows the
            // "normal advertising packet" will be lost because the window
            // for sending the scan request will be missed.
            //
            // See "https://devzone.nordicsemi.com/f/nordic-q-a/92376/ble_gap_evt_adv_report-generation-for-active-scan":
            //
            // "... If a advertiser is doing scannable advertising, it will transmit the normal
            // advertising packet first, and after it have done that, it will for a short period
            // of time stay in RX mode, listening for a scan response request from a potential
            // scanner. The scanner needs to send the request in this window. ..."

//            DEBUG_printf("BLE_GAP_EVT_ADV_REPORT - connectable: %u, scannable: %u, directed: %u, scan_response: %u, status: 0x%02x\n",
//                (uint16_t)p_adv_report->type.connectable,
//                (uint16_t)p_adv_report->type.scannable,
//                (uint16_t)p_adv_report->type.directed,
//                (uint16_t)p_adv_report->type.scan_response,
//                (uint16_t)p_adv_report->type.status);

            uint8_t mp_addr_type;
            uint8_t mp_addr[6];
            ble_gap_addr_to_mp_addr(&(p_adv_report->peer_addr), &mp_addr_type, mp_addr);

            uint8_t mp_adv_type = ble_gap_adv_report_type_to_mp_adv_type(p_adv_report->type);
            int8_t rssi = p_adv_report->rssi;

            const uint8_t *data = p_adv_report->data.p_data;
            uint16_t data_len = p_adv_report->data.len;

            // Notify modbluetooth of a scan result.
            mp_bluetooth_gap_on_scan_result(mp_addr_type, mp_addr, mp_adv_type, rssi, data, data_len);

//            #if DEBUG_MODBLUETOOTH
//            // Continue scan.
//            uint32_t err_code = ble_gap_scan_start_continue(NULL);
//
//            DEBUG_printf("ble_gap_scan_start_continue(NULL) - result: %lu\n", err_code);
//            #else // DEBUG_MODBLUETOOTH
            ble_gap_scan_start_continue(NULL);
//            #endif // DEBUG_MODBLUETOOTH
            break;
        }

            #endif // #if MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE


            #if MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT

        case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP: {
            DEBUG_printf("BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP, count: %u\n",
                p_ble_evt->evt.gattc_evt.params.prim_srvc_disc_rsp.count);

            uint16_t conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;
            uint16_t gatt_status = p_ble_evt->evt.gattc_evt.gatt_status;

            uint32_t err_code = NRF_SUCCESS;

            if (gatt_status == BLE_GATT_STATUS_SUCCESS) {
                uint16_t end_handle = 0xffff;

                for (uint16_t i = 0; i < p_ble_evt->evt.gattc_evt.params.prim_srvc_disc_rsp.count; i++) {
                    const ble_gattc_service_t *p_service = &(p_ble_evt->evt.gattc_evt.params.prim_srvc_disc_rsp.services[i]);

                    uint16_t start_handle = p_service->handle_range.start_handle;
                    end_handle = p_service->handle_range.end_handle;

                    mp_obj_bluetooth_uuid_t mp_service_uuid;
                    uint32_t uuid_err_code = ble_uuid_to_mp_uuid(&p_service->uuid, &mp_service_uuid);

                    DEBUG_printf("BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP - Service - start_handle: %u, end_handle: %u, " \
                        "ble_uuid.type: %u, ble_uuid.uuid: 0x%04x, ble_uuid_to_mp_uuid() result: 0x%04lx, mp_uuid.type: %u\n",
                        start_handle, end_handle,
                        (uint16_t)p_service->uuid.type, p_service->uuid.uuid, uuid_err_code, mp_service_uuid.type);

                    if (uuid_err_code == NRF_SUCCESS) {
                        // Notify modbluetooth that a service was found (either by discover-all, or discover-by-uuid).
                        mp_bluetooth_gattc_on_primary_service_result(conn_handle, start_handle, end_handle, &mp_service_uuid);
                    }
                }

                // Continue service discovery.

                ble_uuid_t *p_service_discovery_uuid = NULL;

                ble_uuid_t *p_ble_uuid = &(m_connection_handle_info[conn_handle].service_discovery_uuid);

                if (p_ble_uuid->type != BLE_UUID_TYPE_UNKNOWN) {
                    // Continue service discovery with service UUID.
                    p_service_discovery_uuid = p_ble_uuid;
                }

                err_code = sd_ble_gattc_primary_services_discover(
                    conn_handle, end_handle + 1, p_service_discovery_uuid);
            }

            if (gatt_status != BLE_GATT_STATUS_SUCCESS || err_code != NRF_SUCCESS) {
                // Service discovery finished.

                // Clear m_connection_handle_info[conn_handle].service_discovery_uuid.
                memset(&(m_connection_handle_info[conn_handle].service_discovery_uuid), 0, sizeof(ble_uuid_t));

                // Emit mp_bluetooth_gattc_on_discover_complete().

                uint16_t mp_status = gatt_status;

                if (gatt_status == BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND) {
                    // All attributes has been discovered - no error.
                    mp_status = 0;
                } else if (gatt_status == BLE_GATT_STATUS_SUCCESS) {
                    // Use NRF_* error code as status.
                    mp_status = err_code;
                }

                mp_bluetooth_gattc_on_discover_complete(MP_BLUETOOTH_IRQ_GATTC_SERVICE_DONE, conn_handle, mp_status);
            }
            break;
        }

        case BLE_GATTC_EVT_CHAR_DISC_RSP: {
            DEBUG_printf("BLE_GATTC_EVT_CHAR_DISC_RSP, count: %u\n",
                p_ble_evt->evt.gattc_evt.params.char_disc_rsp.count);

            uint16_t conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;
            uint16_t gatt_status = p_ble_evt->evt.gattc_evt.gatt_status;

            uint32_t err_code = NRF_SUCCESS;

            if (gatt_status == BLE_GATT_STATUS_SUCCESS) {

                ble_uuid_t *p_ble_uuid = &(m_connection_handle_info[conn_handle].characteristic_discovery_uuid);

                uint16_t last_value_handle = 0xffff;

                for (uint16_t i = 0; i < p_ble_evt->evt.gattc_evt.params.char_disc_rsp.count; i++) {
                    const ble_gattc_char_t *p_char = &(p_ble_evt->evt.gattc_evt.params.char_disc_rsp.chars[i]);

                    // end_handle should be the handle of the last characteristic descriptor
                    // of this characteristic if there is one. Otherwise end_handle should
                    // be the characteristic value handle.
                    // But we couldn't evaluate that.
                    // So set end_handle to an invalid value like it is done in
                    // report_gatt_characteristics() in 'modbluetooth_btstack.c'.
                    uint16_t end_handle = p_char->handle_decl - 1;
                    uint16_t value_handle = p_char->handle_value;

                    DEBUG_printf("BLE_GATTC_EVT_CHAR_DISC_RSP - Characteristic - value_handle: %u, " \
                        "ble_uuid.type: %u, ble_uuid.uuid: 0x%04x\n",
                        value_handle, (uint16_t)p_char->uuid.type, p_char->uuid.uuid);

                    // Filter for m_connection_handle_info[conn_handle].characteristic_discovery_uuid.
                    if (p_ble_uuid->type == BLE_UUID_TYPE_UNKNOWN ||
                        (p_ble_uuid->type == p_char->uuid.type && p_ble_uuid->uuid == p_char->uuid.uuid)) {
                        // No Characteristic UUID filter set or Characteristic UUID matches.

                        mp_obj_bluetooth_uuid_t mp_characteristic_uuid;
                        uint32_t uuid_err_code = ble_uuid_to_mp_uuid(&p_char->uuid, &mp_characteristic_uuid);

                        DEBUG_printf("... ble_uuid_to_mp_uuid() result: 0x%04lx, mp_uuid.type: %u\n",
                            uuid_err_code, mp_characteristic_uuid.type);

                        if (uuid_err_code == NRF_SUCCESS) {
                            uint8_t mp_properties =
                                ble_gatt_char_props_to_mp_characteristic_properties(&(p_char->char_props));

                            // Notify modbluetooth that a characteristic was found (either by discover-all-on-service, or discover-by-uuid-on-service).
                            mp_bluetooth_gattc_on_characteristic_result(conn_handle, value_handle, end_handle, mp_properties, &mp_characteristic_uuid);
                        }
                    }

                    last_value_handle = value_handle;
                }

                // Check if discovery has to be continued.

                ble_gattc_handle_range_t *p_char_discovery_range =
                    &(m_connection_handle_info[conn_handle].char_discovery_range);

                // Update m_connection_handle_info[conn_handle].char_discovery_range.
                p_char_discovery_range->start_handle = last_value_handle + 1;

                if (p_char_discovery_range->start_handle > p_char_discovery_range->end_handle) {
                    // Characteristics discovery finished.
                    gatt_status = BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND;
                } else {
                    // Continue characteristics discovery.
                    err_code = sd_ble_gattc_characteristics_discover(conn_handle, p_char_discovery_range);
                }
            }

            if (gatt_status != BLE_GATT_STATUS_SUCCESS || err_code != NRF_SUCCESS) {
                // Characteristics discovery finished.

                // Clear m_connection_handle_info[conn_handle].char_discovery_range.
                memset(&(m_connection_handle_info[conn_handle].char_discovery_range), 0, sizeof(ble_gattc_handle_range_t));

                // Clear m_connection_handle_info[conn_handle].characteristic_discovery_uuid.
                memset(&(m_connection_handle_info[conn_handle].characteristic_discovery_uuid), 0, sizeof(ble_uuid_t));

                // Emit mp_bluetooth_gattc_on_discover_complete().

                uint16_t mp_status = gatt_status;

                if (gatt_status == BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND) {
                    // All attributes has been discovered - no error.
                    mp_status = 0;
                } else if (gatt_status == BLE_GATT_STATUS_SUCCESS) {
                    // Use NRF_* error code as status.
                    mp_status = err_code;
                }

                mp_bluetooth_gattc_on_discover_complete(MP_BLUETOOTH_IRQ_GATTC_CHARACTERISTIC_DONE, conn_handle, mp_status);
            }
            break;
        }

        case BLE_GATTC_EVT_DESC_DISC_RSP: {
            DEBUG_printf("BLE_GATTC_EVT_DESC_DISC_RSP, count: %d\n",
                p_ble_evt->evt.gattc_evt.params.desc_disc_rsp.count);

            uint16_t conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;
            uint16_t gatt_status = p_ble_evt->evt.gattc_evt.gatt_status;

            uint32_t err_code = NRF_SUCCESS;

            if (gatt_status == BLE_GATT_STATUS_SUCCESS) {

                uint16_t last_desc_handle = 0xffff;

                for (uint16_t i = 0; i < p_ble_evt->evt.gattc_evt.params.desc_disc_rsp.count; i++) {
                    const ble_gattc_desc_t *p_desc = &(p_ble_evt->evt.gattc_evt.params.desc_disc_rsp.descs[i]);

                    uint16_t desc_handle = p_desc->handle;

                    mp_obj_bluetooth_uuid_t mp_descriptor_uuid;
                    uint32_t uuid_err_code = ble_uuid_to_mp_uuid(&p_desc->uuid, &mp_descriptor_uuid);

                    DEBUG_printf("BLE_GATTC_EVT_DESC_DISC_RSP - Descriptor - desc_handle: %u, " \
                        "ble_uuid.type: %u, ble_uuid.uuid: 0x%04x, ble_uuid_to_mp_uuid() result: 0x%04lx, mp_uuid.type: %u\n",
                        desc_handle, (uint16_t)p_desc->uuid.type, p_desc->uuid.uuid,
                        uuid_err_code, mp_descriptor_uuid.type);

                    if (uuid_err_code == NRF_SUCCESS) {
                        // Notify modbluetooth that a descriptor was found.
                        mp_bluetooth_gattc_on_descriptor_result(conn_handle, desc_handle, &mp_descriptor_uuid);
                    }

                    last_desc_handle = desc_handle;
                }

                // Check if discovery has to be continued.

                ble_gattc_handle_range_t *p_desc_discovery_range =
                    &(m_connection_handle_info[conn_handle].desc_discovery_range);

                // Update m_connection_handle_info[conn_handle].desc_discovery_range.
                p_desc_discovery_range->start_handle = last_desc_handle + 1;

                if (p_desc_discovery_range->start_handle > p_desc_discovery_range->end_handle) {
                    // Descriptors discovery finished.
                    gatt_status = BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND;
                } else {
                    // Continue descriptors discovery.
                    err_code = sd_ble_gattc_descriptors_discover(conn_handle, p_desc_discovery_range);
                }
            }

            if (gatt_status != BLE_GATT_STATUS_SUCCESS || err_code != NRF_SUCCESS) {
                // Characteristics discovery finished.

                // Clear m_connection_handle_info[conn_handle].desc_discovery_range.
                memset(&(m_connection_handle_info[conn_handle].desc_discovery_range), 0, sizeof(ble_gattc_handle_range_t));

                // Emit mp_bluetooth_gattc_on_discover_complete().

                uint16_t mp_status = gatt_status;

                if (gatt_status == BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND) {
                    // All attributes has been discovered - no error.
                    mp_status = 0;
                } else if (gatt_status == BLE_GATT_STATUS_SUCCESS) {
                    // Use NRF_* error code as status.
                    mp_status = err_code;
                }

                mp_bluetooth_gattc_on_discover_complete(MP_BLUETOOTH_IRQ_GATTC_DESCRIPTOR_DONE, conn_handle, mp_status);
            }
            break;
        }

        case BLE_GATTC_EVT_READ_RSP: {
            const ble_gattc_evt_read_rsp_t *p_read_rsp = &(p_ble_evt->evt.gattc_evt.params.read_rsp);

            DEBUG_printf("BLE_GATTC_EVT_READ_RSP, offset: 0x%02x, length: 0x%02x\n",
                p_read_rsp->offset,
                p_read_rsp->len);

            uint16_t conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;
            uint16_t gatt_status = p_ble_evt->evt.gattc_evt.gatt_status;

            uint16_t value_handle = 0;

            if (gatt_status == BLE_GATT_STATUS_SUCCESS) {
                value_handle = p_read_rsp->handle;
                uint16_t len = p_read_rsp->len;
                const uint8_t *data = p_read_rsp->data;

                // Notify modbluetooth that a read has completed with data
                mp_bluetooth_gattc_on_data_available(MP_BLUETOOTH_IRQ_GATTC_READ_RESULT, conn_handle, value_handle, &data, &len, 1);
            }

            // Notify modbluetooth that a read operation has completed.
            mp_bluetooth_gattc_on_read_write_status(MP_BLUETOOTH_IRQ_GATTC_READ_DONE, conn_handle, value_handle, gatt_status);
            break;
        }

        case BLE_GATTC_EVT_WRITE_RSP: {
            DEBUG_printf("BLE_GATTC_EVT_WRITE_RSP\n");

            const ble_gattc_evt_write_rsp_t *p_write_rsp = &(p_ble_evt->evt.gattc_evt.params.write_rsp);

            uint16_t conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;
            uint16_t gatt_status = p_ble_evt->evt.gattc_evt.gatt_status;

            uint16_t value_handle = p_write_rsp->handle;

            // Notify modbluetooth that a write operation has completed.
            mp_bluetooth_gattc_on_read_write_status(MP_BLUETOOTH_IRQ_GATTC_WRITE_DONE, conn_handle, value_handle, gatt_status);
            break;
        }

        case BLE_GATTC_EVT_HVX: {
            DEBUG_printf("BLE_GATTC_EVT_HVX\n");

            const ble_gattc_evt_hvx_t *p_hvx = &(p_ble_evt->evt.gattc_evt.params.hvx);

            uint16_t conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;
            uint16_t gatt_status = p_ble_evt->evt.gattc_evt.gatt_status;

            if (gatt_status == BLE_GATT_STATUS_SUCCESS) {
                uint16_t value_handle = p_hvx->handle;
                uint16_t len = p_hvx->len;
                const uint8_t *data = p_hvx->data;

                if (p_hvx->type == BLE_GATT_HVX_NOTIFICATION) {
                    // Notify modbluetooth that a notify has completed with data
                    mp_bluetooth_gattc_on_data_available(MP_BLUETOOTH_IRQ_GATTC_NOTIFY, conn_handle, value_handle, &data, &len, 1);
                } else if (p_hvx->type == BLE_GATT_HVX_INDICATION) {
                    sd_ble_gattc_hv_confirm(conn_handle, value_handle);
                    // Notify modbluetooth that a indicate has completed with data
                    mp_bluetooth_gattc_on_data_available(MP_BLUETOOTH_IRQ_GATTC_INDICATE, conn_handle, value_handle, &data, &len, 1);
                }
            }
            break;
        }
            #endif // #if MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT

        #if (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)
        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST: {
            DEBUG_printf("BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST\n");

            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            (void)sd_ble_gap_conn_param_update(conn_handle,
                &p_ble_evt->evt.gap_evt.params.conn_param_update_request.conn_params);
            break;
        }

        case BLE_GATTS_EVT_SYS_ATTR_MISSING: {
            DEBUG_printf("BLE_GATTS_EVT_SYS_ATTR_MISSING\n");
            uint16_t conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
            // No system attributes have been stored.
            (void)sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
            break;
        }

        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST: {
            uint16_t conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            DEBUG_printf("BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST - conn_handle: %u, max_tx_octets: %u, max_rx_octets: %u, max_tx_time_us: %u, max_rx_time_us: %u\n",
                conn_handle,
                p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params.max_tx_octets,
                p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params.max_rx_octets,
                p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params.max_tx_time_us,
                p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params.max_rx_time_us);

            #if DEBUG_MODBLUETOOTH
            uint32_t err_code = sd_ble_gap_data_length_update(conn_handle, NULL, NULL);

            DEBUG_printf("sd_ble_gap_data_length_update() result: 0x%04lx\n", err_code);
            #else // DEBUG_MODBLUETOOTH
            sd_ble_gap_data_length_update(conn_handle, NULL, NULL);
            #endif // DEBUG_MODBLUETOOTH

            break;
        }

        case BLE_GAP_EVT_PHY_UPDATE: {
            DEBUG_printf("BLE_GAP_EVT_PHY_UPDATE\n");
            break;
        }

        case BLE_GAP_EVT_DATA_LENGTH_UPDATE: {
            DEBUG_printf("BLE_GAP_EVT_DATA_LENGTH_UPDATE - max_tx_octets: %u, max_rx_octets: %u, max_tx_time_us: %u, max_rx_time_us: %u\n",
                p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_tx_octets,
                p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_rx_octets,
                p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_tx_time_us,
                p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_rx_time_us);
            break;
        }

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
            DEBUG_printf("BLE_GAP_EVT_PHY_UPDATE_REQUEST\n");
            ble_gap_phys_t const phys =
            {
                BLE_GAP_PHY_AUTO,
                BLE_GAP_PHY_AUTO,
            };
            sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            break;
        }

        #endif // (BLUETOOTH_SD == 132) || (BLUETOOTH_SD == 140)

        case BLE_GAP_EVT_TIMEOUT: {
            const ble_gap_evt_timeout_t *p_timeout = &(p_ble_evt->evt.gap_evt.params.timeout);

            DEBUG_printf("BLE_GAP_EVT_TIMEOUT, conn_handle: %u, src: %u\n",
                p_ble_evt->evt.gap_evt.conn_handle,
                p_timeout->src);

            if (p_timeout->src == BLE_GAP_TIMEOUT_SRC_SCAN) {
                // Scan timed out.
                m_variable_scan_params.scan_in_progress = 0;
                mp_bluetooth_gap_on_scan_complete();
            } else if (p_timeout->src == BLE_GAP_TIMEOUT_SRC_CONN) {
                m_static_flags.connect_in_progress = 0;
            }
            break;
        }

        default: {
            DEBUG_printf(">>> unhandled evt: 0x%04x\n", p_ble_evt->header.evt_id);
            break;
        }
    }
}

MP_REGISTER_ROOT_POINTER(struct _mp_bluetooth_nrf_root_pointers_t *bluetooth_nrf_root_pointers);

#endif // MICROPY_PY_BLUETOOTH

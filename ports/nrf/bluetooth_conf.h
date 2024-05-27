#ifndef BLUETOOTH_CONF_H__
#define BLUETOOTH_CONF_H__


// SD specific configurations.

#ifndef USE_NRF_SDK_COMPONENTS
#define USE_NRF_SDK_COMPONENTS          (0)
#endif

#if USE_NRF_SDK_COMPONENTS
#include "sdk_config.h"
#endif


#if (BLUETOOTH_SD == 110) && USE_NRF_SDK_COMPONENTS
#error "SoftDevice 110 not supported with USE_NRF_SDK_COMPONENTS."
#endif


#if MICROPY_PY_BLUETOOTH

#define BLUETOOTH_BLE_DRV               (0)
#define MICROPY_PY_BLE                  (0)
#define MICROPY_PY_BLE_NUS              (0)
#define BLUETOOTH_WEBBLUETOOTH_REPL     (0)
#define MICROPY_PY_UBLUEPY              (0)

#define MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING (1)

// Required for MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING.
#define MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS        (1)
#define MICROPY_TRACKED_ALLOC                       (1)

#define MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS  (0)
#define MICROPY_PY_BLUETOOTH_ENABLE_HCI_CMD         (0)

#define MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE    (1)
#define MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT     (1)


#else // MICROPY_PY_BLUETOOTH


#define BLUETOOTH_BLE_DRV               (1)

#if (BLUETOOTH_SD == 110)

#define MICROPY_PY_BLE                  (1)
#define MICROPY_PY_BLE_NUS              (0)
#define BLUETOOTH_WEBBLUETOOTH_REPL     (0)
#define MICROPY_PY_UBLUEPY              (1)
#define MICROPY_PY_UBLUEPY_PERIPHERAL   (1)

#elif (BLUETOOTH_SD == 132)

#define MICROPY_PY_BLE                  (1)
#define MICROPY_PY_BLE_NUS              (0)
#define BLUETOOTH_WEBBLUETOOTH_REPL     (0)
#define MICROPY_PY_UBLUEPY              (1)
#define MICROPY_PY_UBLUEPY_PERIPHERAL   (1)
#define MICROPY_PY_UBLUEPY_CENTRAL      (1)

#elif (BLUETOOTH_SD == 140)

#define MICROPY_PY_BLE                  (1)
#define MICROPY_PY_BLE_NUS              (0)
#define BLUETOOTH_WEBBLUETOOTH_REPL     (0)
#define MICROPY_PY_UBLUEPY              (1)
#define MICROPY_PY_UBLUEPY_PERIPHERAL   (1)
#define MICROPY_PY_UBLUEPY_CENTRAL      (1)

#else
#error "SD not supported"
#endif

#endif // MICROPY_PY_BLUETOOTH


// Default defines.

#ifndef MICROPY_PY_BLE
#define MICROPY_PY_BLE                  (0)
#endif

#ifndef MICROPY_PY_BLE_NUS
#define MICROPY_PY_BLE_NUS              (0)
#endif


#ifndef NRF_SDH_BLE_GATT_MAX_MTU_SIZE
//
// Maximum size of ATT packet that can be sent or received.
// In case of USE_NRF_SDK_COMPONENTS this is defined in sdk_config.h.
//
// Note that for (BLUETOOTH_SD == 110) that have to be equal to
// BLE_GATT_ATT_MTU_DEFAULT (23).
//
#define NRF_SDH_BLE_GATT_MAX_MTU_SIZE   (23)
#endif // #ifndef NRF_SDH_BLE_GATT_MAX_MTU_SIZE

#endif

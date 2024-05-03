#ifndef BLUETOOTH_CONF_H__
#define BLUETOOTH_CONF_H__


// SD specific configurations.

#if MICROPY_PY_BLUETOOTH

#define BLUETOOTH_BLE_DRV               (0)

#if (BLUETOOTH_SD == 140)

#define MICROPY_PY_BLE                  (0)
#define MICROPY_PY_BLE_NUS              (0)
#define BLUETOOTH_WEBBLUETOOTH_REPL     (0)
#define MICROPY_PY_UBLUEPY              (0)

// TODO: Experimental.
//// Required for MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING.
// #define MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS        (1)
// #define MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING (1)

#define MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING (0)
#define MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS  (0)
#define MICROPY_PY_BLUETOOTH_ENABLE_HCI_CMD         (0)

#define MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE    (1)
#define MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT     (1)

#else // (BLUETOOTH_SD == 140)
#error "SD not supported by MICROPY_PY_BLUETOOTH"
#endif // (BLUETOOTH_SD == 140)

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

#endif

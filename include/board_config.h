#pragma once
#include <Arduino.h>

// ============================================================================
// Hardware pin map — adjust to match your actual PCB / wiring.
// ============================================================================

// Battery voltage sense pin. Must be an ADC1 pin (ADC2 conflicts with BLE
// radio and will give garbage readings while advertising).
// Wire a 2:1 voltage divider (e.g. two 100k resistors) from BAT+ to this pin
// so the ~4.2V-3.0V LiPo range fits inside the ESP32's 0-3.3V ADC range.
static const int PIN_BATTERY_ADC = 34;

// TP4056 CHRG status pin (active LOW while charging). Optional — wire to a
// GPIO if you want the firmware to know charge state; set to -1 to disable.
static const int PIN_CHARGE_STATUS = 35;

// Hold this pin LOW at boot to force PROVISIONING mode even if the node is
// already configured (this is the standard ESP32 DevKit "BOOT" button on
// GPIO0, so no extra hardware is needed for re-provisioning in the field).
static const int PIN_FORCE_PROVISION = 0;

// Onboard status LED (most DevKit boards have one on GPIO2).
static const int PIN_STATUS_LED = 2;

// How long PIN_FORCE_PROVISION must be held LOW at boot to trigger
// re-provisioning, in milliseconds.
// Window after boot during which pressing BOOT forces PROVISIONING mode.
// Press it AFTER power-on — holding GPIO0 during reset enters flashing mode.
static const uint32_t FORCE_PROVISION_WINDOW_MS = 3000;

// ============================================================================
// Temperature sensor + fan
// ============================================================================
static const int PIN_TEMP_SENSOR = 4;   // DS18B20 data, 4.7k pull-up to 3.3V
static const int PIN_FAN = 26;          // MOSFET gate via 100R, 10k gate-to-GND
static const float FAN_ON_TEMP_C  = 35.0f;
static const float FAN_OFF_TEMP_C = 32.0f;
static const uint32_t TEMP_READ_INTERVAL_MS = 2000;
static const bool FAN_ON_IF_SENSOR_FAILS = true;

// ============================================================================
// BLE timing
// ============================================================================

// Advertising interval while in NAVIGATION mode. Faster = smoother phone-side
// trilateration but higher power draw. 200-300ms is a good balance for a
// beacon that needs to run for weeks on a small LiPo.
static const uint16_t ADV_INTERVAL_MIN_MS = 100;
static const uint16_t ADV_INTERVAL_MAX_MS = 100;
// Scan response (battery/temp/fan) refresh; also refreshed when the fan switches.
static const uint32_t STATUS_REFRESH_INTERVAL_MS = 15UL * 1000UL;
// Low-battery LED check interval.
static const uint32_t BATTERY_CHECK_INTERVAL_MS = 60UL * 1000UL;
// How long PROVISIONING mode stays open with no technician connection before
// the node gives up and falls back to whatever config it already had (or
// stays in provisioning forever if it has never been configured).
static const uint32_t PROVISIONING_TIMEOUT_MS = 5UL * 60UL * 1000UL; // 5 min

// ============================================================================
// Provisioning GATT service — random but fixed UUIDs, unique to this product
// line so technician apps can filter scan results to only "our" hardware.
// ============================================================================

static const char* PROVISIONING_SERVICE_UUID   = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CHAR_MALL_UUID_UUID         = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // write/read - "global id"
static const char* CHAR_FLOOR_MAJOR_UUID       = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // write/read - "local id"
static const char* CHAR_NODE_MINOR_UUID        = "6e400004-b5a3-f393-e0a9-e50e24dcca9e"; // write/read - unique node id
static const char* CHAR_NODE_LABEL_UUID        = "6e400005-b5a3-f393-e0a9-e50e24dcca9e"; // write/read - human label
static const char* CHAR_TX_POWER_UUID          = "6e400006-b5a3-f393-e0a9-e50e24dcca9e"; // write/read - calibration
static const char* CHAR_COMMAND_UUID           = "6e400007-b5a3-f393-e0a9-e50e24dcca9e"; // write only
static const char* CHAR_STATUS_UUID            = "6e400008-b5a3-f393-e0a9-e50e24dcca9e"; // read/notify
static const char* CHAR_SERIAL_UUID            = "6e400009-b5a3-f393-e0a9-e50e24dcca9e"; // read only

static const char* FIRMWARE_VERSION = "1.0.0";

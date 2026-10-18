// ============================================================================
//  Indoor Navigation Beacon Node — generic firmware
//
//  Every unit of this hardware ships with this exact same firmware. It has
//  no knowledge of "which mall" or "which floor" until a technician
//  provisions it over BLE. Two modes:
//
//    PROVISIONING  — connectable GATT server, technician app writes config
//    NAVIGATION    — non-connectable iBeacon broadcast, this is normal
//                     day-to-day operation once configured
//
//  Mode is chosen at boot:
//    - no saved config yet                -> PROVISIONING
//    - BOOT button held at power-on       -> PROVISIONING (re-configure)
//    - otherwise                          -> NAVIGATION
// ============================================================================

#include <Arduino.h>
#include "board_config.h"
#include "config_manager.h"
#include "battery_monitor.h"
#include "fan_controller.h"
#include "ble_beacon.h"
#include "ble_provisioning.h"
#include "serial_config.h"

enum class NodeMode { PROVISIONING, NAVIGATION };

static ConfigManager configManager;
static BatteryMonitor batteryMonitor;
static FanController fanController;
static BleBeacon bleBeacon;
static BleProvisioning bleProvisioning;
static SerialConfiguration serialConfig;

static NodeMode currentMode;
static uint32_t lastStatusRefresh = 0;
static uint32_t lastBatteryCheck = 0;
static bool lastFanState = false;

// Blink pattern just for visual sanity-checking during bring-up/demo — not
// required for the system to function, safe to remove or expand.
static void blinkStatusLed(int times, int onMs = 100, int offMs = 100) {
    for (int i = 0; i < times; i++) {
        digitalWrite(PIN_STATUS_LED, HIGH);
        delay(onMs);
        digitalWrite(PIN_STATUS_LED, LOW);
        delay(offMs);
    }
}

// Returns true if the BOOT button is held LOW for the configured duration —
// used to force re-entry into PROVISIONING mode on an already-configured
// node, so a technician can walk up to any deployed unit and reconfigure it
// without needing a factory reset first.
static bool forceProvisioningRequested() {
    pinMode(PIN_FORCE_PROVISION, INPUT_PULLUP);
    Serial.printf("[Boot] Press BOOT within %lu s to enter PROVISIONING mode...\n",
                  (unsigned long) (FORCE_PROVISION_WINDOW_MS / 1000));
    uint32_t start = millis();
    uint32_t pressedSince = 0;
    bool pressed = false;

    while (millis() - start < FORCE_PROVISION_WINDOW_MS) {
        digitalWrite(PIN_STATUS_LED, ((millis() - start) / 100) % 2 ? HIGH : LOW);
        if (digitalRead(PIN_FORCE_PROVISION) == LOW) {
            if (!pressed) { pressed = true; pressedSince = millis(); }
            else if (millis() - pressedSince >= 50) { digitalWrite(PIN_STATUS_LED, LOW); return true; }
        } else {
            pressed = false;
        }
        delay(10);
    }
    digitalWrite(PIN_STATUS_LED, LOW);
    return false;
}

void setup() {
    setCpuFrequencyMhz(80);
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[Boot] Indoor Navigation Beacon Node starting...");
    Serial.printf("[Boot] Firmware version: %s\n", FIRMWARE_VERSION);

    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    fanController.begin(PIN_TEMP_SENSOR, PIN_FAN);
    batteryMonitor.begin(PIN_BATTERY_ADC, PIN_CHARGE_STATUS);
    configManager.begin();
    serialConfig.begin(configManager, batteryMonitor, fanController);

    bool forceProvision = forceProvisioningRequested();
    const NodeConfig& cfg = configManager.get();

    if (!cfg.configured || forceProvision) {
        currentMode = NodeMode::PROVISIONING;
        Serial.println(forceProvision
            ? "[Boot] BOOT button pressed -> forcing PROVISIONING mode."
            : "[Boot] No saved config found -> entering PROVISIONING mode.");
        bleProvisioning.begin(configManager, batteryMonitor);
        blinkStatusLed(3, 200, 200); // slow triple-blink = "waiting to be configured"
    } else {
        currentMode = NodeMode::NAVIGATION;
        Serial.println("[Boot] Configured node -> entering NAVIGATION mode.");
        bleBeacon.begin(cfg, batteryMonitor, fanController);
        blinkStatusLed(1, 500, 0); // single long blink = "broadcasting normally"
    }

    lastStatusRefresh = millis();
    lastBatteryCheck = millis();
    lastFanState = fanController.isFanOn();
}

void loop() {
    serialConfig.poll(); // SET_UUID / SAVE / RESET / STATUS etc. work regardless of current mode
    fanController.update();
    if (currentMode == NodeMode::PROVISIONING) {
        // GATT writes are handled entirely by NimBLE callbacks in the
        // background — loop() just watches for a command that needs to
        // change firmware state (reboot / factory reset).

        if (bleProvisioning.shouldFactoryReset()) {
            Serial.println("[Loop] Executing factory reset...");
            configManager.factoryReset();
            delay(300);
            ESP.restart();
        }

        if (bleProvisioning.shouldReboot()) {
            Serial.println("[Loop] Config saved, rebooting into NAVIGATION mode...");
            blinkStatusLed(2, 150, 150);
            delay(300); // let the BLE "OK:SAVED" notify actually get sent before we drop the connection
            ESP.restart();
        }

        // Optional safety valve: if a node has never been configured and
        // nobody shows up to provision it within the timeout, we just keep
        // waiting (there's nothing useful to fall back to). If it's an
        // already-configured node that was force-provisioned and the
        // technician walks away, fall back to normal beaconing instead of
        // sitting there uselessly forever.
                if (configManager.get().configured &&
            !bleProvisioning.isClientConnected() &&
            millis() - bleProvisioning.lastActivityMs() > PROVISIONING_TIMEOUT_MS) {
            Serial.println("[Loop] Provisioning timeout (no technician activity) -> reverting to NAVIGATION mode.");
            ESP.restart();
        }

       } else { // NAVIGATION mode
        bool fanChanged = fanController.isFanOn() != lastFanState;
        if (fanChanged || millis() - lastStatusRefresh >= STATUS_REFRESH_INTERVAL_MS) {
            bleBeacon.refreshStatus(batteryMonitor, fanController);
            lastFanState = fanController.isFanOn();
            lastStatusRefresh = millis();
        }
        if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL_MS) {
            lastBatteryCheck = millis();
            if (batteryMonitor.readPercent() <= 15) {
                blinkStatusLed(5, 80, 80);
            }
        }
    }

    delay(50);
}

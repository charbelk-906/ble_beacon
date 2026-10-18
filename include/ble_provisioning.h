#pragma once
#include <Arduino.h>
#include "config_manager.h"
#include "battery_monitor.h"

// PROVISIONING mode: a connectable BLE GATT server the technician app talks
// to when first setting up a node (or re-configuring one on-site).
//
// Flow the technician app is expected to follow:
//   1. Scan for devices advertising PROVISIONING_SERVICE_UUID.
//   2. Connect, optionally read CHAR_SERIAL to confirm which physical unit
//      this is (matches a label/QR code on the hardware).
//   3. Write CHAR_MALL_UUID, CHAR_FLOOR_MAJOR, CHAR_NODE_MINOR,
//      CHAR_NODE_LABEL, and CHAR_TX_POWER (tx power ideally measured on-site
//      by holding a reference phone at 1m and reading the RSSI it sees).
//   4. Write CHAR_COMMAND = "SAVE_REBOOT" to persist + restart into
//      NAVIGATION mode.
//
// Returns true from loop-driving code once a save+reboot command has been
// received, so main.cpp knows to actually restart the device.
class BleProvisioning {
public:
    void begin(ConfigManager& configMgr, BatteryMonitor& battery);

    // True once the technician has sent SAVE_REBOOT and the config was
    // saved successfully — main.cpp should call ESP.restart() after seeing
    // this go true (a short delay first lets the "success" notify go out).
    bool shouldReboot() const { return _rebootRequested; }

    // True if a factory reset was requested via CHAR_COMMAND.
    bool shouldFactoryReset() const { return _factoryResetRequested; }
    bool isClientConnected() const { return _clientConnected; }
    uint32_t lastActivityMs() const { return _lastActivityMs; }

private:

    static ConfigManager* _configMgr;
    static BatteryMonitor* _battery;
    static volatile bool _rebootRequested;
    static volatile bool _factoryResetRequested;
    static volatile bool _clientConnected;
    static volatile uint32_t _lastActivityMs;

    friend class ProvisioningCallbacks;
    friend class ProvisioningServerCallbacks;
};

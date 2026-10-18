#pragma once
#include <Arduino.h>
#include "config_manager.h"
#include "battery_monitor.h"
#include "fan_controller.h"

// Broadcasts this node as a standard iBeacon:
//   UUID  = mall identifier   ("global id" — same for every node in one mall)
//   Major = floor identifier  ("local id")
//   Minor = this specific node's id within the floor
//
// This is a non-connectable, one-way broadcast — nodes never talk to each
// other and never need to be discovered/connected to during normal
// operation, matching the "no mesh, beacon-only" design.
//
// Battery info can't fit in the 31-byte iBeacon advertising packet itself
// (it's already using 30 of the 31 bytes), so it rides in the separate
// 31-byte BLE scan response packet instead, as simple manufacturer data
// the app can read alongside the iBeacon fields.
class BleBeacon {
public:
        void begin(const NodeConfig& config, const BatteryMonitor& battery, const FanController& fan);
    void refreshStatus(const BatteryMonitor& battery, const FanController& fan);

private:
    static const uint16_t MFG_COMPANY_ID_APPLE = 0x004C; // required for iBeacon format recognition
    static const uint16_t MFG_COMPANY_ID_CUSTOM = 0xFFFF; // reserved/testing ID, used for our battery payload

    void buildAndStartAdvertising(const NodeConfig& config);
    static bool parseUuid(const char* uuidStr, uint8_t out[16]);
};

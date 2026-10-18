#pragma once
#include <Arduino.h>

// Holds everything that makes this generic hardware into a specific
// "shop entrance on floor 1 of Qatar Mall" node. Everything here is
// runtime-configurable and persisted to flash (NVS) — nothing about a
// specific mall is ever compiled into the firmware.
struct NodeConfig {
    bool     configured   = false;   // false until a technician provisions it
    char     mallUuid[37] = {0};     // 128-bit UUID as string, e.g. "e2c56db5-dffb-48d2-b060-d0f5a71096e0" — "global id"
    uint16_t floorMajor   = 0;       // "local id", e.g. floor number
    uint16_t nodeMinor    = 0;       // unique id of this node within the floor
    char     nodeLabel[33]= {0};     // human-readable, e.g. "Zara Store Entrance" — technician convenience only
    int8_t   txPowerCal   = -59;     // measured RSSI at 1m, used by phone-side trilateration
};

class ConfigManager {
public:
    // Loads config from NVS into memory. Call once in setup().
    void begin();

    // Returns the currently loaded config (in-memory, not re-read from flash).
    const NodeConfig& get() const { return _config; }

    // Applies field-level updates (used by the GATT write callbacks) — does
    // NOT persist to flash by itself, call save() when the technician sends
    // the "SAVE_REBOOT" command so a half-written config never survives a
    // crash mid-provisioning.
    void setMallUuid(const char* uuid);
    void setFloorMajor(uint16_t major);
    void setNodeMinor(uint16_t minor);
    void setNodeLabel(const char* label);
    void setTxPowerCal(int8_t txPower);

    // Persists the in-memory config to NVS and marks it "configured".
    bool save();

    // Wipes NVS config back to factory/unconfigured state.
    void factoryReset();

    // A stable per-device identifier that exists even before provisioning,
    // derived from the ESP32's burned-in MAC address. Lets the mall's backend
    // map "this specific piece of hardware" to a DB row before or after the
    // technician assigns it a mall/floor/node id.
    String getDeviceSerial() const;
        // True if s is a 36-char hyphenated UUID (hex digits).
    static bool isValidUuid(const char* s);
private:
    NodeConfig _config;
};

#include "config_manager.h"
#include <Preferences.h>
#include <ctype.h>

static const char* NVS_NAMESPACE = "beacon_cfg";

void ConfigManager::begin() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);

    _config.configured = prefs.getBool("configured", false);

    String uuid = prefs.getString("mallUuid", "");
    strncpy(_config.mallUuid, uuid.c_str(), sizeof(_config.mallUuid) - 1);

    _config.floorMajor = prefs.getUShort("floorMajor", 0);
    _config.nodeMinor  = prefs.getUShort("nodeMinor", 0);

    String label = prefs.getString("nodeLabel", "");
    strncpy(_config.nodeLabel, label.c_str(), sizeof(_config.nodeLabel) - 1);

    _config.txPowerCal = (int8_t) prefs.getChar("txPowerCal", -59);

    prefs.end();

    Serial.println("[Config] Loaded from NVS:");
    Serial.printf("  configured  = %s\n", _config.configured ? "true" : "false");
    Serial.printf("  mallUuid    = %s\n", _config.mallUuid);
    Serial.printf("  floorMajor  = %u\n", _config.floorMajor);
    Serial.printf("  nodeMinor   = %u\n", _config.nodeMinor);
    Serial.printf("  nodeLabel   = %s\n", _config.nodeLabel);
    Serial.printf("  txPowerCal  = %d\n", _config.txPowerCal);
}
bool ConfigManager::isValidUuid(const char* s) {
    if (s == nullptr || strlen(s) != 36) return false;
    for (int i = 0; i < 36; i++) {
        bool hyphenPos = (i == 8 || i == 13 || i == 18 || i == 23);
        if (hyphenPos) {
            if (s[i] != '-') return false;
        } else if (!isxdigit((unsigned char) s[i])) {
            return false;
        }
    }
    return true;
}

void ConfigManager::setMallUuid(const char* uuid) {
    strncpy(_config.mallUuid, uuid, sizeof(_config.mallUuid) - 1);
    _config.mallUuid[sizeof(_config.mallUuid) - 1] = '\0';
}

void ConfigManager::setFloorMajor(uint16_t major) {
    _config.floorMajor = major;
}

void ConfigManager::setNodeMinor(uint16_t minor) {
    _config.nodeMinor = minor;
}

void ConfigManager::setNodeLabel(const char* label) {
    strncpy(_config.nodeLabel, label, sizeof(_config.nodeLabel) - 1);
    _config.nodeLabel[sizeof(_config.nodeLabel) - 1] = '\0';
}

void ConfigManager::setTxPowerCal(int8_t txPower) {
    _config.txPowerCal = txPower;
}

bool ConfigManager::save() {
    // Basic sanity check before committing — an empty mall UUID means the
    // technician hasn't actually finished provisioning yet.
    if (!isValidUuid(_config.mallUuid)) {
        Serial.printf("[Config] Refusing to save: mallUuid '%s' is not a valid UUID\n", _config.mallUuid);
        return false;
    }

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);

    _config.configured = true;
    prefs.putBool("configured", true);
    prefs.putString("mallUuid", _config.mallUuid);
    prefs.putUShort("floorMajor", _config.floorMajor);
    prefs.putUShort("nodeMinor", _config.nodeMinor);
    prefs.putString("nodeLabel", _config.nodeLabel);
    prefs.putChar("txPowerCal", _config.txPowerCal);

    prefs.end();

    Serial.println("[Config] Saved to NVS.");
    return true;
}

void ConfigManager::factoryReset() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
    prefs.clear();
    prefs.end();

    _config = NodeConfig(); // reset in-memory struct to defaults
    Serial.println("[Config] Factory reset complete.");
}

String ConfigManager::getDeviceSerial() const {
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    snprintf(buf, sizeof(buf), "%012llX", mac);
    return String(buf);
}

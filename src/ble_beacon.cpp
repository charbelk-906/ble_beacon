#include "ble_beacon.h"
#include "board_config.h"
#include <NimBLEDevice.h>
#include <ctype.h>
#include <math.h>

bool BleBeacon::parseUuid(const char* uuidStr, uint8_t out[16]) {
    // Accepts standard hyphenated UUID string form:
    // "e2c56db5-dffb-48d2-b060-d0f5a71096e0"
    int bytePos = 0;
    int i = 0;
    int len = strlen(uuidStr);

    while (i < len && bytePos < 16) {
        if (uuidStr[i] == '-') { i++; continue; }
        if (i + 1 >= len) return false;
        if (!isxdigit((unsigned char) uuidStr[i]) || !isxdigit((unsigned char) uuidStr[i + 1])) {
            return false;
        }

        char hex[3] = { uuidStr[i], uuidStr[i + 1], '\0' };
        out[bytePos] = (uint8_t) strtol(hex, nullptr, 16);
        bytePos++;
        i += 2;
    }
    return bytePos == 16;
}

void BleBeacon::begin(const NodeConfig& config, const BatteryMonitor& battery, const FanController& fan) {
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    refreshStatus(battery, fan);       // set scan response BEFORE advertising starts
    buildAndStartAdvertising(config);
}

void BleBeacon::buildAndStartAdvertising(const NodeConfig& config) {
    uint8_t uuidBytes[16];
    if (!parseUuid(config.mallUuid, uuidBytes)) {
        Serial.println("[Beacon] ERROR: mallUuid failed to parse, node is not properly provisioned!");
        memset(uuidBytes, 0, sizeof(uuidBytes)); // fail safe: broadcast all-zero UUID, clearly invalid
    }

    // --- Primary advertising packet: standard iBeacon payload ---
    std::string mfgData;
    mfgData += (char) 0x02; // beacon sub-type: proximity
    mfgData += (char) 0x15; // payload length that follows (21 bytes: 16 uuid + 2 major + 2 minor + 1 txpower)
    for (int i = 0; i < 16; i++) mfgData += (char) uuidBytes[i];
    mfgData += (char) (config.floorMajor >> 8);
    mfgData += (char) (config.floorMajor & 0xFF);
    mfgData += (char) (config.nodeMinor >> 8);
    mfgData += (char) (config.nodeMinor & 0xFF);
    mfgData += (char) config.txPowerCal;

    NimBLEAdvertisementData advData;
    advData.setFlags(0x06); // LE General Discoverable + BR/EDR not supported // BR/EDR not supported
    advData.setManufacturerData(std::string(1, (char)(MFG_COMPANY_ID_APPLE & 0xFF)) +
                                 std::string(1, (char)(MFG_COMPANY_ID_APPLE >> 8)) +
                                 mfgData);

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAdvertisementType(BLE_GAP_CONN_MODE_NON); // non-connectable
    adv->setAdvertisementType(BLE_GAP_CONN_MODE_NON); // non-connectable
    adv->setAdvertisementData(advData);
    adv->setMinInterval(ADV_INTERVAL_MIN_MS / 0.625); // NimBLE takes units of 0.625ms
    adv->setMaxInterval(ADV_INTERVAL_MAX_MS / 0.625);
    adv->setScanResponse(true);

    Serial.printf("[Beacon] Advertising iBeacon: UUID=%s Major=%u Minor=%u TxPower=%d\n",
                  config.mallUuid, config.floorMajor, config.nodeMinor, config.txPowerCal);

    adv->start();
}

void BleBeacon::refreshStatus(const BatteryMonitor& battery, const FanController& fan) {
    uint8_t pct = battery.readPercent();
    uint8_t charging = battery.isCharging() ? 1 : 0;
    bool tempOk = fan.sensorOk();
    int16_t temp10 = tempOk ? (int16_t) lroundf(fan.temperatureC() * 10.0f) : (int16_t) 0x7FFF;
    uint8_t fanOn = fan.isFanOn() ? 1 : 0;

    // [FF FF][battery %][charging][temp x10 hi][temp x10 lo][fan]
    std::string data;
    data += (char)(MFG_COMPANY_ID_CUSTOM & 0xFF);
    data += (char)(MFG_COMPANY_ID_CUSTOM >> 8);
    data += (char) pct;
    data += (char) charging;
    data += (char)(((uint16_t) temp10) >> 8);
    data += (char)(((uint16_t) temp10) & 0xFF);
    data += (char) fanOn;

    NimBLEAdvertisementData scanResp;
    scanResp.setManufacturerData(data);
    NimBLEDevice::getAdvertising()->setScanResponseData(scanResp);

    if (tempOk) {
        Serial.printf("[Beacon] Status: battery=%u%% charging=%s temp=%.1fC fan=%s\n",
                      pct, charging ? "yes" : "no", fan.temperatureC(), fanOn ? "ON" : "OFF");
    } else {
        Serial.printf("[Beacon] Status: battery=%u%% charging=%s temp=n/a fan=%s\n",
                      pct, charging ? "yes" : "no", fanOn ? "ON" : "OFF");
    }
}
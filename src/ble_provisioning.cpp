#include "ble_provisioning.h"
#include "board_config.h"
#include <NimBLEDevice.h>

ConfigManager* BleProvisioning::_configMgr = nullptr;
BatteryMonitor* BleProvisioning::_battery = nullptr;
volatile bool BleProvisioning::_rebootRequested = false;
volatile bool BleProvisioning::_factoryResetRequested = false;
volatile bool BleProvisioning::_clientConnected = false;
volatile uint32_t BleProvisioning::_lastActivityMs = 0;

static NimBLECharacteristic* pStatusChar = nullptr;

static void updateStatusChar(BatteryMonitor* battery) {
    if (!pStatusChar) return;
    char statusBuf[48];
    snprintf(statusBuf, sizeof(statusBuf), "FW:%s BATT:%u%%",
             FIRMWARE_VERSION, battery ? battery->readPercent() : 0);
    pStatusChar->setValue(std::string(statusBuf));
}

class ProvisioningServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        BleProvisioning::_clientConnected = true;
        BleProvisioning::_lastActivityMs = millis();
        updateStatusChar(BleProvisioning::_battery);
        Serial.println("[Provisioning] Technician app connected.");
    }
    void onDisconnect(NimBLEServer* pServer) override {
        BleProvisioning::_clientConnected = false;
        BleProvisioning::_lastActivityMs = millis();
        Serial.println("[Provisioning] Technician app disconnected.");
    }
};

// One callback class handles every writable characteristic — dispatches on
// the characteristic's UUID so we don't need a separate class per field.
class ProvisioningCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        std::string uuid = pChar->getUUID().toString();
        std::string value = pChar->getValue();

        ConfigManager* cfg = BleProvisioning::_configMgr;
        if (!cfg) return;

        if (uuid == CHAR_MALL_UUID_UUID) {
            if (ConfigManager::isValidUuid(value.c_str())) {
                cfg->setMallUuid(value.c_str());
                Serial.printf("[Provisioning] mallUuid set to: %s\n", value.c_str());
            } else {
                Serial.printf("[Provisioning] Rejected invalid mallUuid: '%s'\n", value.c_str());
                if (pStatusChar) {
                    pStatusChar->setValue(std::string("ERR:BAD_UUID"));
                    pStatusChar->notify();
                }
            }

        } else if (uuid == CHAR_FLOOR_MAJOR_UUID) {
            if (value.size() >= 2) {
                uint16_t major = ((uint8_t)value[0] << 8) | (uint8_t)value[1];
                cfg->setFloorMajor(major);
                Serial.printf("[Provisioning] floorMajor set to: %u\n", major);
            }

        } else if (uuid == CHAR_NODE_MINOR_UUID) {
            if (value.size() >= 2) {
                uint16_t minor = ((uint8_t)value[0] << 8) | (uint8_t)value[1];
                cfg->setNodeMinor(minor);
                Serial.printf("[Provisioning] nodeMinor set to: %u\n", minor);
            }

        } else if (uuid == CHAR_NODE_LABEL_UUID) {
            cfg->setNodeLabel(value.c_str());
            Serial.printf("[Provisioning] nodeLabel set to: %s\n", value.c_str());

        } else if (uuid == CHAR_TX_POWER_UUID) {
            if (value.size() >= 1) {
                int8_t txPower = (int8_t) value[0];
                cfg->setTxPowerCal(txPower);
                Serial.printf("[Provisioning] txPowerCal set to: %d\n", txPower);
            }

        } else if (uuid == CHAR_COMMAND_UUID) {
            if (value == "SAVE_REBOOT") {
                bool ok = cfg->save();
                std::string status = ok ? "OK:SAVED" : "ERR:INVALID_CONFIG";
                if (pStatusChar) {
                    pStatusChar->setValue(status);
                    pStatusChar->notify();
                }
                if (ok) {
                    BleProvisioning::_rebootRequested = true;
                    Serial.println("[Provisioning] SAVE_REBOOT received and config saved. Rebooting shortly...");
                } else {
                    Serial.println("[Provisioning] SAVE_REBOOT received but config was invalid, NOT rebooting.");
                }
            } else if (value == "FACTORY_RESET") {
                BleProvisioning::_factoryResetRequested = true;
                Serial.println("[Provisioning] FACTORY_RESET received.");
            }
        }
    }
};

static ProvisioningCallbacks provCallbacks;
static ProvisioningServerCallbacks serverCallbacks;

void BleProvisioning::begin(ConfigManager& configMgr, BatteryMonitor& battery) {
    _configMgr = &configMgr;
    _battery = &battery;
    _rebootRequested = false;
    _factoryResetRequested = false;
    _clientConnected = false;
    _lastActivityMs = millis();

    NimBLEDevice::init("NavBeacon-Setup");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(&serverCallbacks, false);
    NimBLEService* service = server->createService(PROVISIONING_SERVICE_UUID);

    auto addWritableChar = [&](const char* uuid) {
        NimBLECharacteristic* c = service->createCharacteristic(
            uuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
        c->setCallbacks(&provCallbacks);
        return c;
    };

    NimBLECharacteristic* cMallUuid = addWritableChar(CHAR_MALL_UUID_UUID);
    NimBLECharacteristic* cFloor    = addWritableChar(CHAR_FLOOR_MAJOR_UUID);
    NimBLECharacteristic* cMinor    = addWritableChar(CHAR_NODE_MINOR_UUID);
    NimBLECharacteristic* cLabel    = addWritableChar(CHAR_NODE_LABEL_UUID);
    NimBLECharacteristic* cTxPower  = addWritableChar(CHAR_TX_POWER_UUID);
    service->createCharacteristic(CHAR_COMMAND_UUID, NIMBLE_PROPERTY::WRITE)
        ->setCallbacks(&provCallbacks);

    // Pre-fill read characteristics with whatever config already exists, so
    // a technician re-opening an already-configured node sees its current
    // values instead of blanks.
    const NodeConfig& existing = configMgr.get();
    cMallUuid->setValue(std::string(existing.mallUuid));
    cFloor->setValue(std::string(1, (char)(existing.floorMajor >> 8)) +
                      std::string(1, (char)(existing.floorMajor & 0xFF)));
    cMinor->setValue(std::string(1, (char)(existing.nodeMinor >> 8)) +
                      std::string(1, (char)(existing.nodeMinor & 0xFF)));
    cLabel->setValue(std::string(existing.nodeLabel));
    cTxPower->setValue(std::string(1, (char) existing.txPowerCal));

    // Read-only: firmware version + battery status, useful for the
    // technician app to show "this unit is healthy" during setup.
    pStatusChar = service->createCharacteristic(
        CHAR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    updateStatusChar(&battery);

    // Read-only: stable hardware serial, independent of provisioning state.
    NimBLECharacteristic* cSerial = service->createCharacteristic(
        CHAR_SERIAL_UUID, NIMBLE_PROPERTY::READ);
    cSerial->setValue(configMgr.getDeviceSerial().c_str());

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(PROVISIONING_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->start();

    Serial.println("[Provisioning] GATT server started, waiting for technician app...");
    Serial.printf("[Provisioning] Device serial: %s\n", configMgr.getDeviceSerial().c_str());
}

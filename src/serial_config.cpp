#include "serial_config.h"
#include "board_config.h"

void SerialConfiguration::begin(ConfigManager& configMgr, BatteryMonitor& battery, FanController& fan) {
    _configMgr = &configMgr;
    _battery = &battery;
    _fan = &fan;
    _lineBuffer = "";

    Serial.println();
    Serial.println("=================================================");
    Serial.println(" Serial configuration CLI ready. Type HELP + Enter.");
    Serial.println("=================================================");
    printStatus();
}

void SerialConfiguration::poll() {
    while (Serial.available() > 0) {
        char c = (char) Serial.read();

        if (c == '\n') {
            _lineBuffer.trim();
            if (_lineBuffer.length() > 0) {
                handleLine(_lineBuffer);
            }
            _lineBuffer = "";
        } else if (c != '\r') {
            _lineBuffer += c;
        }
    }
}

void SerialConfiguration::printHelp() const {
    Serial.println();
    Serial.println("Available commands:");
    Serial.println("  SET_UUID <uuid>     e.g. SET_UUID e2c56db5-dffb-48d2-b060-d0f5a71096e0");
    Serial.println("  SET_FLOOR <n>       e.g. SET_FLOOR 1          (0-65535, this is the 'local id')");
    Serial.println("  SET_MINOR <n>       e.g. SET_MINOR 12         (0-65535, unique id on this floor)");
    Serial.println("  SET_LABEL <text>    e.g. SET_LABEL Zara Store Entrance");
    Serial.println("  SET_TXPOWER <n>     e.g. SET_TXPOWER -59      (signed dBm measured at 1m)");
    Serial.println("  SAVE                persist config to flash and reboot into NAVIGATION mode");
    Serial.println("  RESET               factory reset and reboot into PROVISIONING mode");
    Serial.println("  STATUS              show current in-memory config and battery info");
    Serial.println("  HELP                show this list");
    Serial.println();
}

void SerialConfiguration::printStatus() const {
    const NodeConfig& cfg = _configMgr->get();
    Serial.println();
    Serial.println("--- Current config (in memory, not yet saved unless you ran SAVE) ---");
    Serial.printf("  Device serial : %s\n", _configMgr->getDeviceSerial().c_str());
    Serial.printf("  Configured    : %s\n", cfg.configured ? "yes" : "no");
    Serial.printf("  Mall UUID     : %s\n", strlen(cfg.mallUuid) ? cfg.mallUuid : "(not set)");
    Serial.printf("  Floor (Major) : %u\n", cfg.floorMajor);
    Serial.printf("  Node  (Minor) : %u\n", cfg.nodeMinor);
    Serial.printf("  Label         : %s\n", strlen(cfg.nodeLabel) ? cfg.nodeLabel : "(not set)");
    Serial.printf("  TX power cal  : %d dBm\n", cfg.txPowerCal);
    if (_battery) {
        Serial.printf("  Battery       : %u%% (%.2fV) %s\n",
                      _battery->readPercent(), _battery->readVoltage(),
                      _battery->isCharging() ? "[charging]" : "");
    }
    if (_fan) {
        if (_fan->sensorOk()) Serial.printf("  Temperature   : %.2f C\n", _fan->temperatureC());
     else                  Serial.println("  Temperature   : n/a (no reading yet or sensor error)");
    Serial.printf("  Fan           : %s (on >= %.1f C, off <= %.1f C)\n",
                    _fan->isFanOn() ? "ON" : "OFF", FAN_ON_TEMP_C, FAN_OFF_TEMP_C);
    }
    Serial.println("-----------------------------------------------------------------------");
    Serial.println();
}

void SerialConfiguration::handleLine(const String& line) {
    // Split into command + rest-of-line argument (argument may itself
    // contain spaces, e.g. a label like "Zara Store Entrance").
    int spaceIdx = line.indexOf(' ');
    String cmd = (spaceIdx == -1) ? line : line.substring(0, spaceIdx);
    String arg = (spaceIdx == -1) ? "" : line.substring(spaceIdx + 1);
    cmd.toUpperCase();
    arg.trim();

    if (cmd == "HELP") {
        printHelp();

    } else if (cmd == "STATUS") {
        printStatus();

    } else if (cmd == "SET_UUID") {
        if (!ConfigManager::isValidUuid(arg.c_str())) {
            Serial.println("[ERROR] UUID must be 36 chars, e.g. e2c56db5-dffb-48d2-b060-d0f5a71096e0");
        } else {
            _configMgr->setMallUuid(arg.c_str());
            Serial.printf("[OK] Mall UUID set to: %s\n", arg.c_str());
        }

    } else if (cmd == "SET_FLOOR") {
        long val = arg.toInt();
        if (arg.length() == 0 || val < 0 || val > 65535) {
            Serial.println("[ERROR] Floor must be a number 0-65535");
        } else {
            _configMgr->setFloorMajor((uint16_t) val);
            Serial.printf("[OK] Floor (Major) set to: %ld\n", val);
        }

    } else if (cmd == "SET_MINOR") {
        long val = arg.toInt();
        if (arg.length() == 0 || val < 0 || val > 65535) {
            Serial.println("[ERROR] Node id must be a number 0-65535");
        } else {
            _configMgr->setNodeMinor((uint16_t) val);
            Serial.printf("[OK] Node (Minor) set to: %ld\n", val);
        }

    } else if (cmd == "SET_LABEL") {
        if (arg.length() == 0) {
            Serial.println("[ERROR] Label cannot be empty");
        } else if (arg.length() > 32) {
            Serial.println("[ERROR] Label must be 32 characters or fewer");
        } else {
            _configMgr->setNodeLabel(arg.c_str());
            Serial.printf("[OK] Label set to: %s\n", arg.c_str());
        }

    } else if (cmd == "SET_TXPOWER") {
        long val = arg.toInt();
        if (arg.length() == 0 || val < -127 || val > 127) {
            Serial.println("[ERROR] TX power must be a signed number, e.g. -59");
        } else {
            _configMgr->setTxPowerCal((int8_t) val);
            Serial.printf("[OK] TX power calibration set to: %ld dBm\n", val);
        }

    } else if (cmd == "SAVE") {
        if (_configMgr->save()) {
            Serial.println("[OK] Config saved to flash. Rebooting into NAVIGATION mode...");
            delay(300);
            ESP.restart();
        } else {
            Serial.println("[ERROR] Save failed — did you set a Mall UUID (SET_UUID)?");
        }

    } else if (cmd == "RESET") {
        Serial.println("[OK] Factory reset. Rebooting into PROVISIONING mode...");
        _configMgr->factoryReset();
        delay(300);
        ESP.restart();

    } else {
        Serial.printf("[ERROR] Unknown command: '%s'. Type HELP for the command list.\n", cmd.c_str());
    }
}

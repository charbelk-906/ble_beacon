#pragma once 
#include <Arduino.h> 
#include "config_manager.h" 
#include "battery_monitor.h" 
#include "fan_controller.h"
// Lets you configure a node entirely over USB serial, with no phone/app 
// needed — useful for bring-up and testing before the technician app 
// exists. Open the PlatformIO serial monitor (115200 baud), type HELP. // 
// Commands (case-insensitive, one per line, press Enter): 
// SET_UUID <uuid> e.g. SET_UUID e2c56db5-dffb-48d2-b060-d0f5a71096e0 
// SET_FLOOR <n> e.g. SET_FLOOR 1 (0-65535) 
// SET_MINOR <n> e.g. SET_MINOR 12 (0-65535) 
// SET_LABEL <text> e.g. SET_LABEL Zara Store Entrance 
// SET_TXPOWER <n> e.g. SET_TXPOWER -59 (signed, dBm at 1m) 
// SAVE persist config to flash + reboot into NAVIGATION mode 
// RESET factory reset + reboot into PROVISIONING mode 
// STATUS print current in-memory config + battery info 
// HELP print this command list 
class SerialConfiguration { 
public:
    void begin(ConfigManager& configMgr, BatteryMonitor& battery, FanController& fan);
    // Call every loop() iteration — non-blocking, only acts when a full 
    // line has arrived on Serial. 
    void poll(); 

private:
    ConfigManager* _configMgr = nullptr;
    BatteryMonitor* _battery = nullptr;
    FanController* _fan = nullptr;
    String _lineBuffer;

    void handleLine(const String& line);
    void printHelp() const;
    void printStatus() const;
};
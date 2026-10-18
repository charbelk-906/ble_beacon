#pragma once
#include <Arduino.h>

class OneWire;
class DallasTemperature;

// Reads a DS18B20 and switches the fan with hysteresis. update() is
// non-blocking, so BLE and the serial CLI keep running smoothly.
class FanController {
public:
    void begin(int tempSensorPin, int fanPin);
    void update();   // call every loop(), in both modes

    float temperatureC() const { return _tempC; }
    bool  sensorOk() const     { return _sensorOk; }
    bool  isFanOn() const      { return _fanOn; }

private:
    void setFan(bool on);
    void applyHysteresis();

    int _fanPin = -1;
    OneWire* _oneWire = nullptr;
    DallasTemperature* _sensors = nullptr;

    bool     _conversionPending = false;
    uint32_t _lastRequestMs = 0;

    float   _tempC = NAN;
    bool    _sensorOk = false;
    bool    _fanOn = false;
    uint8_t _failCount = 0;
    bool    _failReported = false;
};
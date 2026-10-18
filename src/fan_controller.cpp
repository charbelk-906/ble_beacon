#include "fan_controller.h"
#include "board_config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

static const uint8_t  TEMP_RESOLUTION_BITS = 11;   // 0.125 C steps, ~375 ms
static const uint32_t TEMP_CONVERSION_MS   = 400;
static const uint8_t  MAX_CONSECUTIVE_FAILS = 3;

void FanController::begin(int tempSensorPin, int fanPin) {
    _fanPin = fanPin;
    pinMode(_fanPin, OUTPUT);
    digitalWrite(_fanPin, LOW);   // fan off before anything else
    _fanOn = false;

    _oneWire = new OneWire(tempSensorPin);
    _sensors = new DallasTemperature(_oneWire);
    _sensors->begin();
    _sensors->setResolution(TEMP_RESOLUTION_BITS);
    _sensors->setWaitForConversion(false);

    Serial.printf("[Fan] DS18B20 sensors found: %u (data GPIO%d, fan GPIO%d)\n",
                  (unsigned) _sensors->getDeviceCount(), tempSensorPin, fanPin);
    Serial.printf("[Fan] Fan ON at >= %.1f C, OFF at <= %.1f C\n", FAN_ON_TEMP_C, FAN_OFF_TEMP_C);

    _sensors->requestTemperatures();
    _conversionPending = true;
    _lastRequestMs = millis();
}

void FanController::update() {
    if (!_sensors) return;
    uint32_t now = millis();

    if (_conversionPending) {
        if (now - _lastRequestMs < TEMP_CONVERSION_MS) return;
        _conversionPending = false;

        float t = _sensors->getTempCByIndex(0);
        // -127 = disconnected; exactly 85.0 = power-on default (bad read)
        bool valid = (t != DEVICE_DISCONNECTED_C) && (t != 85.0f) &&
                     (t > -55.0f) && (t < 125.0f);

        if (valid) {
            if (_failReported) Serial.println("[Fan] Temperature sensor recovered.");
            _tempC = t;
            _sensorOk = true;
            _failCount = 0;
            _failReported = false;
            applyHysteresis();
        } else if (_failCount < MAX_CONSECUTIVE_FAILS && ++_failCount >= MAX_CONSECUTIVE_FAILS) {
            _sensorOk = false;
            if (!_failReported) {
                Serial.printf("[Fan] WARNING: temperature sensor not responding -> fan %s\n",
                              FAN_ON_IF_SENSOR_FAILS ? "ON (fail-safe)" : "OFF");
                _failReported = true;
            }
            setFan(FAN_ON_IF_SENSOR_FAILS);
        }
        return;
    }

    if (now - _lastRequestMs >= TEMP_READ_INTERVAL_MS) {
        _sensors->requestTemperatures();
        _conversionPending = true;
        _lastRequestMs = now;
    }
}

void FanController::applyHysteresis() {
    if (!_fanOn && _tempC >= FAN_ON_TEMP_C)      setFan(true);
    else if (_fanOn && _tempC <= FAN_OFF_TEMP_C) setFan(false);
}

void FanController::setFan(bool on) {
    digitalWrite(_fanPin, on ? HIGH : LOW);
    if (on != _fanOn) {
        if (_sensorOk) Serial.printf("[Fan] Fan %s (temperature %.2f C)\n", on ? "ON" : "OFF", _tempC);
        else           Serial.printf("[Fan] Fan %s\n", on ? "ON" : "OFF");
    }
    _fanOn = on;
}
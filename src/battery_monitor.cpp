#include "battery_monitor.h"

void BatteryMonitor::begin(int adcPin, int chargeStatusPin) {
    _adcPin = adcPin;
    _chargePin = chargeStatusPin;

    analogReadResolution(12);       // 0-4095
    analogSetAttenuation(ADC_11db); // full ~0-3.3V range

    pinMode(_adcPin, INPUT);
    if (_chargePin >= 0) {
        pinMode(_chargePin, INPUT_PULLUP); // TP4056 CHRG is open-drain, active LOW
    }
}

float BatteryMonitor::readVoltage() const {
    const int numSamples = 16;
    uint32_t totalMv = 0;
    for (int i = 0; i < numSamples; i++) {
        totalMv += analogReadMilliVolts(_adcPin);   // uses factory ADC calibration
        delayMicroseconds(200);
    }
    float pinVoltage = ((float) totalMv / numSamples) / 1000.0f;
    return pinVoltage * DIVIDER_RATIO;
}

uint8_t BatteryMonitor::readPercent() const {
    float v = readVoltage();

    // Rough single-cell LiPo discharge curve (rests, not under load).
    // 4.20V = 100%, 3.00V = 0% (below this the protection circuit on most
    // LiPo/TP4056 setups will have already cut off).
    struct Point { float volts; uint8_t pct; };
    static const Point curve[] = {
        {4.20f, 100}, {4.00f, 85}, {3.85f, 70}, {3.70f, 50},
        {3.55f, 30},  {3.40f, 15}, {3.20f, 5},  {3.00f, 0}
    };
    const int n = sizeof(curve) / sizeof(curve[0]);

    if (v >= curve[0].volts) return 100;
    if (v <= curve[n - 1].volts) return 0;

    for (int i = 0; i < n - 1; i++) {
        if (v <= curve[i].volts && v >= curve[i + 1].volts) {
            float span = curve[i].volts - curve[i + 1].volts;
            float frac = (v - curve[i + 1].volts) / span;
            return (uint8_t) (curve[i + 1].pct + frac * (curve[i].pct - curve[i + 1].pct));
        }
    }
    return 0;
}

bool BatteryMonitor::isCharging() const {
    if (_chargePin < 0) return false;
    return digitalRead(_chargePin) == LOW; // TP4056 CHRG is active LOW
}

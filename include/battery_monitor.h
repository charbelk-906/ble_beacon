#pragma once
#include <Arduino.h>

// Reads LiPo battery voltage through a resistor divider on an ADC pin and
// converts it to an approximate charge percentage.
//
// NOTE: the TP4056 module itself has no fuel gauge / no way to report state
// of charge over a data line — it's just a charge controller. This class
// estimates charge purely from open-circuit voltage against a typical
// single-cell LiPo discharge curve. That's a coarse estimate (+/- ~10%,
// worse under load), which is fine for "is this node dying, go swap it"
// maintenance alerts, but don't treat the percentage as precise. A real
// product would add a fuel-gauge IC (e.g. MAX17048) for accurate %.
class BatteryMonitor {
public:
    void begin(int adcPin, int chargeStatusPin = -1);

    // Raw battery voltage in volts, averaged over a few samples to reduce
    // ADC noise (which gets worse while the BLE radio is transmitting).
    float readVoltage() const;

    // Approximate charge percentage, 0-100, clamped.
    uint8_t readPercent() const;

    // True if the TP4056 CHRG pin indicates the battery is actively charging.
    // Returns false if no charge status pin was configured.
    bool isCharging() const;

private:
    int _adcPin = -1;
    int _chargePin = -1;

    // Divider ratio: with two equal resistors (e.g. 100k/100k) the divider
    // halves the voltage, so multiply the ADC reading back up by 2.0.
    // Adjust this if you use different resistor values.
    static constexpr float DIVIDER_RATIO = 2.0f;
};

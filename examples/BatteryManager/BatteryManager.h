/* 
 *  18650 Ion-Li battery
 */

#ifndef BatteryManager_H_
#define BatteryManager_H_

#include "Arduino.h"

#define DEFAULT_PIN 3
#define DEFAULT_CONVERSION_FACTOR 1.455
#define DEFAULT_READS 4095

class BatteryManager {
 public:
  BatteryManager();
  ~BatteryManager();
  BatteryManager(int adcPin);
  BatteryManager(int adcPin, double conversionFactor);
  BatteryManager(int adcPin, double conversionFactor, int reads);

  int getBatteryChargeLevel(bool useConversionTable = false);
  double getBatteryVolts();

 private:
  int _adcPin;
  int _reads;
  double _conversionFactor;
  double *_conversionTable = nullptr;

  void _initConversionTable();
  int _getChargeLevelFromConversionTable(double volts);
  int _calculateChargeLevel(double volts);
  int _avgAnalogRead(int pinNumber, int reads);
  double _analogReadToVolts(int readValue);
};

#endif

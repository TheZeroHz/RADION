#include "BatteryManager.h"

// Create battery object using default pin (32) and default conversion factor
BatteryManager battery;

// Alternative constructors you can use:
// BatteryManager battery(A0);                    // Custom pin
// BatteryManager battery(A0, 2.5);               // Custom pin and conversion factor
// BatteryManager battery(A0, 2.5, 10);          // Custom pin, conversion factor, and number of reads

void setup() {
  Serial.begin(15200);
  Serial.println("18650 Battery Monitor Started");
  Serial.println("============================");
}

void loop() {
  // Read battery voltage
  double voltage = battery.getBatteryVolts();
  
  // Read battery charge level (percentage)
  int chargeLevel = battery.getBatteryChargeLevel();
  
  // Print results to serial monitor
  Serial.print("Battery Voltage: ");
  Serial.print(voltage, 2);  // Print with 2 decimal places
  Serial.println(" V");
  
  Serial.print("Charge Level: ");
  Serial.print(chargeLevel);
  Serial.println(" %");
  
  // Optional: Print charge status
  if (chargeLevel > 75) {
    Serial.println("Status: High");
  } else if (chargeLevel > 50) {
    Serial.println("Status: Medium");
  } else if (chargeLevel > 25) {
    Serial.println("Status: Low");
  } else {
    Serial.println("Status: Critical - Recharge needed!");
  }
  
  Serial.println("---");
  
  // Wait 2 seconds before next reading
  delay(2000);
}

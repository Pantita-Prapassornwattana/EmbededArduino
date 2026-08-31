//17.1 clear
#include "my_EEPROM.h"

void setup() {
  Serial.begin(38400);
  delay(1000);
  
  Serial.println("Resetting all EEPROM to 0xFF...");

  // ลบข้อมูลทุกตำแหน่งตั้งแต่ 0 ถึง 1023
  for (int i = 0; i < 1024; i++) {
    EEPROM_Erase_only(i); 
  }

  Serial.println("EEPROM successfully reset!");
  Serial.println("Now you can upload your main code again.");
}

void loop() {
  // ทำงานครั้งเดียวใน setup()
}
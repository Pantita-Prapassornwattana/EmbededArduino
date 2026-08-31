//17.1
#include "my_EEPROM.h"

int first_location = -1;   // ตำแหน่งแรกที่เจอข้อมูล (ไม่ใช่ 0xFF)
int last_location  = -1;   // ตำแหน่งสุดท้ายที่เจอข้อมูล (ไม่ใช่ 0xFF)

uint16_t scan_EEPROM(void)
{
  int i, number_data_found;
  uint8_t d;
  number_data_found = 0;
  first_location = -1;
  last_location  = -1;

  Serial.println("Performing EEPROM scanning...");
  for(i = 0; i < 1024; i++)
  {
    d = EEPROM_read1byte(i);
    if (d != 0xFF)
    {
      if (first_location == -1)
        first_location = i;     // จำตำแหน่งแรกที่เจอ
      last_location = i;        // อัปเดตตำแหน่งล่าสุดที่เจอ

      number_data_found++;
      Serial.print("0x");
      Serial.print(d, HEX);
      Serial.print(" is found in EEPROM location[");
      Serial.print(i);
      Serial.println("]");
    }
  }

  if (number_data_found > 1)
    Serial.println("More than one data are found in EEPROM");
  else if(number_data_found == 0)
    Serial.println("No data found in EEPROM");

  return number_data_found;
}

void setup()
{
  Serial.begin(38400);
  delay(1000); // รอ Serial Port พร้อมใช้งาน
  display_all_data_in_EEPROM();

  uint16_t number_data_found = scan_EEPROM();

  if (number_data_found == 0)
  {
    // ------- กรณีไม่มีข้อมูลเลย : เขียนค่าเริ่มต้นที่ 1021-1023 -------
    Serial.println("No data found. Writing 0xFACE0F to location 1021-1023");

    EEPROM_Erase_and_Write1B(1021, 0xFA);
    EEPROM_Erase_and_Write1B(1022, 0xCE);
    EEPROM_Erase_and_Write1B(1023, 0x0F);
  }
  else if (number_data_found > 1)
  {
    // ------- ตรวจสอบว่าใช่ Pattern 0xFA, 0xCE, 0x0F อยู่ติดกันหรือไม่ -------
    bool is_valid_pattern = false;

    if (number_data_found == 3 && (last_location - first_location == 2))
    {
      if (EEPROM_read1byte(first_location)     == 0xFA &&
          EEPROM_read1byte(first_location + 1) == 0xCE &&
          EEPROM_read1byte(first_location + 2) == 0x0F)
      {
        is_valid_pattern = true;
      }
    }

    if (is_valid_pattern)
    {
      // --- เงื่อนไขเจอ Pattern ติดกัน 3 ตัว : ลบออกแล้วเขียนขยับลงมา 3 ตำแหน่ง ---
      EEPROM_Erase_only(first_location);
      EEPROM_Erase_only(first_location + 1);
      EEPROM_Erase_only(first_location + 2);

      int new_start = first_location - 3;

      // ตรวจสอบไม่ให้ตำแหน่งติดลบ (< 0)
      if (new_start >= 0)
      {
        Serial.print("Moving sequence to location ");
        Serial.print(new_start);
        Serial.print("-");
        Serial.println(new_start + 2);

        EEPROM_Erase_and_Write1B(new_start,     0xFA);
        EEPROM_Erase_and_Write1B(new_start + 1, 0xCE);
        EEPROM_Erase_and_Write1B(new_start + 2, 0x0F);
      }
      else
      {
        Serial.println("Reached lowest EEPROM location (0). Cannot shift lower!");
      }
    }
    else
    {
      // --- เงื่อนไขมีข้อมูลเกิน 1 ตำแหน่ง แต่ไม่ใช่ Pattern 3 ตัวข้างต้น : ลบออกให้หมด ---
      Serial.println("Invalid pattern or general multiple data found. Clearing all locations...");
      for (int i = 0; i < 1024; i++)
      {
        if (EEPROM_read1byte(i) != 0xFF)
        {
          EEPROM_Erase_only(i);
        }
      }
      Serial.println("All non-0xFF data cleared.");
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:
}
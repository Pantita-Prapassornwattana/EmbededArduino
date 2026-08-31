//14.1
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); //Module IIC/I2C Interface 

tmElements_t tm; //ตัวแปรโครงสร้างข้อมูล

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    lcd.print('0');
  }
  lcd.print(number);
}

void setup()
{
  Wire.begin();
  lcd.init();
  lcd.backlight(); // เปิด backlight
}

void loop()
{
  if (RTC.read(tm)) {
    lcd.setCursor(0, 0);
    lcd.print("D/M/Y= ");
    lcd.print(tm.Day);
    lcd.print('/');
    lcd.print(tm.Month);
    lcd.print('/');
    lcd.print(tmYearToCalendar(tm.Year));
    lcd.print("    ");

    lcd.setCursor(0, 1);
    lcd.print("Time = ");
    print2digits(tm.Hour);
    lcd.print(':');
    print2digits(tm.Minute);
    lcd.print("    ");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("RTC read error  ");
    lcd.setCursor(0, 1);
    lcd.print("Check circuit   ");
  }

  delay(1000);
}
//Checkpoint 12.3
#include "DHT.h"
#include <LiquidCrystal.h>

#define DHTPIN 2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal lcd1(8, 9, 10, 11, 12, 13);   // RS, E, D4, D5, D6, D7 (ตามรูปที่ 12.14)

void setup() {
  Serial.begin(9600);
  dht.begin();

  lcd1.begin(16, 2);
  lcd1.clear();
  lcd1.setCursor(0, 0);
  lcd1.print("DHT11 Ready...");
  delay(1000);
}

void loop() {
  delay(2000);   // DHT11 ควรอ่านค่าห่างกันอย่างน้อย ~2 วินาที

  float h = dht.readHumidity();
  float t = dht.readTemperature();       // องศาเซลเซียส

  lcd1.clear();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    lcd1.setCursor(0, 0);
    lcd1.print("Sensor Error!");
    return;
  }

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\tTemperature: ");
  Serial.print(t);
  Serial.println(" *C");

  lcd1.setCursor(0, 0);
  lcd1.print("Humid: ");
  lcd1.print(h);
  lcd1.print(" %");

  lcd1.setCursor(0, 1);
  lcd1.print("Temp : ");
  lcd1.print(t);
  lcd1.print((char)223);   // สัญลักษณ์องศา (°) บน LCD
  lcd1.print("C");
}
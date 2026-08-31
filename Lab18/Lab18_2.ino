//18.2
#define Y_LED 0
#define R_LED 15
#define G_LED 2
#define SW1 4

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID   "TMPL6xLfy_DY7"
#define BLYNK_TEMPLATE_NAME "Checkpoint2"
#define BLYNK_AUTH_TOKEN    "K-bHUEUuXvdHM0ZLvW8udJgMgX0dBKFy"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "Pizza";
char pass[] = "12345676";

BlynkTimer timer;

// เมื่อกด Button บนแอพ (Virtual Pin V0) -> ควบคุมไฟแดง
BLYNK_WRITE(V0)
{
  int value = param.asInt();
  if (value == 1)
    digitalWrite(R_LED, HIGH);
  else
    digitalWrite(R_LED, LOW);
}

// เมื่อเลื่อน Slider บนแอพ (Virtual Pin V1) -> ปรับความสว่างไฟเขียว
BLYNK_WRITE(V1)
{
  int value = param.asInt();
  analogWrite(G_LED, value);
}

// อ่านค่าดิปสวิตช์บิตที่ 1 แล้วส่งไปแสดงผลที่ V3 บนแอพทุก 100 ms
void ReadSW()
{
  uint8_t d = digitalRead(SW1);
  Blynk.virtualWrite(V3, d);
}

void setup()
{
  pinMode(Y_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);
  pinMode(R_LED, OUTPUT);
  pinMode(SW1, INPUT);

  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Setup a function to be called every 100 ms
  timer.setInterval(100L, ReadSW);
}

void loop()
{
  Blynk.run();
  timer.run();
}
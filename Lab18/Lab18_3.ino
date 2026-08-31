//3
#define R_LED 15   // LED สีแดง
#define Y_LED 0    // LED สีเหลือง
#define G_LED 2    // LED สีเขียว

#define SW_FRONT 4   // SW บิต 1 (GPIO4) -> ประตูหน้าบ้าน
#define SW_BACK  14  // SW บิต 2 (GPIO14) -> ประตูหลังบ้าน

#define SW_BIT8 16   // GPIO16
#define SW_BIT7 5    // GPIO5
#define SW_BIT6 13   // GPIO13
#define SW_BIT5 12   // GPIO12

// ----------------- Blynk Configuration -----------------
#define BLYNK_TEMPLATE_ID "TMPL6_Zi8RsGy"
#define BLYNK_TEMPLATE_NAME "Lab18Check3"
#define BLYNK_AUTH_TOKEN    "muMN0lDqqZKukIWejf6yRmv43SkqofbA"

#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char ssid[] = "Pizza";
char pass[] = "12345676";

BlynkTimer timer;

// ตัวแปรเก็บค่าสถานะปุ่มจากหน้าแอป Blynk
int btnRed = 0;
int btnGreen = 0;
int btnYellow = 0;

// รับค่าจากปุ่มบนแอป
BLYNK_WRITE(V10) { btnRed = param.asInt(); }
BLYNK_WRITE(V11) { btnGreen = param.asInt(); }
BLYNK_WRITE(V12) { btnYellow = param.asInt(); }

// ฟังก์ชันอ่านสวิตช์ประตู ส่งค่าขึ้น Widget LED บนแอป (V3 และ V4)
void sendSensorData() {
  uint8_t frontState = digitalRead(SW_FRONT);
  uint8_t backState  = digitalRead(SW_BACK);

  Blynk.virtualWrite(V20, frontState); // ประตูหน้าบ้าน -> V3
  Blynk.virtualWrite(V21, backState);  // ประตูหลังบ้าน -> V4
}

// ฟังก์ชันตรวจสอบสถานะการเชื่อมต่อ Blynk ทุกๆ 2 วินาที
void checkBlynkStatus() {
  if (Blynk.connected()) {
    Serial.println("[Status] Connected to Blynk Cloud (Online)");
  } else {
    Serial.println("[Status] Connecting to Blynk Cloud...");
  }
}

// ฟังก์ชันช่วยดับไฟทุกดวง
void turnOffAll() {
  digitalWrite(R_LED, LOW);
  digitalWrite(G_LED, LOW);
  digitalWrite(Y_LED, LOW);
}

void setup() {
  pinMode(R_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);
  pinMode(Y_LED, OUTPUT);

  pinMode(SW_FRONT, INPUT);
  pinMode(SW_BACK, INPUT);

  pinMode(SW_BIT8, INPUT);
  pinMode(SW_BIT7, INPUT);
  pinMode(SW_BIT6, INPUT);
  pinMode(SW_BIT5, INPUT);

  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n--- Starting Program ---");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Serial.println("\n>>> WiFi & Blynk Connected Successfully! <<<");

  // ส่งค่าสถานะประตูทุกๆ 100 ms
  timer.setInterval(100L, sendSensorData);
  // เช็กสถานะการเชื่อมต่อไปยัง Serial ทุกๆ 2 วินาที
  timer.setInterval(2000L, checkBlynkStatus);
}

void loop() {
  Blynk.run();
  timer.run();

  // อ่านค่าจาก DIP Switch 4 บิต (16, 5, 13, 12)
  int b8 = digitalRead(SW_BIT8);
  int b7 = digitalRead(SW_BIT7);
  int b6 = digitalRead(SW_BIT6);
  int b5 = digitalRead(SW_BIT5);

  // รวมค่า 4 บิตให้อยู่ในรูปตัวเลขฐานสอง/สิบ
  int mode = (b8 << 3) | (b7 << 2) | (b6 << 1) | b5;

  switch (mode) {
    case 0b1000: // 1000: แอลอีดีทุกดวงกระพริบติดดับพร้อมกันทุก 1 วินาที
      digitalWrite(R_LED, HIGH);
      digitalWrite(G_LED, HIGH);
      digitalWrite(Y_LED, HIGH);
      delay(1000);
      turnOffAll();
      delay(1000);
      break;

    case 0b1001: // 1001: คุมเฉพาะไฟแดงผ่านปุ่ม V0 (ดวงอื่นดับ)
      digitalWrite(R_LED, btnRed ? HIGH : LOW);
      digitalWrite(G_LED, LOW);
      digitalWrite(Y_LED, LOW);
      break;

    case 0b1010: // 1010: คุมเฉพาะไฟเขียวผ่านปุ่ม V1 (ดวงอื่นดับ)
      digitalWrite(R_LED, LOW);
      digitalWrite(G_LED, btnGreen ? HIGH : LOW);
      digitalWrite(Y_LED, LOW);
      break;

    case 0b1011: // 1011: คุมเฉพาะไฟเหลืองผ่านปุ่ม V2 (ดวงอื่นดับ)
      digitalWrite(R_LED, LOW);
      digitalWrite(G_LED, LOW);
      digitalWrite(Y_LED, btnYellow ? HIGH : LOW);
      break;

    case 0b0100: // 0100: ทั้งสามดวงถูกควบคุมโดยปุ่มบนมือถือ (V0, V1, V2)
      digitalWrite(R_LED, btnRed ? HIGH : LOW);
      digitalWrite(G_LED, btnGreen ? HIGH : LOW);
      digitalWrite(Y_LED, btnYellow ? HIGH : LOW);
      break;

    case 0b0101: // 0101: ไฟแดงกระพริบสลับดับทุก 1 วินาที (สว่าง 20% -> PWM ~ 205)
      digitalWrite(G_LED, LOW);
      digitalWrite(Y_LED, LOW);
      analogWrite(R_LED, 205);
      delay(1000);
      analogWrite(R_LED, 0);
      delay(1000);
      break;

    case 0b0110: // 0110: ไฟเขียวกระพริบสลับดับทุก 1 วินาที (สว่าง 50% -> PWM ~ 512)
      digitalWrite(R_LED, LOW);
      digitalWrite(Y_LED, LOW);
      analogWrite(G_LED, 512);
      delay(1000);
      analogWrite(G_LED, 0);
      delay(1000);
      break;

    case 0b0111: // 0111: ไฟเหลืองกระพริบสลับดับทุก 1 วินาที (สว่าง 70% -> PWM ~ 716)
      digitalWrite(R_LED, LOW);
      digitalWrite(G_LED, LOW);
      analogWrite(Y_LED, 716);
      delay(1000);
      analogWrite(Y_LED, 0);
      delay(1000);
      break;

    default:
      turnOffAll();
      break;
  }
}
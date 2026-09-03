//15.2.2
#include <avr/sleep.h>
#include <avr/interrupt.h>

const uint8_t PIN_S1 = 2; // Interrupt 0 (S1)
const uint8_t PIN_S2 = 3; // Interrupt 1 (S2)
const uint8_t LED_PINS[8] = {4, 5, 6, 7, 8, 9, 10, 11};

volatile bool flag_S1 = false;
volatile bool flag_S2 = false;

// Active Low: จ่าย HIGH เพื่อดับ LED ทั้งหมด
void clearLEDs() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_PINS[i], HIGH);
  }
}

// ไฟวิ่งจากขอบนอกเข้าสู่ตรงกลาง (3 รอบ)
void runLED_Inward() {
  for (int round = 0; round < 3; round++) {
    for (int i = 0; i < 4; i++) {
      clearLEDs();
      digitalWrite(LED_PINS[i], LOW);       // Active Low: LOW คือติด
      digitalWrite(LED_PINS[7 - i], LOW);   // Active Low: LOW คือติด
      delay(150);
    }
  }
  clearLEDs(); // ดับไฟทั้งหมดเมื่อจบ 3 รอบ
}

// ไฟวิ่งจากตรงกลางออกสู่ขอบนอก (3 รอบ)
void runLED_Outward() {
  for (int round = 0; round < 3; round++) {
    for (int i = 3; i >= 0; i--) {
      clearLEDs();
      digitalWrite(LED_PINS[i], LOW);       // Active Low: LOW คือติด
      digitalWrite(LED_PINS[7 - i], LOW);   // Active Low: LOW คือติด
      delay(150);
    }
  }
  clearLEDs(); // ดับไฟทั้งหมดเมื่อจบ 3 รอบ
}

void isr_S1() { flag_S1 = true; }
void isr_S2() { flag_S2 = true; }

void enterSleepMode() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //[cite: 1]
  sleep_enable();                       //[cite: 1]
  sleep_bod_disable();                  //[cite: 1]
  
  sei();
  sleep_cpu();                          //[cite: 1]
  
  sleep_disable();                      //[cite: 1]
}

void setup() {
  for (int i = 0; i < 8; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }

  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_S1), isr_S1, LOW);
  attachInterrupt(digitalPinToInterrupt(PIN_S2), isr_S2, LOW);

  clearLEDs(); // ปิดไฟตั้งแต่เริ่มต้น
}

void loop() {
  enterSleepMode();

  if (flag_S1) {
    delay(50); // Debounce
    if (digitalRead(PIN_S1) == LOW) {
      runLED_Inward();
      while (digitalRead(PIN_S1) == LOW); // รอกดปล่อย
    }
    flag_S1 = false;
  }

  if (flag_S2) {
    delay(50); // Debounce
    if (digitalRead(PIN_S2) == LOW) {
      runLED_Outward();
      while (digitalRead(PIN_S2) == LOW); // รอกดปล่อย
    }
    flag_S2 = false;
  }
}
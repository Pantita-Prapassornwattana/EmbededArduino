//15.1.2
#include <avr/sleep.h>
#include <avr/interrupt.h>

const uint8_t PIN_S1 = 2; // Interrupt 0 (S1)
const uint8_t PIN_S2 = 3; // Interrupt 1 (S2)
const uint8_t LED_PINS[8] = {4, 5, 6, 7, 8, 9, 10, 11};

volatile uint8_t counter = 0;
volatile bool flag_S1 = false;
volatile bool flag_S2 = false;

void updateLEDs(uint8_t val) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_PINS[i], (val >> i) & 0x01);
  }
}

// ISR สั้นๆ สำหรับปลุก CPU และตั้ง Flag
void isr_S1() {
  flag_S1 = true;
}

void isr_S2() {
  flag_S2 = true;
}

void enterSleepMode() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // กำหนดโหมดหลับลึก[cite: 1]
  sleep_enable();                       // เปิดการทำงาน Sleep[cite: 1]
  sleep_bod_disable();                  // ปิด BOD เพื่อประหยัดพลังงาน[cite: 1]
  
  sei();                                // เปิดใช้งาน Interrupts ทั้งหมด
  sleep_cpu();                          // สั่งให้ CPU เข้าสู่ภาวะหลับ[cite: 1]
  
  sleep_disable();                      // ปิดการทำงาน Sleep เมื่อตื่นขึ้นมา[cite: 1]
}

void setup() {
  for (int i = 0; i < 8; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }

  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);

  // ใช้วิธี LOW เพื่อให้แน่ใจว่าปลุกจาก SLEEP_MODE_PWR_DOWN ได้เสถียรที่สุด
  attachInterrupt(digitalPinToInterrupt(PIN_S1), isr_S1, LOW);
  attachInterrupt(digitalPinToInterrupt(PIN_S2), isr_S2, LOW);

  updateLEDs(counter);
}

void loop() {
  // เข้าสู่ภาวะหลับ
  enterSleepMode();

  // จัดการประมวลผลหลังตื่นจากการขัดจังหวะ
  if (flag_S1) {
    delay(50); // หน่วงเวลาป้องกันการสั่นของสวิตช์ (Debounce)
    if (digitalRead(PIN_S1) == LOW) {
      counter++;
      updateLEDs(counter);
    }
    flag_S1 = false;
    while (digitalRead(PIN_S1) == LOW); // รอกดปล่อย
  }

  if (flag_S2) {
    delay(50);
    if (digitalRead(PIN_S2) == LOW) {
      counter--;
      updateLEDs(counter);
    }
    flag_S2 = false;
    while (digitalRead(PIN_S2) == LOW); // รอกดปล่อย
  }
}
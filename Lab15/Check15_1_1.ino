//15.1.1
#include <avr/interrupt.h>

const uint8_t PIN_S1 = 2; // S1 (D2) -> เพิ่มค่า
const uint8_t PIN_S2 = 3; // S2 (D3) -> ลดค่า
const uint8_t LED_PINS[8] = {4, 5, 6, 7, 8, 9, 10, 11};

volatile uint8_t counter = 0;
volatile unsigned long last_interrupt_time = 0;

void updateLEDs(uint8_t val) {
  uint8_t active_low_val = ~val;
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_PINS[i], (active_low_val >> i) & 0x01);
  }
}

void isr_S1() {
  unsigned long current_time = millis();
  if (current_time - last_interrupt_time > 200) {
    counter++;
    updateLEDs(counter);
    last_interrupt_time = current_time;
  }
}

void isr_S2() {
  unsigned long current_time = millis();
  if (current_time - last_interrupt_time > 200) {
    counter--;
    updateLEDs(counter);
    last_interrupt_time = current_time;
  }
}

void setup() {
  for (int i = 0; i < 8; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
  
  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_S1), isr_S1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_S2), isr_S2, FALLING);

  ADCSRA |= (1 << ADEN); // เปิดใช้งาน ADC ดึงกระแสเพิ่ม
  updateLEDs(counter);
}

void loop() {
  // วนลูปประมวลผลต่อเนื่องเพื่อดึงกระแส CPU ให้ทำงานเต็มร้อย
  volatile float dummy = 3.14159 * analogRead(A0);
}
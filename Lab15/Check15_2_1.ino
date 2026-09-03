//15.2.1
const uint8_t PIN_S1 = 2; // S1 -> วิ่งนอกเข้าใน
const uint8_t PIN_S2 = 3; // S2 -> วิ่งในออกนอก
const uint8_t LED_PINS[8] = {4, 5, 6, 7, 8, 9, 10, 11};

// ดับ LED ทั้งหมด (Active Low จ่าย HIGH คือดับ)
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
      digitalWrite(LED_PINS[i], LOW);       // LOW คือติด
      digitalWrite(LED_PINS[7 - i], LOW);   // LOW คือติด
      delay(150);
    }
  }
  clearLEDs(); // ดับไฟทั้งหมดเมื่อจบ
}

// ไฟวิ่งจากตรงกลางออกสู่ขอบนอก (3 รอบ)
void runLED_Outward() {
  for (int round = 0; round < 3; round++) {
    for (int i = 3; i >= 0; i--) {
      clearLEDs();
      digitalWrite(LED_PINS[i], LOW);       // LOW คือติด
      digitalWrite(LED_PINS[7 - i], LOW);   // LOW คือติด
      delay(150);
    }
  }
  clearLEDs(); // ดับไฟทั้งหมดเมื่อจบ
}

void setup() {
  for (int i = 0; i < 8; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
  
  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);

  clearLEDs();
}

void loop() {
  // Polling เช็กปุ่ม S1
  if (digitalRead(PIN_S1) == LOW) {
    delay(50); // Debounce
    if (digitalRead(PIN_S1) == LOW) {
      runLED_Inward();
      while (digitalRead(PIN_S1) == LOW); // รอกดปล่อย
    }
  }

  // Polling เช็กปุ่ม S2
  if (digitalRead(PIN_S2) == LOW) {
    delay(50); // Debounce
    if (digitalRead(PIN_S2) == LOW) {
      runLED_Outward();
      while (digitalRead(PIN_S2) == LOW); // รอกดปล่อย
    }
  }
}
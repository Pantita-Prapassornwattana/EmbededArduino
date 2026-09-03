//15.3
#include <avr/sleep.h>
#include <avr/interrupt.h>

// --- ขาเชื่อมต่อ TM1638 (ใช้ขา Analog ตามฮาร์ดแวร์เดิม) ---
const uint8_t STROBE = A0;
const uint8_t CLOCK  = A1;
const uint8_t DATA   = A2;

// --- ตาราง Segment เลข 0-9 และ - d n
const uint8_t digitToSegment[] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
const uint8_t SEG_DASH  = 0x40; // - 
const uint8_t SEG_D     = 0x5E; // d 
const uint8_t SEG_N     = 0x54; // n
const uint8_t SEG_BLANK = 0x00;

// --- การสื่อสารระดับล่างกับ TM1638 (Bit-bang SPI) ---
void sendCommand(uint8_t value) {
  digitalWrite(STROBE, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, value);
  digitalWrite(STROBE, HIGH);
}

void resetTM() {
  sendCommand(0x40); // โหมด Auto-increment address
  digitalWrite(STROBE, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, 0xC0);
  for (uint8_t i = 0; i < 16; i++) {
    shiftOut(DATA, CLOCK, LSBFIRST, 0x00);
  }
  digitalWrite(STROBE, HIGH);
}

void displayAll(uint8_t seg[8]) {
  sendCommand(0x44); // โหมด Fixed address
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(STROBE, LOW);
    shiftOut(DATA, CLOCK, LSBFIRST, 0xC0 + (i * 2));
    shiftOut(DATA, CLOCK, LSBFIRST, seg[i]);
    digitalWrite(STROBE, HIGH);
  }
}

// อ่านสถานะปุ่มกด S1-S8 (คืนค่า Bitmask: Bit 0 = S1 ... Bit 7 = S8)
uint8_t readButtons() {
  uint8_t buttons = 0;
  digitalWrite(STROBE, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, 0x42);

  pinMode(DATA, INPUT);
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t v = shiftIn(DATA, CLOCK, LSBFIRST);
    buttons |= (v << i);
  }
  pinMode(DATA, OUTPUT);

  digitalWrite(STROBE, HIGH);
  return buttons;
}

// --- การจัดการ State และ ตัวแปรควบคุมระบบ ---
enum State { ST_IDLE, ST_RUN, ST_BLINK };
volatile State state = ST_IDLE;

volatile uint16_t centi = 1200;     // เวลาเริ่มต้น 12.00 วินาที (หน่วย 1/100 วินาที)
volatile uint16_t idleTicks = 0;    // นับเวลา ST_IDLE (1 tick = 10ms, 500 ticks = 5s)
volatile uint8_t  blinkPhase = 0;   // เฟสการกระพริบ 0..3 (ติด-ดับ 2 รอบ)
volatile uint16_t blinkTicks = 0;   // ตัวนับเวลาแต่ละเฟสกระพริบ (~300ms)
volatile bool     tick10 = false;   // Flag สัญญาณ 10ms จาก Timer Interrupt

uint8_t lastButtons = 0;
const uint8_t DEBOUNCE_TICKS = 3;   // Debounce 30ms (3 x 10ms)

// Debounce ด้วยวิธีตรวจสอบค่าซ้ำแบบสุ่มตรวจราย Tick
uint8_t readDebouncedButtons() {
  static uint8_t candidate = 0, count = 0, stable = 0;
  uint8_t raw = readButtons();

  if (raw != candidate) {
    candidate = raw;
    count = 0;
  } else if (count < DEBOUNCE_TICKS) {
    count++;
    if (count == DEBOUNCE_TICKS) {
      stable = candidate;
    }
  }
  return stable;
}

// --- ตั้งค่า Timer1 ให้ขัดจังหวะทุกๆ 10ms (CTC Mode) ---
void setupTimer1() {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A  = 2499;                         // 16MHz / (64 * 100Hz) - 1
  TCCR1B |= (1 << WGM12);                // CTC Mode
  TCCR1B |= (1 << CS11) | (1 << CS10);   // Prescaler 64
  TIMSK1 |= (1 << OCIE1A);               // เปิด Output Compare A Match Interrupt
  sei();
}

ISR(TIMER1_COMPA_vect) {
  tick10 = true;
}

// --- แสดงผลขึ้นหน้าจอ 7-Segment ---
void renderDisplay() {
  uint8_t seg[8] = {
    SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK,
    SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK
  };

  // แสดง "dn" เมื่ออยู่ในสถานะ ST_IDLE
  if (state == ST_IDLE) {
    seg[0] = SEG_D;
    seg[1] = SEG_N;
  }

  // ซ่อนตัวเลขในเฟสคี่ของ ST_BLINK (เพื่อให้เกิดการกระพริบ)
  bool showNumbers = !(state == ST_BLINK && (blinkPhase % 2 == 1));
  if (showNumbers) {
    uint16_t sec  = centi / 100;
    uint16_t hund = centi % 100;

    seg[3] = digitToSegment[sec / 10];
    seg[4] = digitToSegment[sec % 10];
    seg[5] = SEG_DASH;
    seg[6] = digitToSegment[hund / 10];
    seg[7] = digitToSegment[hund % 10];
  }

  displayAll(seg);
}

// --- เข้าสู่ Power-Down Sleep Mode ---
void goToSleep() {
  uint8_t off[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  displayAll(off); // ดับไฟหน้าจอทั้งหมด

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_bod_disable();
  sleep_cpu();
  sleep_disable();
}

// --- Setup และ Main Loop ---
void setup() {
  pinMode(STROBE, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(DATA, OUTPUT);

  sendCommand(0x8F); // เปิดจอ ความสว่างสูงสุด
  resetTM();

  lastButtons = readButtons();
  renderDisplay();
  setupTimer1();
}

void loop() {
  if (!tick10) return;
  tick10 = false;

  // อ่านปุ่มกดและหาขอบขาลง (Rising Edge ของการกดปุ่ม)
  uint8_t buttons = readDebouncedButtons();
  uint8_t pressed = buttons & (~lastButtons);
  lastButtons = buttons;

  // FSM ควบคุมสถานะการทำงาน
  switch (state) {
    case ST_IDLE:
      if (pressed & 0x80) { // กด S8 (Bit 7) เพื่อเริ่มนับถอยหลัง
        state = ST_RUN;
        centi = 1200;
      } else {
        idleTicks++;
        if (idleTicks >= 500) { // ไม่ได้กดปุ่มครบ 5 วินาที เข้าสู่ Sleep
          goToSleep();
          return;
        }
      }
      break;

    case ST_RUN:
      if (centi > 0) {
        centi--;
      }
      if (centi == 0) {
        state = ST_BLINK;
        blinkPhase = 0;
        blinkTicks = 0;
      }
      break;

    case ST_BLINK:
      blinkTicks++;
      if (blinkTicks >= 30) { // ครบ ~300ms เปลี่ยนเฟสการกระพริบ
        blinkTicks = 0;
        blinkPhase++;
        if (blinkPhase >= 4) { // กระพริบติด-ดับครบ 2 รอบแล้ว Sleep
          goToSleep();
          return;
        }
      }
      break;
  }

  renderDisplay();
}
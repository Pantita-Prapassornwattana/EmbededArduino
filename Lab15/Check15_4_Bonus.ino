//bonus
/* ==========================================================================
   240-319  Lab 15 : Sleep modes of ATmega328P
   Checkpoint 4 : วงจรนับถอยหลัง/นับเดินหน้า 12 วินาที สลับโหมดได้ด้วย S1
                  ตื่นจากภาวะหลับด้วย Watchdog Timer (TM1638) [Fast Polling]
   ========================================================================== */

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

// --- ขาเชื่อมต่อ TM1638 (STB->A0, CLK->A1, DIO->A2) ---
const uint8_t STROBE = A0;
const uint8_t CLOCK  = A1;
const uint8_t DATA   = A2;

const uint8_t digitToSegment[] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
const uint8_t SEG_DASH  = 0x40; // '-'
const uint8_t SEG_D     = 0x5E; // 'd'
const uint8_t SEG_N     = 0x54; // 'n'
const uint8_t SEG_U     = 0x1C; // 'u'
const uint8_t SEG_P     = 0x73; // 'P'
const uint8_t SEG_BLANK = 0x00;

const uint16_t COUNT_MAX = 1200; // 12.00 วินาที (หน่วย 1/100 วินาที)

// --- ฟังก์ชันการทำงานของ Watchdog Timer ---

// เผื่อใช้ที่อื่นคงเดิมไว้
void WDT_interrupt_enable_4sec() {
  cli();
  MCUSR &= ~(1 << WDRF);
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = (1 << WDIE) | (1 << WDP3);
  sei();
}

// แบบ Fast Polling (~125ms) สำหรับสแกนปุ่มกดอย่างรวดเร็ว
void WDT_interrupt_enable_fast() {
  cli();
  MCUSR &= ~(1 << WDRF);                       // เคลียร์ Reset Flag
  WDTCSR |= (1 << WDCE) | (1 << WDE);          // เปิดโหมดเปลี่ยนค่า WDT
  WDTCSR = (1 << WDIE) | (1 << WDP1) | (1 << WDP0); // Interrupt Mode + Timeout ~125ms
  sei();
}

void WDT_disable_custom() {
  cli();
  MCUSR &= ~(1 << WDRF);
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = 0x00;                             // ปิด WDT
  sei();
}

// --- ฟังก์ชันสื่อสารกับ TM1638 (Bit-bang SPI) ---
void sendCommand(uint8_t value) {
  digitalWrite(STROBE, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, value);
  digitalWrite(STROBE, HIGH);
}

void resetTM() {
  sendCommand(0x40);
  digitalWrite(STROBE, LOW);
  shiftOut(DATA, CLOCK, LSBFIRST, 0xC0);
  for (uint8_t i = 0; i < 16; i++) shiftOut(DATA, CLOCK, LSBFIRST, 0x00);
  digitalWrite(STROBE, HIGH);
}

void displayAll(uint8_t seg[8]) {
  sendCommand(0x44);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(STROBE, LOW);
    shiftOut(DATA, CLOCK, LSBFIRST, 0xC0 + (i * 2));
    shiftOut(DATA, CLOCK, LSBFIRST, seg[i]);
    digitalWrite(STROBE, HIGH);
  }
}

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

// --- ตัวแปรควบคุมสถานะ ---
enum Mode  { MODE_DOWN, MODE_UP };
enum State { ST_IDLE, ST_RUN, ST_BLINK };

volatile Mode  mode  = MODE_DOWN;
volatile State state = ST_IDLE;
volatile uint16_t centi = 0;        // ค่านับปัจจุบัน (1/100 วินาที)
volatile uint16_t idleTicks = 0;    // นับเวลา 5 วินาที (500 x 10ms)
volatile uint8_t  blinkPhase = 0;
volatile uint16_t blinkTicks = 0;
volatile bool tick10 = false;
volatile uint8_t wdtKeys = 0;       // เก็บค่าปุ่มที่อ่านได้จาก ISR(WDT_vect)

uint8_t lastButtons = 0;
const uint8_t DEBOUNCE_TICKS = 3;

uint8_t readDebouncedButtons() {
  static uint8_t candidate = 0, count = 0, stable = 0;
  uint8_t raw = readButtons();

  if (raw != candidate) {
    candidate = raw;
    count = 0;
  } else if (count < DEBOUNCE_TICKS) {
    count++;
    if (count == DEBOUNCE_TICKS) stable = candidate;
  }
  return stable;
}

void setupTimer1() {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A  = 2499;                          // 10ms @ 16MHz / prescaler 64
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

ISR(TIMER1_COMPA_vect) {
  tick10 = true;
}

// ISR ของ WDT อ่านค่าปุ่มจาก TM1638 ทุกๆ รอบ Timeout (~125ms)
ISR(WDT_vect) {
  digitalWrite(STROBE, HIGH);
  delayMicroseconds(10);
  wdtKeys = readButtons();
}

void renderDisplay() {
  uint8_t seg[8] = {SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK,
                    SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK};

  uint16_t showValue;

  if (state == ST_IDLE) {
    showValue = (mode == MODE_DOWN) ? COUNT_MAX : 0;
    if (mode == MODE_DOWN) { seg[0] = SEG_D; seg[1] = SEG_N; }
    else                   { seg[0] = SEG_U; seg[1] = SEG_P; }
  } else {
    showValue = centi;
  }

  bool showNumbers = !(state == ST_BLINK && (blinkPhase % 2 == 1));
  if (showNumbers) {
    uint16_t sec  = showValue / 100;
    uint16_t hund = showValue % 100;
    seg[3] = digitToSegment[sec / 10];
    seg[4] = digitToSegment[sec % 10];
    seg[5] = SEG_DASH;
    seg[6] = digitToSegment[hund / 10];
    seg[7] = digitToSegment[hund % 10];
  }

  displayAll(seg);
}

// หลับแบบ Power-Down แล้วใช้ WDT ตื่นสแกนปุ่มทุก ~125ms
void enterSleepUntilKeyPress() {
  uint8_t off[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  displayAll(off); // ดับหน้าจอก่อนหลับ

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  do {
    wdtKeys = 0;
    WDT_interrupt_enable_fast();          // ตื่นถี่ขึ้นทุก ~125ms
    sleep_enable();
    sleep_bod_disable();
    sei();
    sleep_cpu();                          // สั่งหลับ...
    sleep_disable();
  } while (wdtKeys == 0);                 // ถ้าไม่มีการกดปุ่ม จะกลับไปหลับต่อทันที

  WDT_disable_custom();                   // ตื่นขึ้นมาแล้วปิด WDT

  sendCommand(0x8F);                      // เปิดจอแสดงผล TM1638 คืนค่าความสว่าง
  
  lastButtons = wdtKeys;                  // กำหนดค่า Baseline ปุ่มกด
  state = ST_IDLE;
  idleTicks = 0;
  renderDisplay();                        // แสดงผลหน้าจอกลับมาปกติ
}

void setup() {
  pinMode(STROBE, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(DATA, OUTPUT);

  sendCommand(0x8F);
  resetTM();

  lastButtons = readButtons();
  renderDisplay();
  setupTimer1();
}

void loop() {
  if (!tick10) return;
  tick10 = false;

  uint8_t buttons = readDebouncedButtons();
  uint8_t pressed = buttons & (~lastButtons);
  lastButtons = buttons;

  switch (state) {
    case ST_IDLE:
      if (pressed & 0x80) {               // กด S8 = เริ่มนับ
        state = ST_RUN;
        centi = (mode == MODE_DOWN) ? COUNT_MAX : 0;
      } else if (pressed & 0x01) {         // กด S1 = สลับโหมด (DOWN <-> UP)
        mode = (mode == MODE_DOWN) ? MODE_UP : MODE_DOWN;
        idleTicks = 0;
      } else {
        idleTicks++;
        if (idleTicks >= 500) {            // ไม่กดปุ่มใดๆ ครบ 5 วินาที -> หลับ
          enterSleepUntilKeyPress();
          return;
        }
      }
      break;

    case ST_RUN:
      if (mode == MODE_DOWN) {
        if (centi > 0) centi--;
        if (centi == 0) { state = ST_BLINK; blinkPhase = 0; blinkTicks = 0; }
      } else {
        if (centi < COUNT_MAX) centi++;
        if (centi >= COUNT_MAX) { state = ST_BLINK; blinkPhase = 0; blinkTicks = 0; }
      }
      break;

    case ST_BLINK:
      blinkTicks++;
      if (blinkTicks >= 30) {             // ~300ms เปลี่ยนเฟสกระพริบ
        blinkTicks = 0;
        blinkPhase++;
        if (blinkPhase >= 6) {             // กระพริบติด-ดับครบ 2 รอบ -> หลับ
          enterSleepUntilKeyPress();
          return;
        }
      }
      break;
  }

  renderDisplay();
}
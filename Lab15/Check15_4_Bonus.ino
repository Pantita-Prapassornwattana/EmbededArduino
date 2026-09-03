#include <avr/sleep.h>
#include <avr/interrupt.h>
#include "my_wdt.h"

// --- ขาเชื่อมต่อ TM1638 (ตามรูปที่ 15.3 ที่ต่อจริง — ใช้ขา Analog) ---
const uint8_t STROBE = A0;
const uint8_t CLOCK  = A1;
const uint8_t DATA   = A2;

const uint8_t digitToSegment[] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
const uint8_t SEG_DASH  = 0x40; // 'g'          -> '-'
const uint8_t SEG_D     = 0x5E; // b,c,d,e,g    -> 'd'
const uint8_t SEG_N     = 0x54; // c,e,g        -> 'n'
const uint8_t SEG_U     = 0x1C; // c,d,e        -> 'u'
const uint8_t SEG_P     = 0x73; // a,b,e,f,g    -> 'P'
const uint8_t SEG_BLANK = 0x00;

const uint16_t COUNT_MAX = 1200; // 12.00 วินาที (หน่วย 1/100 วินาที)

// --- ฟังก์ชันสื่อสารกับ TM1638 (bit-bang เหมือน Lab 11 / Checkpoint 3) ---
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

// --- สถานะการทำงาน ---
enum Mode  { MODE_DOWN, MODE_UP };
enum State { ST_IDLE, ST_RUN, ST_BLINK };

volatile Mode  mode  = MODE_DOWN;
volatile State state = ST_IDLE;
volatile uint16_t centi = 0;        // ค่านับปัจจุบัน หน่วย 1/100 วินาที (ใช้ตอน ST_RUN/ST_BLINK)
volatile uint16_t idleTicks = 0;    // 10ms/tick, 500 = 5 วินาที
volatile uint8_t  blinkPhase = 0;
volatile uint16_t blinkTicks = 0;
volatile bool tick10 = false;
volatile uint8_t wdtKeys = 0;       // อ่านจาก ISR(WDT_vect) ตอนหลับ

uint8_t lastButtons = 0;
const uint8_t DEBOUNCE_TICKS = 3; // 3 x 10ms = ~30ms (แนวทางเดียวกับ Lab 9 Checkpoint 2)

// debounce แบบนับซ้ำ เหมือน Checkpoint 3 — ค่าที่อ่านได้ต้องเหมือนเดิมติดกันครบ
// DEBOUNCE_TICKS รอบ (tick 10ms) จึงจะถือว่าเป็นสถานะจริง
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
  OCR1A  = 2499;                          // 10ms @ 16MHz / prescaler64
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

ISR(TIMER1_COMPA_vect) {
  tick10 = true;
}

// ทำงานทุก ~4 วินาทีขณะหลับ: แอบอ่านคีย์ TM1638 (ตัวเดียวที่ CPU ทำได้ระหว่างหลับ)
ISR(WDT_vect) {
  wdtKeys = readButtons();
}

void renderDisplay() {
  uint8_t seg[8] = {SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK,
                     SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK};

  uint16_t showValue;
  bool showLabel = false;

  if (state == ST_IDLE) {
    showLabel = true;
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

// หลับ + ตื่นทุก ~4 วิด้วย WDT เพื่อแอบดูคีย์ วนจนกว่าจะมีคนกดจริง แล้วค่อยกลับสู่ idle
void enterSleepUntilKeyPress() {
  uint8_t off[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  displayAll(off);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  do {
    wdtKeys = 0;
    WDT_interrupt_enable(wdt_timeout_4sec); // WDIE ถูกฮาร์ดแวร์เคลียร์เองหลังทำงาน 1 ครั้ง ต้องเปิดใหม่ทุกรอบ
    sleep_enable();
    sleep_bod_disable();
    sleep_cpu();                            // หลับ...ตื่นทุก 4 วิ (หรือเร็วกว่านั้นถ้ามี interrupt อื่นแทรก)
    sleep_disable();
  } while (wdtKeys == 0);                   // ตื่นแบบไม่มีคีย์ถูกกด -> หลับต่อ
  WDT__disable();                            // กลับสู่การทำงานปกติแล้ว ปิด WDT กันมันไปรีเซ็ตเครื่องเอง

  // ปุ่มที่ทำให้ตื่นถือเป็น "การกดเพื่อปลุก" เท่านั้น ยังไม่ใช่คำสั่ง ต้องกดใหม่อีกครั้ง
  // (ตามที่สาธิตในคลิป) จึงเก็บสถานะปุ่มปัจจุบันไว้เป็น baseline ก่อน
  lastButtons = wdtKeys;
  state = ST_IDLE;
  idleTicks = 0;
  renderDisplay();
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
      if (pressed & 0x80) {                 // S8 = เริ่มนับ
        state = ST_RUN;
        centi = (mode == MODE_DOWN) ? COUNT_MAX : 0;
      } else if (pressed & 0x01) {           // S1 = สลับโหมด (ทำได้เฉพาะตอน idle)
        mode = (mode == MODE_DOWN) ? MODE_UP : MODE_DOWN;
        idleTicks = 0;
      } else {
        idleTicks++;
        if (idleTicks >= 500) {              // ไม่กดปุ่มครบ 5 วินาที -> หลับ
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
      if (blinkTicks >= 30) {                // ~300ms ต่อเฟส
        blinkTicks = 0;
        blinkPhase++;
        if (blinkPhase >= 6) {               // กระพริบครบ 2 รอบ -> หลับ
          enterSleepUntilKeyPress();
          return;
        }
      }
      break;
  }

  renderDisplay();
}

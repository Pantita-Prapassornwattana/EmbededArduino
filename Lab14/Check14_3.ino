//14.3
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

#define I2CADDR   0x20
#define BLINK_MS  400
#define YEAR_MIN  2000        // ช่วงปีที่ DS-1307 รองรับ
#define YEAR_MAX  2099

const byte ROWS = 4;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {7, 6, 5, 4};
byte colPins[COLS] = {3, 2, 1, 0};

LiquidCrystal_I2C lcd(0x27, 16, 2);   // บางรุ่นอาจเป็น 0x3F
Keypad_I2C keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR);

enum Mode  { MODE_RUN, MODE_SET_TIME, MODE_SET_DATE };
enum Field { F_HOUR, F_MIN, F_DAY, F_MON, F_YEAR };

Mode  mode  = MODE_RUN;
Field field = F_HOUR;

uint8_t e_hour = 0, e_min = 0, e_day = 1, e_mon = 1;
int     e_year = 2026;

bool          blink_on    = true;
unsigned long blink_last  = 0;
unsigned long draw_last   = 0;
bool          need_redraw = true;

uint8_t f_pos[3], f_len[3];   // ตำแหน่ง/ความยาวของ วัน เดือน ปี บนบรรทัดที่ 0

tmElements_t tm;
char line[17];

// ---------------------------------------------------------------- LCD helper
void lcd_line(uint8_t row, const char *s)
{
  char b[17];
  uint8_t i = 0;
  while (i < 16 && s[i]) { b[i] = s[i]; i++; }
  while (i < 16) b[i++] = ' ';
  b[16] = '\0';
  lcd.setCursor(0, row);
  lcd.print(b);
}

// สร้างบรรทัด "D/M/Y=" + วันที่ชิดขวา พร้อมจดตำแหน่งของแต่ละฟิลด์ไว้ใช้ตอนกะพริบ
void build_date_line(char *out, uint8_t d, uint8_t m, int y)
{
  char t[12];
  uint8_t p = 0, fp[3], fl[3];

  fl[0] = sprintf(t, "%d", d);       fp[0] = 0;  p  = fl[0];
  t[p++] = '/';
  fl[1] = sprintf(t + p, "%d", m);   fp[1] = p;  p += fl[1];
  t[p++] = '/';
  fl[2] = sprintf(t + p, "%d", y);   fp[2] = p;  p += fl[2];

  uint8_t off = 16 - p;              // จัดชิดขวาสุดของบรรทัด
  memset(out, ' ', 16);
  out[16] = '\0';
  memcpy(out, "D/M/Y=", 6);
  memcpy(out + off, t, p);

  for (uint8_t i = 0; i < 3; i++) {
    f_pos[i] = off + fp[i];
    f_len[i] = fl[i];
  }
}

// ------------------------------------------------------------- calendar rule
uint8_t days_in_month(uint8_t m, int y)
{
  static const uint8_t d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return d[m - 1];
}

// กันกรณีวันที่เกินจำนวนวันของเดือนใหม่ เช่น 31 -> เดือน 4 ต้องเหลือ 30
void clamp_day(void)
{
  uint8_t last = days_in_month(e_mon, e_year);
  if (e_day > last) e_day = last;
}

// ------------------------------------------------------------ RTC read/write
void load_from_rtc(void)
{
  if (RTC.read(tm)) {
    e_hour = tm.Hour;
    e_min  = tm.Minute;
    e_day  = tm.Day;
    e_mon  = tm.Month;
    e_year = tmYearToCalendar(tm.Year);
    if (e_mon < 1 || e_mon > 12) e_mon = 1;    // กันค่าขยะตอนไอซียังไม่เคยตั้งเวลา
    if (e_day < 1) e_day = 1;
    clamp_day();
  }
}

void save_to_rtc(void)
{
  tmElements_t w;
  w.Hour   = e_hour;
  w.Minute = e_min;
  w.Second = 0;
  w.Day    = e_day;
  w.Month  = e_mon;
  w.Year   = CalendarYrToTm(e_year);

  time_t t = makeTime(w);            // คำนวณวันในสัปดาห์ให้สอดคล้องกับวันที่
  breakTime(t, w);

  if (RTC.write(w)) Serial.println("Date/Time saved to DS1307");
  else              Serial.println("DS1307 write error");
}

// ------------------------------------------------------------------- display
void show_run(void)
{
  if (RTC.read(tm)) {
    build_date_line(line, tm.Day, tm.Month, tmYearToCalendar(tm.Year));
    lcd_line(0, line);
    sprintf(line, "Time = %02d:%02d", tm.Hour, tm.Minute);
    lcd_line(1, line);
  } else if (RTC.chipPresent()) {
    lcd_line(0, "DS1307 stopped");
    lcd_line(1, "Press A or B");
  } else {
    lcd_line(0, "DS1307 read err");
    lcd_line(1, "Check circuitry");
  }
}

void show_set(void)
{
  // ---- บรรทัดที่ 0 : วัน/เดือน/ปี (กะพริบฟิลด์ที่กำลังปรับในโหมด SET DATE)
  build_date_line(line, e_day, e_mon, e_year);
  if (!blink_on && mode == MODE_SET_DATE) {
    uint8_t i = field - F_DAY;                 // F_DAY/F_MON/F_YEAR -> 0/1/2
    for (uint8_t k = 0; k < f_len[i]; k++) line[f_pos[i] + k] = ' ';
  }
  lcd_line(0, line);

  // ---- บรรทัดที่ 1 : เวลา (กะพริบหลักที่กำลังปรับในโหมด SET TIME)
  sprintf(line, "Time = %02d:%02d SET", e_hour, e_min);
  if (!blink_on && mode == MODE_SET_TIME) {
    uint8_t p = (field == F_HOUR) ? 7 : 10;    // HH อยู่คอลัมน์ 7-8, MM อยู่ 10-11
    line[p] = ' ';
    line[p + 1] = ' ';
  }
  lcd_line(1, line);
}

// ------------------------------------------------------------- key handlers
void adjust(int8_t step)
{
  switch (field) {
    case F_HOUR:
      e_hour = (uint8_t)((e_hour + 24 + step) % 24);
      break;
    case F_MIN:
      e_min  = (uint8_t)((e_min + 60 + step) % 60);
      break;
    case F_DAY: {
      uint8_t last = days_in_month(e_mon, e_year);
      e_day = (uint8_t)((e_day - 1 + last + step) % last) + 1;   // วน 1..last
      break;
    }
    case F_MON:
      e_mon = (uint8_t)((e_mon - 1 + 12 + step) % 12) + 1;       // วน 1..12
      clamp_day();
      break;
    case F_YEAR: {
      int span = YEAR_MAX - YEAR_MIN + 1;
      e_year = (e_year - YEAR_MIN + span + step) % span + YEAR_MIN;
      clamp_day();                                               // เผื่อ 29 ก.พ.
      break;
    }
  }
}

void restart_blink(void)
{
  blink_on   = true;
  blink_last = millis();
}

void handle_key(char key)
{
  switch (key) {
    case 'A':                                  // เข้า/เลื่อนโหมดปรับ ว/ด/ป
      if (mode != MODE_SET_DATE) {
        if (mode == MODE_RUN) load_from_rtc();
        mode  = MODE_SET_DATE;
        field = F_DAY;
      } else {
        field = (field == F_DAY) ? F_MON : (field == F_MON) ? F_YEAR : F_DAY;
      }
      restart_blink();
      break;

    case 'B':                                  // เข้า/สลับโหมดปรับเวลา
      if (mode != MODE_SET_TIME) {
        if (mode == MODE_RUN) load_from_rtc();
        mode  = MODE_SET_TIME;
        field = F_HOUR;
      } else {
        field = (field == F_HOUR) ? F_MIN : F_HOUR;
      }
      restart_blink();
      break;

    case 'C':
      if (mode != MODE_RUN) adjust(+1);
      break;

    case 'D':
      if (mode != MODE_RUN) adjust(-1);
      break;

    case '#':                                  // บันทึกลง DS-1307 แล้วออก
      if (mode != MODE_RUN) {
        save_to_rtc();
        mode = MODE_RUN;
      }
      break;

    default:
      break;
  }
}

// ------------------------------------------------------------------ main
void setup()
{
  Serial.begin(38400);
  Wire.begin();
  keypad.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  blink_last = millis();
}

void loop()
{
  char key = keypad.getKey();
  if (key != NO_KEY) { handle_key(key); need_redraw = true; }

  if (millis() - blink_last >= BLINK_MS) {
    blink_last = millis();
    blink_on = !blink_on;
    need_redraw = true;
  }

  // วาดจอทุก 250 มิลลิวินาที หรือเมื่อมีเหตุให้ค่าบนจอเปลี่ยน
  if (need_redraw || millis() - draw_last >= 250) {
    draw_last   = millis();
    need_redraw = false;
    if (mode == MODE_RUN) show_run();
    else                  show_set();
  }
}
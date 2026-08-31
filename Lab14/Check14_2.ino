//14.2
//14.2
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

#define I2CADDR   0x20   // address ของ PCF8574 (คีย์แพ็ด)
#define BLINK_MS  400     // ความเร็วกะพริบของตัวเลขที่กำลังตั้งค่า (มิลลิวินาที)

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

LiquidCrystal_I2C lcd(0x27, 16, 2);
Keypad_I2C keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR);

// ----- สถานะการทำงาน -----
// isSetMode: อยู่ในโหมดตั้งเวลาหรือไม่
// isEditingHour: ตอนอยู่ในโหมดตั้งเวลา กำลังแก้ "ชั่วโมง" หรือ "นาที"
bool isSetMode = false;
bool isEditingHour = true;   // true = แก้ชั่วโมง, false = แก้นาที

// ค่าที่กำลังแก้ไข (คัดลอกมาจาก RTC ตอนเข้าโหมดตั้งเวลา)
uint8_t editHour = 0, editMinute = 0;
uint8_t editDay = 1, editMonth = 1;
int     editYear = 2026;

// สำหรับทำให้ตัวเลขที่กำลังตั้งค่ากะพริบ
bool          blinkOn   = true;
unsigned long lastBlink = 0;

// วาดจอใหม่เฉพาะเมื่อจำเป็น 
unsigned long lastDraw   = 0;
bool          needRedraw = true;

tmElements_t tm;
char lineBuf[17];

// เติมช่องว่างให้ครบ 16 ตัวอักษรเสมอ ป้องกันตัวอักษรเก่าค้างจอ
void printLine(uint8_t row, const char *text)
{
  char padded[17];
  uint8_t i = 0;
  while (i < 16 && text[i]) { padded[i] = text[i]; i++; }
  while (i < 16) padded[i++] = ' ';
  padded[16] = '\0';

  lcd.setCursor(0, row);
  lcd.print(padded);
}

// จัดรูปแบบ "D/M/Y= xx/xx/xxxx" โดยจัดชิดขวาให้พอดี 16 ตัวอักษร
// (เพราะวัน/เดือนไม่เติมศูนย์นำหน้า ความยาวจึงไม่คงที่)
void formatDateLine(char *out, uint8_t d, uint8_t m, int y)
{
  char temp[12];
  uint8_t len = sprintf(temp, "%d/%d/%d", d, m, y);

  memset(out, ' ', 16);
  out[16] = '\0';
  memcpy(out, "D/M/Y=", 6);
  memcpy(out + (16 - len), temp, len);
}

// ----- อ่าน/เขียนค่าจาก DS1307 -----

void loadFromRTC()
{
  if (RTC.read(tm)) {
    editHour   = tm.Hour;
    editMinute = tm.Minute;
    editDay    = tm.Day;
    editMonth  = tm.Month;
    editYear   = tmYearToCalendar(tm.Year);
  }
  // ถ้าอ่านไม่ได้ (RTC หยุดทำงาน) ให้ใช้ค่าตั้งต้นที่กำหนดไว้แทน
}

void saveToRTC()
{
  tmElements_t newTime;
  newTime.Hour   = editHour;
  newTime.Minute = editMinute;
  newTime.Second = 0;                    // เริ่มนับวินาทีใหม่ที่ 0
  newTime.Day    = editDay;
  newTime.Month  = editMonth;
  newTime.Year   = CalendarYrToTm(editYear);

  // คำนวณวันในสัปดาห์ (dayOfWeek) ให้ถูกต้องอัตโนมัติ
  time_t t = makeTime(newTime);
  breakTime(t, newTime);

  if (RTC.write(newTime)) {
    Serial.println("Time saved to DS1307");
  } else {
    Serial.println("DS1307 write error");
  }
}

// ----- แสดงผลบน LCD -----

void showRunMode()
{
  if (RTC.read(tm)) {
    formatDateLine(lineBuf, tm.Day, tm.Month, tmYearToCalendar(tm.Year));
    printLine(0, lineBuf);

    sprintf(lineBuf, "Time = %02d:%02d", tm.Hour, tm.Minute);
    printLine(1, lineBuf);
  }
  else if (RTC.chipPresent()) {
    // เจอไอซี แต่ยังไม่เคยตั้งเวลา (บิต CH ยังเป็น 1)
    printLine(0, "DS1307 stopped");
    printLine(1, "Press B to set");
  }
  else {
    printLine(0, "DS1307 read err");
    printLine(1, "Check circuitry");
  }
}

void showSetMode()
{
  formatDateLine(lineBuf, editDay, editMonth, editYear);
  printLine(0, lineBuf);

  sprintf(lineBuf, "Time = %02d:%02d SET", editHour, editMinute);

  // ทำให้หลักที่กำลังแก้ไขกะพริบ โดยลบตัวเลขออกชั่วคราว
  // "Time = HH:MM SET" -> ชั่วโมงอยู่คอลัมน์ 7-8, นาทีอยู่คอลัมน์ 10-11
  if (!blinkOn) {
    uint8_t pos = isEditingHour ? 7 : 10;
    lineBuf[pos]     = ' ';
    lineBuf[pos + 1] = ' ';
  }
  printLine(1, lineBuf);
}

// ----- จัดการปุ่มกด -----

// หมายเหตุ: ตั้งชื่อ adjustField (ไม่ใช่ adjustTime) เพราะ TimeLib.h
// มีฟังก์ชัน adjustTime(long) อยู่แล้ว ถ้าใช้ชื่อซ้ำจะ compile ไม่ผ่าน
void adjustField(int8_t step)
{
  if (isEditingHour) {
    editHour = (editHour + 24 + step) % 24;
  } else {
    editMinute = (editMinute + 60 + step) % 60;
  }
}

void handleKey(char key)
{
  switch (key) {

    case 'B':
      if (!isSetMode) {
        loadFromRTC();          // ดึงเวลาปัจจุบันมาเป็นค่าตั้งต้น
        isSetMode = true;
        isEditingHour = true;
      } else {
        isEditingHour = !isEditingHour;   // สลับชั่วโมง <-> นาที
      }
      blinkOn   = true;          // เริ่มจังหวะกะพริบใหม่ให้เห็นชัดทันที
      lastBlink = millis();
      break;

    case 'C':
      if (isSetMode) adjustField(+1);
      break;

    case 'D':
      if (isSetMode) adjustField(-1);
      break;

    case '#':
      if (isSetMode) {
        saveToRTC();
        isSetMode = false;
      }
      break;

    default:
      break; // ปุ่มอื่นไม่ใช้ใน Checkpoint นี้
  }
}

// ----- main -----

void setup()
{
  Serial.begin(38400);
  Wire.begin();
  keypad.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lastBlink = millis();
}

void loop()
{
  char key = keypad.getKey();
  if (key != NO_KEY) {
    Serial.print(key);
    Serial.println(" is pressed");
    handleKey(key);
    needRedraw = true;
  }

  // สลับสถานะกะพริบทุก BLINK_MS
  if (millis() - lastBlink >= BLINK_MS) {
    lastBlink = millis();
    blinkOn = !blinkOn;
    needRedraw = true;
  }

  // วาดจอเฉพาะเมื่อมีการเปลี่ยนแปลง หรือทุก 250 ms
  if (needRedraw || millis() - lastDraw >= 250) {
    lastDraw   = millis();
    needRedraw = false;
    isSetMode ? showSetMode() : showRunMode();
  }
}
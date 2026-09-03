//19.3
//19.3
#include <Arduino_FreeRTOS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

#define I2CADDR_KEYPAD 0x20
#define YEAR_MIN       2000
#define YEAR_MAX       2099
#define BLINK_MS       400

const byte ROWS = 4, COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {7, 6, 5, 4};
byte colPins[COLS] = {3, 2, 1, 0};

LiquidCrystal_I2C lcd(0x27, 16, 2);
Keypad_I2C keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR_KEYPAD);

enum Mode  { MODE_RUN, MODE_SET_TIME, MODE_SET_DATE };
enum Field { F_HOUR, F_MIN, F_DAY, F_MON, F_YEAR };

Mode  mode  = MODE_RUN;
Field field = F_HOUR;

uint8_t e_hour = 0, e_min = 0, e_day = 1, e_mon = 1;
int     e_year = 2026;
tmElements_t tm;

bool blink_on = true;
uint8_t f_pos[3], f_len[3];

// ============================================================================
// [CHECKPOINT 3] EEPROM 24C32 & WEAR LEVELING
// ============================================================================
#define EEPROM_ADDR    0x50
#define EEPROM_SLOTS   2048

uint8_t display_mode = 1; // Mode 1: Date บน / Time ล่าง, Mode 2: Time บน / Date ล่าง
uint16_t current_slot = 0;
uint8_t current_seq = 0;

void ext_eeprom_write_byte(uint16_t mem_addr, uint8_t data) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((int)(mem_addr >> 8));
  Wire.write((int)(mem_addr & 0xFF));
  Wire.write(data);
  Wire.endTransmission();
  delay(10);
}

uint8_t ext_eeprom_read_byte(uint16_t mem_addr) {
  uint8_t rdata = 0xFF;
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((int)(mem_addr >> 8));
  Wire.write((int)(mem_addr & 0xFF));
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)EEPROM_ADDR, (uint8_t)1);
  if (Wire.available()) rdata = Wire.read();
  return rdata;
}

// [CHECKPOINT 3] อ่านค่า Mode ล่าสุดจาก EEPROM 24C32 ด้วย Wear Leveling เมื่อเริ่มทำงาน
void load_display_mode_wl(void) {
  uint8_t max_seq = 0;
  uint16_t best_slot = 0;
  bool found = false;

  for (uint16_t slot = 0; slot < EEPROM_SLOTS; slot++) {
    uint16_t addr = slot * 2;
    uint8_t val = ext_eeprom_read_byte(addr);
    uint8_t seq = ext_eeprom_read_byte(addr + 1);

    if (val == 1 || val == 2) {
      if (!found || (uint8_t)(seq - max_seq) < 128) {
        max_seq = seq;
        best_slot = slot;
        display_mode = val;
        found = true;
      }
    }
  }

  if (found) {
    current_slot = best_slot;
    current_seq = max_seq;
  } else {
    display_mode = 1;
    current_slot = 0;
    current_seq = 0;
    ext_eeprom_write_byte((uint16_t)0, (uint8_t)1);
    ext_eeprom_write_byte((uint16_t)1, (uint8_t)0);
  }
}

// [CHECKPOINT 3] บันทึก Mode ใหม่ลง EEPROM 24C32 โดยวน Slot เพื่อทำ Wear Leveling
void save_display_mode_wl(uint8_t new_mode) {
  current_slot = (current_slot + 1) % EEPROM_SLOTS;
  current_seq++;

  uint16_t addr = current_slot * 2;
  ext_eeprom_write_byte(addr, new_mode);
  ext_eeprom_write_byte(addr + 1, current_seq);

  display_mode = new_mode;
}

// ============================================================================
// [CHECKPOINT 1 & 2] RTC DATA MANAGEMENT
// ============================================================================
uint8_t days_in_month(uint8_t m, int y) {
  static const uint8_t d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return d[m - 1];
}

void clamp_day(void) {
  uint8_t last = days_in_month(e_mon, e_year);
  if (e_day > last) e_day = last;
}

void load_from_rtc(void) {
  if (RTC.read(tm)) {
    e_hour = tm.Hour;   e_min  = tm.Minute;
    e_day  = tm.Day;    e_mon  = tm.Month;
    e_year = tmYearToCalendar(tm.Year);
    if (e_mon < 1 || e_mon > 12) e_mon = 1;
    if (e_day < 1) e_day = 1;
    clamp_day();
  }
}

// [CHECKPOINT 1 & 2] บันทึกเวลา และ วัน/เดือน/ปี ใหม่ลง DS1307 เมื่อกด #
void save_to_rtc(void) {
  tmElements_t w;
  w.Hour   = e_hour;
  w.Minute = e_min;
  w.Second = 0;
  w.Day    = e_day;
  w.Month  = e_mon;
  w.Year   = CalendarYrToTm(e_year);

  time_t t = makeTime(w);
  breakTime(t, w);
  RTC.write(w);
}

// [CHECKPOINT 1 & 2] ปรับค่าตามช่วงขอบเขต (00-23, 00-59, วัน/เดือน/ปี)
void adjust(int8_t step) {
  switch (field) {
    case F_HOUR:
      e_hour = (uint8_t)((e_hour + 24 + step) % 24);
      break;
    case F_MIN:
      e_min  = (uint8_t)((e_min + 60 + step) % 60);
      break;
    case F_DAY: {
      uint8_t last = days_in_month(e_mon, e_year);
      e_day = (uint8_t)((e_day - 1 + last + step) % last) + 1;
      break;
    }
    case F_MON:
      e_mon = (uint8_t)((e_mon - 1 + 12 + step) % 12) + 1;
      clamp_day();
      break;
    case F_YEAR: {
      int span = YEAR_MAX - YEAR_MIN + 1;
      e_year = (e_year - YEAR_MIN + span + step) % span + YEAR_MIN;
      clamp_day();
      break;
    }
  }
}

// ============================================================================
// LCD HELPERS & DISPLAY
// ============================================================================
void lcd_line(uint8_t row, const char *s) {
  char b[17];
  uint8_t i = 0;
  while (i < 16 && s[i]) { b[i] = s[i]; i++; }
  while (i < 16) b[i++] = ' ';
  b[16] = '\0';
  lcd.setCursor(0, row);
  lcd.print(b);
}

void build_date_line(char *out, uint8_t d, uint8_t m, int y) {
  char t[12];
  uint8_t p = 0, fp[3], fl[3];

  fl[0] = sprintf(t, "%d", d);        fp[0] = 0;  p  = fl[0];
  t[p++] = '/';
  fl[1] = sprintf(t + p, "%02d", m); fp[1] = p;  p += fl[1];
  t[p++] = '/';
  fl[2] = sprintf(t + p, "%04d", y); fp[2] = p;  p += fl[2];

  uint8_t off = 16 - p;
  memset(out, ' ', 16);
  out[16] = '\0';
  memcpy(out, "D/M/Y=", 6);
  memcpy(out + off, t, p);

  for (uint8_t i = 0; i < 3; i++) {
    f_pos[i] = off + fp[i];
    f_len[i] = fl[i];
  }
}

void show_run(void) {
  char line_date[17], line_time[17];
  
  if (RTC.read(tm)) {
    build_date_line(line_date, tm.Day, tm.Month, tmYearToCalendar(tm.Year));
    // [CHECKPOINT 1] แสดงผลในรูปแบบเลขสองหลักเสมอ (HH:MM)
    sprintf(line_time, "   Time = %02d:%02d", tm.Hour, tm.Minute);

    // [CHECKPOINT 3] ทอกเกิลการแสดงผลระหว่าง Mode 1 และ Mode 2
    if (display_mode == 1) { 
      lcd_line(0, line_date);
      lcd_line(1, line_time);
    } else { 
      lcd_line(0, line_time);
      lcd_line(1, line_date);
    }
  } else {
    lcd_line(0, "DS1307 Read Err");
    lcd_line(1, "Check circuitry");
  }
}

void show_set(void) {
  char line_date[17], line_time[17];

  build_date_line(line_date, e_day, e_mon, e_year);
  if (!blink_on && mode == MODE_SET_DATE) {
    uint8_t i = field - F_DAY;
    for (uint8_t k = 0; k < f_len[i]; k++) line_date[f_pos[i] + k] = ' ';
  }

  sprintf(line_time, "   Time = %02d:%02d", e_hour, e_min);
  if (!blink_on && mode == MODE_SET_TIME) {
    if (field == F_HOUR) {
      line_time[10] = ' '; line_time[11] = ' ';
    } else {
      line_time[13] = ' '; line_time[14] = ' ';
    }
  }

  // [CHECKPOINT 3] แสดงสลับบรรทัดตาม Mode ที่เลือกใน EEPROM
  if (display_mode == 1) {
    lcd_line(0, line_date);
    lcd_line(1, line_time);
  } else {
    lcd_line(0, line_time);
    lcd_line(1, line_date);
  }
}

// ============================================================================
// KEYPAD LOGIC (CHECKPOINT 1, 2, 3)
// ============================================================================
void handle_key(char key) {
  // [CHECKPOINT 3] ปุ่ม 0: ทอกเกิลสลับ Mode 1 <-> Mode 2 และเซฟลง EEPROM 24C32
  if (key == '0') {
    if (mode == MODE_RUN) {
      uint8_t next_mode = (display_mode == 1) ? 2 : 1;
      save_display_mode_wl(next_mode);
    }
    return;
  }

  switch (key) {
    // [CHECKPOINT 2] ปุ่ม A: เข้าโหมดสลับตั้งค่า วัน -> เดือน -> ปี ค.ศ. -> วัน
    case 'A':
      if (mode != MODE_SET_DATE) { 
        if (mode == MODE_RUN) load_from_rtc();
        mode  = MODE_SET_DATE;
        field = F_DAY;
      } else {
        field = (field == F_DAY) ? F_MON : (field == F_MON) ? F_YEAR : F_DAY;
      }
      blink_on = true;
      break;

    // [CHECKPOINT 1] ปุ่ม B: เข้าโหมดสลับตั้งค่า ชั่วโมง -> นาที -> ชั่วโมง
    case 'B':
      if (mode != MODE_SET_TIME) {
        if (mode == MODE_RUN) load_from_rtc();
        mode  = MODE_SET_TIME;
        field = F_HOUR;
      } else {
        field = (field == F_HOUR) ? F_MIN : F_HOUR;
      }
      blink_on = true;
      break;

    // [CHECKPOINT 1 & 2] ปุ่ม C: กดเพื่อเพิ่มค่า (ชั่วโมง/นาที/วัน/เดือน/ปี)
    case 'C':
      if (mode != MODE_RUN) adjust(+1);
      break;

    // [CHECKPOINT 1 & 2] ปุ่ม D: กดเพื่อลดค่า (ชั่วโมง/นาที/วัน/เดือน/ปี)
    case 'D':
      if (mode != MODE_RUN) adjust(-1);
      break;

    // [CHECKPOINT 1 & 2] ปุ่ม #: ออกจากโหมดตั้งค่า และบันทึกลง DS1307
    case '#':
      if (mode != MODE_RUN) {
        save_to_rtc();
        mode = MODE_RUN;
      }
      break;

    default:
      break;
  }
}

// ============================================================================
// [CHECKPOINT 1, 2, 3] FREERTOS TASKS
// ============================================================================
// [CHECKPOINT 1] งานสแกนการกดปุ่มจาก Keypad แยกออกจากระบบหลัก
void TaskKeypad(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      handle_key(key);
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// [CHECKPOINT 1] งานอัปเดตและแสดงผลหน้าจอ LCD
void TaskDisplay(void *pvParameters) {
  (void) pvParameters;
  TickType_t xLastBlinkTime = xTaskGetTickCount();

  for (;;) {
    if ((xTaskGetTickCount() - xLastBlinkTime) >= pdMS_TO_TICKS(BLINK_MS)) {
      xLastBlinkTime = xTaskGetTickCount();
      blink_on = !blink_on;
    }

    if (mode == MODE_RUN) {
      show_run();
    } else {
      show_set();
    }

    vTaskDelay(150 / portTICK_PERIOD_MS);
  }
}

// ============================================================================
// SYSTEM INITIALIZATION
// ============================================================================
void setup() {
  Wire.begin();
  keypad.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // [CHECKPOINT 3] ดึงค่า Mode ล่าสุดจาก EEPROM 24C32 ตอนเปิดเครื่อง
  load_display_mode_wl();

  // [CHECKPOINT 1] สร้าง Task สำหรับ FreeRTOS Scheduler
  xTaskCreate(TaskKeypad,  "KeypadTask",  128, NULL, 2, NULL);
  xTaskCreate(TaskDisplay, "DisplayTask", 192, NULL, 1, NULL);
}

void loop() {
  // FreeRTOS เป็นผู้จัดการ Context Switch
}
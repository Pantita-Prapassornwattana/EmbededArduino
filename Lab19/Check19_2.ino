//19.2
#include <Arduino_FreeRTOS.h>
#include <semphr.h>          // เพิ่ม Header File สำหรับใช้งาน Semaphore / Mutex
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

#define I2CADDR_KEYPAD 0x20  // กำหนด Address I2C สำหรับ Keypad
#define YEAR_MIN       2000  // ขอบเขตปีขั้นต่ำ
#define YEAR_MAX       2099  // ขอบเขตปีสูงสุด
#define BLINK_MS       400   // ความเร็วในการกระพริบตัวเลข (มิลลิวินาที)

// ประกาศตัวแปร Global สำหรับเก็บกุญแจ Mutex ล็อกบัส I2C
SemaphoreHandle_t xI2CMutex;

// กำหนดขนาดและแผนผังปุ่มกดของ Keypad 4x4
const byte ROWS = 4, COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {7, 6, 5, 4};
byte colPins[COLS] = {3, 2, 1, 0};

// ประกาศ Object ของ LCD และ Keypad I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);
Keypad_I2C keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR_KEYPAD);

// กำหนด Enum สำหรับโหมดการทำงานและฟิลด์ที่กำลังปรับแต่ง
enum Mode  { MODE_RUN, MODE_SET_TIME, MODE_SET_DATE };
enum Field { F_HOUR, F_MIN, F_DAY, F_MON, F_YEAR };

Mode  mode  = MODE_RUN;
Field field = F_HOUR;

// ตัวแปรสำหรับเก็บค่าเวลาและวันที่ชั่วคราวขณะแก้ไข
uint8_t e_hour = 0, e_min = 0, e_day = 1, e_mon = 1;
int     e_year = 2026;
tmElements_t tm;

bool blink_on = true;        // สถานะเปิด/ปิด การกระพริบของตัวอักษร
uint8_t f_pos[3], f_len[3];  // เก็บตำแหน่งและขนาดของฟิลด์วันที่บน LCD

// ============================================================================
// RTC DATA MANAGEMENT
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

// แสดงผลเวลาและวันที่ในโหมดทำงานปกติ (MODE_RUN)
// (ตัดการสลับบรรทัดออกแล้ว บรรทัดบนเป็นวันที่ บรรทัดล่างเป็นเวลาเสมอ)
void show_run(void) {
  char line_date[17], line_time[17];

  if (RTC.read(tm)) {
    build_date_line(line_date, tm.Day, tm.Month, tmYearToCalendar(tm.Year));
    sprintf(line_time, "   Time = %02d:%02d", tm.Hour, tm.Minute);

    lcd_line(0, line_date);
    lcd_line(1, line_time);
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

  // (ตัดการสลับบรรทัดออกแล้ว บรรทัดบนเป็นวันที่ บรรทัดล่างเป็นเวลาเสมอ)
  lcd_line(0, line_date);
  lcd_line(1, line_time);
}

// ============================================================================
// KEYPAD LOGIC
// ============================================================================
// (ตัดปุ่ม 0 สำหรับสลับบรรทัดออกแล้ว)
void handle_key(char key) {
  switch (key) {
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

    case 'C':
      if (mode != MODE_RUN) adjust(+1);
      break;

    case 'D':
      if (mode != MODE_RUN) adjust(-1);
      break;

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
// FREERTOS TASKS
// ============================================================================
void TaskKeypad(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE) {
      char key = keypad.getKey();
      if (key != NO_KEY) {
        handle_key(key);
      }
      xSemaphoreGive(xI2CMutex);
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void TaskDisplay(void *pvParameters) {
  (void) pvParameters;
  TickType_t xLastBlinkTime = xTaskGetTickCount();

  for (;;) {
    if ((xTaskGetTickCount() - xLastBlinkTime) >= pdMS_TO_TICKS(BLINK_MS)) {
      xLastBlinkTime = xTaskGetTickCount();
      blink_on = !blink_on;
    }

    if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE) {
      if (mode == MODE_RUN) {
        show_run();
      } else {
        show_set();
      }
      xSemaphoreGive(xI2CMutex);
    }

    vTaskDelay(150 / portTICK_PERIOD_MS);
  }
}

// ============================================================================
// SYSTEM INITIALIZATION
// ============================================================================
void setup() {
  Wire.begin();

  xI2CMutex = xSemaphoreCreateMutex();

  keypad.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  xTaskCreate(TaskKeypad,  "KeypadTask",  128, NULL, 2, NULL);
  xTaskCreate(TaskDisplay, "DisplayTask", 192, NULL, 1, NULL);
}

void loop() {
}
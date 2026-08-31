//17.2
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Wire.h>
#include "my_EEPROM.h"

// ==========================================
// 1. ค่าคงที่และการตั้งค่าฮาร์ดแวร์
// ==========================================
#define EEPROM_SIZE 1024
#define I2C_ADDR    0x20
#define BLINK_MS    1000  // คาบเวลากระพริบ (มิลลิวินาที)

// ขาเชื่อมต่อโมดูล TM1638
const int STB_PIN = 4;
const int DIO_PIN = 2;
const int CLK_PIN = 3;

// การตั้งค่า Keypad 4x4 ผ่าน PCF8574
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {7, 6, 5, 4}; // P7-P4 ของ PCF8574
byte colPins[COLS] = {3, 2, 1, 0}; // P3-P0 ของ PCF8574

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2C_ADDR);

// ตารางรหัส 7-Segment สำหรับเลขฐาน 16 (0-F)
const uint8_t segmentMap[16] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, // 0-7
  0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71  // 8-9, A, b, C, d, E, F
};

// ==========================================
// 2. ตัวแปรสถานะการทำงาน (State Machine)
// ==========================================
enum SystemState { 
  ST1_IDLE, // สถานะพัก (รอกด S1/S8)
  ST3_ADDR, // รับค่า Address (ฐาน 10)
  ST4_DATA  // รับค่า Data (ฐาน 16)
};

SystemState currentState = ST1_IDLE;

uint16_t targetAddr = 0;
uint8_t  targetData = 0;
bool     dataEntered = false;

bool          blinkState = true;
unsigned long blinkTimer = 0;
uint8_t       lastButtonMask = 0;

// ==========================================
// 3. ฟังก์ชันควบคุมโมดูล TM1638
// ==========================================
void sendTM1638Cmd(uint8_t cmd) {
  digitalWrite(STB_PIN, LOW);
  shiftOut(DIO_PIN, CLK_PIN, LSBFIRST, cmd);
  digitalWrite(STB_PIN, HIGH);
}

void clearTM1638Display() {
  sendTM1638Cmd(0x40); // Auto increment address
  digitalWrite(STB_PIN, LOW);
  shiftOut(DIO_PIN, CLK_PIN, LSBFIRST, 0xC0);
  for (uint8_t i = 0; i < 16; i++) {
    shiftOut(DIO_PIN, CLK_PIN, LSBFIRST, 0x00);
  }
  digitalWrite(STB_PIN, HIGH);
}

uint8_t readTM1638Buttons() {
  uint8_t buttonMask = 0;
  digitalWrite(STB_PIN, LOW);
  shiftOut(DIO_PIN, CLK_PIN, LSBFIRST, 0x42); // คำสั่งอ่านปุ่มกด

  pinMode(DIO_PIN, INPUT);
  for (uint8_t i = 0; i < 4; i++) {
    buttonMask |= (shiftIn(DIO_PIN, CLK_PIN, LSBFIRST) << i);
  }
  pinMode(DIO_PIN, OUTPUT);

  digitalWrite(STB_PIN, HIGH);
  return buttonMask;
}

void writeSegments(uint8_t segments[8]) {
  sendTM1638Cmd(0x44); // แก้ไขตรงนี้จาก sendCommand เป็น sendTM1638Cmd
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(STB_PIN, LOW);
    shiftOut(DIO_PIN, CLK_PIN, LSBFIRST, 0xC0 + (i * 2));
    shiftOut(DIO_PIN, CLK_PIN, LSBFIRST, segments[i]);
    digitalWrite(STB_PIN, HIGH);
  }
}

// ==========================================
// 4. ฟังก์ชันจัดการการแสดงผลและเขียนข้อมูล
// ==========================================
void updateDisplay() {
  uint8_t seg[8] = {0};

  if (currentState != ST1_IDLE) {
    // แสดง Address 4 หลักซ้ายมือ
    bool showAddr = (currentState != ST3_ADDR) || blinkState;
    if (showAddr) {
      seg[0] = segmentMap[(targetAddr / 1000) % 10];
      seg[1] = segmentMap[(targetAddr / 100) % 10];
      seg[2] = segmentMap[(targetAddr / 10) % 10];
      seg[3] = segmentMap[targetAddr % 10];
    }

    // แสดง Data 2 หลักขวามือ
    if (currentState == ST4_DATA || dataEntered) {
      bool showData = (currentState != ST4_DATA) || blinkState;
      if (showData) {
        seg[6] = segmentMap[(targetData >> 4) & 0x0F];
        seg[7] = segmentMap[targetData & 0x0F];
      }
    }
  }

  writeSegments(seg);
}

void commitToEEPROM() {
  if (targetAddr >= EEPROM_SIZE) {
    Serial.print(F("ERROR: Address "));
    Serial.print(targetAddr);
    Serial.println(F(" is out of range (0-1023)"));
    return;
  }

  // กระพริบยืนยันก่อนบันทึก
  uint8_t seg[8] = {0};
  seg[0] = segmentMap[(targetAddr / 1000) % 10];
  seg[1] = segmentMap[(targetAddr / 100) % 10];
  seg[2] = segmentMap[(targetAddr / 10) % 10];
  seg[3] = segmentMap[targetAddr % 10];
  seg[6] = segmentMap[(targetData >> 4) & 0x0F];
  seg[7] = segmentMap[targetData & 0x0F];
  
  writeSegments(seg);
  delay(300);
  clearTM1638Display();
  delay(300);

  // บันทึกลง EEPROM
  EEPROM_Erase_and_Write1B(targetAddr, targetData);

  Serial.print(F("Written 0x"));
  if (targetData < 0x10) Serial.print('0');
  Serial.print(targetData, HEX);
  Serial.print(F(" to EEPROM location ["));
  Serial.print(targetAddr);
  Serial.println(F("]"));

  dataEntered = false;
  currentState = ST1_IDLE;
}

// ==========================================
// 5. การจัดการปุ่มกด (TM1638 & Keypad)
// ==========================================
void handleTM1638Buttons() {
  uint8_t currentButtons = readTM1638Buttons();
  uint8_t pressed = currentButtons & (~lastButtonMask); // ตรวจจับขอบขาขึ้น

  // ปุ่ม S8: สลับโหมดป้อนค่า (Address <-> Data)
  if (pressed & 0x80) {
    if (currentState == ST1_IDLE) {
      targetAddr = 0;
      targetData = 0;
      dataEntered = false;
      currentState = ST3_ADDR;
      Serial.println(F("ST3: Enter EEPROM Address (Decimal)"));
    } else if (currentState == ST3_ADDR) {
      currentState = ST4_DATA;
      Serial.println(F("ST4: Enter Data (Hexadecimal)"));
    } else {
      currentState = ST3_ADDR;
      Serial.println(F("ST3: Back to Address entry"));
    }
    blinkState = true;
    blinkTimer = millis();
  }

  // ปุ่ม S1: สั่ง Dump ข้อมูล (ใน ST1) หรือสั่งบันทึกข้อมูล (ในโหมดอื่น)
  if (pressed & 0x01) {
    if (currentState == ST1_IDLE) {
      display_all_data_in_EEPROM();
    } else {
      commitToEEPROM();
    }
  }

  lastButtonMask = currentButtons;
}

void handleKeypad() {
  char key = keypad.getKey();
  if (key == NO_KEY || currentState == ST1_IDLE) return;

  if (currentState == ST3_ADDR) {
    // ป้อน Address (เฉพาะตัวเลข 0-9)
    if (key >= '0' && key <= '9') {
      targetAddr = (targetAddr * 10 + (key - '0')) % 10000;
    }
  } 
  else if (currentState == ST4_DATA) {
    // ป้อน Data (ฐาน 16: 0-9, A-D, *=E, #=F)
    uint8_t hexValue = 0xFF;

    if (key >= '0' && key <= '9')      hexValue = key - '0';
    else if (key >= 'A' && key <= 'D') hexValue = key - 'A' + 10;
    else if (key == '*')               hexValue = 0x0E; // * แทน E
    else if (key == '#')               hexValue = 0x0F; // # แทน F

    if (hexValue != 0xFF) {
      targetData = ((targetData << 4) | hexValue) & 0xFF;
      dataEntered = true;
    }
  }

  // รีเซ็ตจังหวะกระพริบเมื่อมีการกดปุ่ม
  blinkState = true;
  blinkTimer = millis();
}

// ==========================================
// 6. setup() และ loop()
// ==========================================
void setup() {
  pinMode(STB_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DIO_PIN, OUTPUT);

  sendTM1638Cmd(0x8F); // เปิดหน้าจอ ความสว่างสูงสุด
  clearTM1638Display();

  Wire.begin();
  keypad.begin();
  Serial.begin(38400);

  Serial.println(F("\n===== LAB17 Checkpoint 2 : EEPROM Editor ====="));
  Serial.println(F("S8 : Switch between Address and Data mode"));
  Serial.println(F("S1 : Write to EEPROM (in edit mode) / Dump EEPROM (in idle)"));

  blinkTimer = millis();
}

void loop() {
  handleTM1638Buttons();
  handleKeypad();

  // สลับสถานะกระพริบตามคาบเวลา BLINK_MS
  if (millis() - blinkTimer >= BLINK_MS) {
    blinkTimer = millis();
    blinkState = !blinkState;
  }

  updateDisplay();
}
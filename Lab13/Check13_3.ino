//13.3
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <Wire.h>

#define I2CADDR 0x20

const byte ROWS = 4;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
 //I2C expander ขาไหนต่ออะไร
byte rowPins[ROWS] = {7, 6, 5, 4};
byte colPins[COLS] = {3, 2, 1, 0};

Keypad_I2C keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR);

void I2C_bus_scan(void);

// ---------- ตัวแปรสถานะเครื่องคิดเลข ----------
int num1 = 0;        // เลขตัวที่ 1
int num2 = 0;        // เลขตัวที่ 2
char op = 0;          // ตัวดำเนินการ ('*' หรือ 'D'), 0 = ยังไม่กด
uint8_t phase = 0;    // 0 = กำลังป้อนเลขตัวที่ 1, 1 = กำลังป้อนเลขตัวที่ 2
uint8_t digitCount = 0; // จำนวนหลักที่ป้อนไปแล้วในเลขตัวปัจจุบัน (จำกัดไม่เกิน 2)

void resetCalc() {
    num1 = 0;
    num2 = 0;
    op = 0;
    phase = 0;
    digitCount = 0;
}

void setup() {
    Wire.begin();
    keypad.begin();
    Serial.begin(38400);
    I2C_bus_scan();
    Serial.println("พร้อมใช้งาน: ป้อนเลข 2 หลัก, กด * หรือ D, ป้อนเลข 2 หลัก, กด # เพื่อคำนวณ");
}

void loop() {
    char key = keypad.getKey();
    if (key == NO_KEY) return;

    // ---------- แสดงปุ่มที่กดทันทีทุกครั้ง ----------
    Serial.print(key);

    if (key >= '0' && key <= '9') {
        // ---------- ป้อนตัวเลข ----------
        int d = key - '0';
        if (digitCount < 2) { // จำกัดรับได้สูงสุด 2 หลัก
            if (phase == 0) {
                num1 = num1 * 10 + d;
            } else {
                num2 = num2 * 10 + d;
            }
            digitCount++;
        }
        // ถ้าป้อนเกิน 2 หลักแล้ว จะไม่รับเพิ่ม (ละเลขที่เกินทิ้ง)
    }
    else if (key == '*' || key == 'D') {
        // ---------- เลือกตัวดำเนินการ (คูณ/หาร) ----------
        if (phase == 0 && op == 0) { // เลือกได้แค่ครั้งเดียว หลังป้อนเลขตัวแรก
            op = key;
            phase = 1;       // เปลี่ยนไปรับเลขตัวที่ 2
            digitCount = 0;  // รีเซ็ตตัวนับหลักสำหรับเลขตัวที่ 2
        }
    }
    else if (key == '#') {
        // ---------- คำนวณและแสดงผล ----------
        Serial.println(); // ขึ้นบรรทัดใหม่ก่อนแสดงผลลัพธ์
        if (op == '*') {
            long result = (long)num1 * num2;
            Serial.print(num1);
            Serial.print("*");
            Serial.print(num2);
            Serial.print("=");
            Serial.println(result);
        }
        else if (op == 'D') {
            Serial.print(num1);
            Serial.print("/");
            Serial.print(num2);
            Serial.print("=");
            if (num2 == 0) {
                Serial.println("หารด้วย 0 ไม่ได้");
            } else {
                int quotient = num1 / num2;
                int remainder = num1 % num2;
                Serial.print(quotient);
                Serial.print(" (ผลหารเท่ากับ ");
                Serial.print(quotient);
                Serial.print(" เศษจากการหารเท่ากับ ");
                Serial.print(remainder);
                Serial.println(")");
            }
        }
        // ถ้ากด # โดยยังไม่ได้เลือกตัวดำเนินการ ไม่ทำอะไร (ป้องกันค่าผิดพลาด)

        resetCalc(); // เคลียร์ค่าทั้งหมด เตรียมรับการคำนวณครั้งใหม่
    }
    // ปุ่ม A, B, C ไม่ได้ใช้งานในโจทย์นี้ จึงไม่ต้องจัดการ
}

void I2C_bus_scan(void) {
    Serial.println();
    Serial.println("www.9arduino.com ...");
    Serial.println("I2C scanner. Scanning ...");
    byte count = 0;
    Wire.begin();
    for (byte i = 8; i < 120; i++) {
        Wire.beginTransmission(i);
        if (Wire.endTransmission() == 0) {
            Serial.print("Found address: ");
            Serial.print(i, DEC);
            Serial.print(" (0x");
            Serial.print(i, HEX);
            Serial.println(")");
            count++;
            delay(1);
        }
    }
    Serial.println("Done.");
    Serial.print("Found ");
    Serial.print(count, DEC);
    Serial.println(" device(s).");
}
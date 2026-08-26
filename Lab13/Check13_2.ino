//13.2 
#include <avr/io.h>
#include <avr/interrupt.h>
#include "my_twi.h"
#include "my_pcf8574.h"

uint8_t mode = 0; // 0: X1, 1: X2, 2: X3, 3: X4

// ---------- ตัวแปรแต่ละโหมด ----------
int count_x1 = 127;          // X1: นับถอยหลังไบนารี
uint8_t x2_step = 0;         // X2: จำนวนดวงที่ติด (0-7)
uint8_t x3_pos = 0;          // X3: ตำแหน่งไฟ 2 ดวง (0-7)

// X4 แบบ non-blocking
uint8_t x4_cycle = 1;        // บอกว่ารอบนี้ต้องกระพริบกี่ครั้ง
uint8_t x4_blink_count = 0;  // นับว่ากระพริบไปแล้วกี่ครั้งในรอบปัจจุบัน
uint8_t x4_state = 0;        // บอกว่าตอนนี้อยู่ขั้นไหน 0=ดับรอ1วิ, 1=ไฟติด, 2=ไฟดับ(ระหว่างกระพริบ)
unsigned long x4_timer = 0;

unsigned long timer_x = 0;   // ตัวจับเวลาใช้ร่วมกันสำหรับ X1, X2

// ---------- ตัวแปรสำหรับตรวจจับปุ่ม (กดสั้น/กดค้าง) ----------
const unsigned long DEBOUNCE_MS   = 50;   // กันปุ่มเด้ง
const unsigned long LONG_PRESS_MS = 700;  // ถือว่า "กดค้าง" ถ้ากดเกินเวลานี้

bool button_active = false;      // true = กำลังกดปุ่มอยู่ (ผ่าน debounce แล้ว)
bool long_press_fired = false;   // กันไม่ให้ยิง event ซ้ำระหว่างกดค้าง
unsigned long press_start = 0;
unsigned long last_debounce = 0;

uint8_t prepare_data(uint8_t d) {
    uint8_t tmp = ~d;
    tmp |= 0x80;
    return tmp;
}

void print_mode() {
    Serial.print("Current Mode: X");
    Serial.println(mode + 1);
}

// return 0 = ไม่มีอะไรเกิดขึ้น
// return 1 = กดสั้น (short press) -> เพิ่งปล่อยปุ่มก่อนครบเวลากดค้าง
// return 2 = กดค้าง (long press)  -> ถือปุ่มไว้จนครบเวลาที่กำหนด (ยิงตอนยังกดอยู่)
uint8_t check_button() {
    uint8_t raw = PCF8574_read(0) & 0x80; // 0 = กำลังกด (active low)

    if (!raw) { 
        if (!button_active) {
            if (millis() - last_debounce > DEBOUNCE_MS) {
                last_debounce = millis();
                if (!(PCF8574_read(0) & 0x80)) { // ยืนยันว่ากดจริง
                    button_active = true;
                    long_press_fired = false;
                    press_start = millis();
                }
            }
        } else {
            // กำลังถือปุ่มไว้ -> เช็คว่าครบเวลากดค้างหรือยัง
            if (!long_press_fired && millis() - press_start >= LONG_PRESS_MS) {
                long_press_fired = true;
                return 2; // ยิง event กดค้างทันทีตอนถือครบเวลา
            }
        }
    } else { // ---------- ปล่อยปุ่มแล้ว ----------
        if (button_active) {
            button_active = false;
            if (!long_press_fired) {
                return 1; // ปล่อยก่อนครบเวลากดค้าง = กดสั้น
            }
        }
    }
    return 0;
}

void reset_mode_vars() {
    count_x1 = 127;
    x2_step = 0;
    x3_pos = 0;
    x4_cycle = 1;
    x4_blink_count = 0;
    x4_state = 0;
    timer_x = millis();
    x4_timer = millis();
}

void setup() {
    Serial.begin(38400);
    init_twi_module();
    PCF8574_write(0, prepare_data(0));
    print_mode(); // แสดงโหมดเริ่มต้น (X1) ตอนเปิดเครื่อง
}

void loop() {
    uint8_t btn = check_button(); // เช็คปุ่มครั้งเดียวต่อรอบ loop

    // กดค้าง -> เปลี่ยนโหมด (ทำงานได้ทุกโหมด)
    if (btn == 2) {
        mode = (mode + 1) % 4;
        reset_mode_vars();
        print_mode();
        return; // ข้ามการทำงานของโหมดในรอบนี้ไปเลย
    }

    switch (mode) {

        case 0: // X1: นับถอยหลังไบนารี 127 -> 0 แล้ววนกลับ 127
            if (millis() - timer_x >= 1000) {
                timer_x = millis();
                PCF8574_write(0, prepare_data((uint8_t)count_x1));
                if (count_x1 == 0) {
                    count_x1 = 127; // ถึง 0 แล้ว วกกลับ 127 ใหม่
                } else {
                    count_x1--;
                }
            }
            break;

        case 1: // X2: ดับหมดก่อน แล้วติดเพิ่มทีละดวงจากซ้าย ทุก 1 วิ จนครบ 7 ดวง แล้วดับหมดวนใหม่
            if (millis() - timer_x >= 1000) {
                timer_x = millis();
                if (x2_step == 0) {
                    PCF8574_write(0, prepare_data(0x00)); // ดับหมด
                } else {
                    uint8_t mask = 0;
                    for (int i = 0; i < x2_step; i++) mask |= (1 << (6 - i));
                    PCF8574_write(0, prepare_data(mask));
                }
                x2_step = (x2_step + 1) % 8; // 0..7 (0=ดับ, 7=ติดครบทุกดวง) แล้ววนใหม่
            }
            break;

        case 2: { // X3: สว่าง 2 ดวงจากขวาสุด เลื่อนซ้ายทุกครั้งที่ "กดสั้น"
            uint8_t mask = (0x03 << x3_pos) & 0x7F;
            PCF8574_write(0, prepare_data(mask)); // แสดงตำแหน่งปัจจุบันค้างไว้

            if (btn == 1) { // กดสั้น -> เลื่อนตำแหน่งไปทางซ้าย
                x3_pos = (x3_pos + 1) % 8; // 0..6 = มีไฟ, 7 = ดับหมด แล้ววนกลับ 0 (ขวาสุด)
            }
            break;
        }

        case 3: // X4: กระพริบ 1,2,3,4 ครั้ง สลับดับ 1 วิ คั่นแต่ละรอบ (non-blocking)
            switch (x4_state) {
                case 0: // ดับไฟ รอ 1 วินาที ก่อนเริ่มรอบใหม่
                    PCF8574_write(0, prepare_data(0x00));
                    if (millis() - x4_timer >= 1000) {
                        x4_timer = millis();
                        x4_blink_count = 0;
                        x4_state = 1;
                    }
                    break;

                case 1: // ไฟติด 0.5 วิ
                    PCF8574_write(0, prepare_data(0x7F));
                    if (millis() - x4_timer >= 500) {
                        x4_timer = millis();
                        x4_state = 2;
                    }
                    break;

                case 2: // ไฟดับ 0.5 วิ (ระหว่างกระพริบ)
                    PCF8574_write(0, prepare_data(0x00));
                    if (millis() - x4_timer >= 500) {
                        x4_timer = millis();
                        x4_blink_count++;
                        if (x4_blink_count >= x4_cycle) {
                            x4_cycle++;
                            if (x4_cycle > 4) x4_cycle = 1; // วนกลับ 1-4 ใหม่
                            x4_state = 0; // ไปดับรอ 1 วิ ก่อนรอบถัดไป
                        } else {
                            x4_state = 1; // กระพริบต่อในรอบเดิม
                        }
                    }
                    break;
            }
            break;
    }
}
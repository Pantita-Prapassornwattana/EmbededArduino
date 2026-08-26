//ท้ายการทดลอง
#include "my_twi.h"

// ---------- ฟังก์ชันตรวจสอบว่ามีอุปกรณ์ตอบ ACK ที่แอดเดรสนี้หรือไม่ ----------
bool check_device_ack(uint8_t addr_7bit) {
    uint8_t sla_w = (addr_7bit << 1); // แปลง 7-bit address เป็น SLA+W (Write bit = 0)

    // ---------- ส่งสัญญาณ START ----------
    TWI_send_start_condition();
    TWI_wait_until_start_has_been_sent();

    // ---------- ส่ง SLA+W ----------
    TWI_send_slave_address(sla_w);
    TWI_wait_until_sla_transmitted();

    // ---------- ตรวจสอบสถานะที่ได้รับกลับมา ----------
    uint8_t status = TWSR & 0xF8;
    bool ack = (status == TW_MT_SLA_ACK); // TW_MT_SLA_ACK มาจาก util/twi.h (ผ่าน my_twi.h)

    // ---------- ส่งสัญญาณ STOP แบบดิบ ----------
    // (ไม่ใช้ TWI_send_stop_condition() เพราะฟังก์ชันนั้นเช็คว่าต้องส่ง DATA
    //  สำเร็จมาก่อน ถ้าเรียกตรงนี้จะพิมพ์ error "no data acknowledge" ทุกครั้ง
    //  ที่ไม่มีอุปกรณ์ตอบ ทั้งที่จริงๆ ไม่ใช่ error ของการสแกนแอดเดรส)
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
    while (TWCR & (1 << TWSTO))
        ; // รอจนกว่า STOP condition จะถูกส่งเสร็จ

    return ack;
}

void setup() {
    Serial.begin(38400);
    init_twi_module();   // เปิดใช้งานมอดูล TWI/I2C (ตั้งค่า TWBR)

    delay(1000);
    Serial.println("=========================================");
    Serial.println("   I2C Scanner: PCF8574 & PCF8574A");
    Serial.println("=========================================");

    int found_count = 0;

    // ---------- ตรวจสอบ PCF8574 (แอดเดรส 0x20 - 0x27) ----------
    Serial.println("\nScanning for PCF8574 (0x20 - 0x27)...");
    for (uint8_t addr = 0x20; addr <= 0x27; addr++) {
        if (check_device_ack(addr)) {
            Serial.print(" -> Found PCF8574 at Address: 0x");
            if (addr < 0x10) Serial.print("0");
            Serial.println(addr, HEX);
            found_count++;
        }
    }

    // ---------- ตรวจสอบ PCF8574A (แอดเดรส 0x38 - 0x3F) ----------
    Serial.println("\nScanning for PCF8574A (0x38 - 0x3F)...");
    for (uint8_t addr = 0x38; addr <= 0x3F; addr++) {
        if (check_device_ack(addr)) {
            Serial.print(" -> Found PCF8574A at Address: 0x");
            if (addr < 0x10) Serial.print("0");
            Serial.println(addr, HEX);
            found_count++;
        }
    }

    // ---------- สรุปผล ----------
    Serial.println("\n-----------------------------------------");
    if (found_count == 0) {
        Serial.println("No PCF8574 or PCF8574A devices found on I2C bus.");
    } else {
        Serial.print("Scan finished. Total devices found: ");
        Serial.println(found_count);
    }
    Serial.println("=========================================");
}

void loop() {
    // ไม่ต้องทำงานซ้ำใน loop
}

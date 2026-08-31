//bonus
// ===== LAB18 Checkpoint 1 : ควบคุม LED 3 ดวงตามรหัสดิปสวิตช์ 4 บิต (ตารางที่ 1) =====
// โจทย์ (หน้า 7-8, ข้อ 6): อ่านดิปสวิตช์บิต 5-8 ซึ่งต่อกับ GPIO12, GPIO13, GPIO5, GPIO16
// รวมเป็นรหัสตรรกะ 4 บิต เรียงเป็น GPIO16,GPIO5,GPIO13,GPIO12 (ตามหัวตารางที่ 1) แล้ว
// สั่งควบคุมการติดดับของ LED สีแดง เหลือง เขียว ตามตาราง — เป็นรหัสสวิตช์ชุดเดียวกับ
// ที่ใช้ใน Checkpoint 3 (lab18_check3) แต่ Checkpoint นี้ยังไม่มีการหรี่ไฟด้วย PWM
//
// ตารางที่ 1 (16,5,13,12 -> หน้าที่การทำงาน):
//   1 X X X -> แดงกระพริบทุก 1 วินาที (เหลือง/เขียวดับ, X = don't care)
//   0 0 0 0 -> ดับทั้งหมด
//   0 0 0 1 -> แดงติดพร้อมเหลือง, เขียวดับ
//   0 0 1 0 -> แดงติดพร้อมเขียว, เหลืองดับ
//   0 0 1 1 -> เหลืองติดพร้อมเขียว, แดงดับ
//   0 1 0 0 -> ทั้งสามดวงกระพริบพร้อมกันทุก 1 วินาที
//   (รหัสอื่นนอกเหนือจากนี้ไม่ได้ระบุไว้ในตาราง -> ปิดไฟทั้งหมดไว้ก่อน)

#define R_LED 15   // LED สีแดง
#define Y_LED 0    // LED สีเหลือง
#define G_LED 2    // LED สีเขียว

// ดิปสวิตช์บิต 5-8 (รูปที่ 3): แต่ละบิตมีตัวต้านทานพูลอัพ/พูลดาวน์ภายนอกอยู่แล้ว
#define SW_B5 12   // บิต LSB ของรหัส
#define SW_B6 13
#define SW_B7 5
#define SW_B8 16   // บิต MSB ของรหัส

// อ่านดิปสวิตช์บิต 5-8 รวมเป็นรหัส 4 บิต เรียง GPIO16,GPIO5,GPIO13,GPIO12 ตามตารางที่ 1
uint8_t readCode()
{
  uint8_t b8 = digitalRead(SW_B8);
  uint8_t b7 = digitalRead(SW_B7);
  uint8_t b6 = digitalRead(SW_B6);
  uint8_t b5 = digitalRead(SW_B5);
  return (b8 << 3) | (b7 << 2) | (b6 << 1) | b5;
}

void setLEDs(bool r, bool y, bool g)
{
  digitalWrite(R_LED, r ? HIGH : LOW);
  digitalWrite(Y_LED, y ? HIGH : LOW);
  digitalWrite(G_LED, g ? HIGH : LOW);
}

void setup()
{
  pinMode(R_LED, OUTPUT);
  pinMode(Y_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);
  pinMode(SW_B5, INPUT);
  pinMode(SW_B6, INPUT);
  pinMode(SW_B7, INPUT);
  pinMode(SW_B8, INPUT);
}

void loop()
{
  static bool blinkPhase = false;          // สลับทุก 500 ms -> ครบคาบ 1 วินาที
  static unsigned long lastToggle = 0;
  unsigned long now = millis();
  if (now - lastToggle >= 1000) {
    lastToggle = now;a
    blinkPhase = !blinkPhase;
  }

  uint8_t code = readCode();

  if (code & 0b1000) {              // แถว "1 X X X" : บิต16=1 บิตอื่น don't care
    setLEDs(blinkPhase, false, false);
    return;
  }

  switch (code) {
    case 0b0000: setLEDs(false, false, false); break;                 // ดับทั้งหมด
    case 0b0001: setLEDs(true,  true,  false); break;                 // แดง+เหลือง
    case 0b0010: setLEDs(true,  false, true);  break;                 // แดง+เขียว
    case 0b0011: setLEDs(false, true,  true);  break;                 // เหลือง+เขียว
    case 0b0100: setLEDs(blinkPhase, blinkPhase, blinkPhase); break;  // กระพริบพร้อมกัน
    default:     setLEDs(false, false, false); break;                 // รหัสนอกตาราง
  }
}
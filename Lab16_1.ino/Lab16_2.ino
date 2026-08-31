//16.2
#include <Arduino_FreeRTOS.h>

#define R_LED 3
#define G_LED 4
#define Y_LED 5
#define O_LED 6
#define PUSH_SW 2
#define VR_PIN A0

// ตัวแปรส่วนกลางสำหรับแชร์ระหว่าง Tasks
volatile int current_mode = 1;      // โหมด 1 - 4
volatile int step_delay_ms = 500;  // ค่าความเร็วในการเปลี่ยนสเต็ป (ms)

// ฟังก์ชันช่วยเปิด/ปิด LED ตามบิตแมสค์ (R, G, Y, O)
void setLEDs(bool r, bool g, bool y, bool o) {
  digitalWrite(R_LED, r ? HIGH : LOW);
  digitalWrite(G_LED, g ? HIGH : LOW);
  digitalWrite(Y_LED, y ? HIGH : LOW);
  digitalWrite(O_LED, o ? HIGH : LOW);
}

// Task สลับไฟ LED ตามโหมดปัจจุบัน
void Task_LEDControl(void *pvParameters) {
  pinMode(R_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);
  pinMode(Y_LED, OUTPUT);
  pinMode(O_LED, OUTPUT);

  int step = 0;

  while (1) {
    int mode = current_mode; // ดึงค่าโหมดปัจจุบัน

    switch (mode) {
      case 1: // Mode 1: R -> G -> Y -> O -> E
        switch (step % 5) {
          case 0: setLEDs(1, 0, 0, 0); break; // R
          case 1: setLEDs(0, 1, 0, 0); break; // G
          case 2: setLEDs(0, 0, 1, 0); break; // Y
          case 3: setLEDs(0, 0, 0, 1); break; // O
          case 4: setLEDs(0, 0, 0, 0); break; // E
        }
        step = (step + 1) % 5;
        break;

      case 2: // Mode 2: O -> Y -> G -> R -> E
        switch (step % 5) {
          case 0: setLEDs(0, 0, 0, 1); break; // O
          case 1: setLEDs(0, 0, 1, 0); break; // Y
          case 2: setLEDs(0, 1, 0, 0); break; // G
          case 3: setLEDs(1, 0, 0, 0); break; // R
          case 4: setLEDs(0, 0, 0, 0); break; // E
        }
        step = (step + 1) % 5;
        break;

      case 3: // Mode 3: E -> R -> RG -> RGY -> RGYO -> RGY -> RG -> R
        switch (step % 8) {
          case 0: setLEDs(0, 0, 0, 0); break; // E
          case 1: setLEDs(1, 0, 0, 0); break; // R
          case 2: setLEDs(1, 1, 0, 0); break; // RG
          case 3: setLEDs(1, 1, 1, 0); break; // RGY
          case 4: setLEDs(1, 1, 1, 1); break; // RGYO
          case 5: setLEDs(1, 1, 1, 0); break; // RGY
          case 6: setLEDs(1, 1, 0, 0); break; // RG
          case 7: setLEDs(1, 0, 0, 0); break; // R
        }
        step = (step + 1) % 8;
        break;

      case 4: // Mode 4: E -> O -> OY -> OYG -> OYGR -> OYG -> OY -> O
        switch (step % 8) {
          case 0: setLEDs(0, 0, 0, 0); break; // E
          case 1: setLEDs(0, 0, 0, 1); break; // O
          case 2: setLEDs(0, 0, 1, 1); break; // OY
          case 3: setLEDs(0, 1, 1, 1); break; // OYG
          case 4: setLEDs(1, 1, 1, 1); break; // OYGR
          case 5: setLEDs(0, 1, 1, 1); break; // OYG
          case 6: setLEDs(0, 0, 1, 1); break; // OY
          case 7: setLEDs(0, 0, 0, 1); break; // O
        }
        step = (step + 1) % 8;
        break;
    }

    // หน่วงเวลาตามค่าที่อ่านได้จาก VR1
    vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
  }
}

// Task อ่านสวิตช์ SW1 เพื่อเปลี่ยนโหมด
void Task_ReadSwitch(void *pvParameters) {
  pinMode(PUSH_SW, INPUT);

  while (1) {
    if (digitalRead(PUSH_SW) == LOW) {
      vTaskDelay(pdMS_TO_TICKS(15)); // Debounce
      if (digitalRead(PUSH_SW) == LOW) {
        while (digitalRead(PUSH_SW) == LOW) { // รอปล่อยสวิตช์
          vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // เปลี่ยน Mode 1 -> 2 -> 3 -> 4 -> 1
        current_mode++;
        if (current_mode > 4) {
          current_mode = 1;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Task อ่านค่า Potentiometer (VR1) และแบ่งเป็น 8 สเต็ปความเร็ว
void Task_ReadVR(void *pvParameters) {
  while (1) {
    int analog_val = analogRead(VR_PIN); // อ่านช่วง 0 - 1023
    
    // แปลงช่วง 0-1023 เป็น 8 สเต็ป (0 ถึง 7)
    int step_level = analog_val / 128; // ได้ค่า 0, 1, 2, 3, 4, 5, 6, 7
    if (step_level > 7) step_level = 7;

    // คำนวณ Delay ช่วง 1500 ms (หมุนทวนเข็ม) ถึง 50 ms (หมุนตามเข็ม)
    // สเต็ป: 0=1500ms, 1=1293ms, 2=1086ms, ..., 7=50ms
    step_delay_ms = 1500 - (step_level * (1500 - 50) / 7);

    vTaskDelay(pdMS_TO_TICKS(100)); // อัปเดตทุก 100 ms
  }
}

void setup() {
  xTaskCreate(Task_LEDControl, "LED Control", 128, NULL, 1, NULL);
  xTaskCreate(Task_ReadSwitch, "Read Switch", 128, NULL, 1, NULL);
  xTaskCreate(Task_ReadVR,     "Read VR",     128, NULL, 1, NULL);

  vTaskStartScheduler();
}

void loop() {}
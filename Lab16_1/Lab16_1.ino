//16.1
#include <Arduino_FreeRTOS.h>

#define R_LED 3
#define G_LED 4
#define Y_LED 5
#define PUSH_SW 2

int display = 0; 
// display = 1: สว่างเฉพาะ Red LED
// display = 2: สว่างเฉพาะ Green LED
// display = 3: สว่างเฉพาะ Yellow LED
// display = 0: ดับทั้งหมด

void setup() {
  xTaskCreate(Read_Switch, "Read push button switch", 128, NULL, 1, NULL);
  xTaskCreate(Display_R_LED, "Red LED Task", 128, NULL, 1, NULL);
  xTaskCreate(Display_G_LED, "Green LED Task", 128, NULL, 1, NULL);
  xTaskCreate(Display_Y_LED, "Yellow LED Task", 128, NULL, 1, NULL);
  
  vTaskStartScheduler();
}

void Display_R_LED(void *pvParameters) {
  pinMode(R_LED, OUTPUT);
  while (1) {
    if (display == 1)
      digitalWrite(R_LED, HIGH);
    else
      digitalWrite(R_LED, LOW);
    vTaskDelay(pdMS_TO_TICKS(10)); // เพิ่ม delay เล็กน้อยเพื่อคืน CPU time
  }
}

void Display_G_LED(void *pvParameters) {
  pinMode(G_LED, OUTPUT);
  while (1) {
    if (display == 2)
      digitalWrite(G_LED, HIGH);
    else
      digitalWrite(G_LED, LOW);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Display_Y_LED(void *pvParameters) {
  pinMode(Y_LED, OUTPUT);
  while (1) {
    if (display == 3)
      digitalWrite(Y_LED, HIGH);
    else
      digitalWrite(Y_LED, LOW);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Read_Switch(void *pvParameters) {
  pinMode(PUSH_SW, INPUT);
  while (1) {
    int sw_status = digitalRead(PUSH_SW);
    if (sw_status == LOW) {
      vTaskDelay(pdMS_TO_TICKS(10)); // Debounce
      sw_status = digitalRead(PUSH_SW);
      if (sw_status == LOW) {
        while (sw_status == LOW) { // รอจนกว่าจะปล่อยปุ่ม
          sw_status = digitalRead(PUSH_SW);
          vTaskDelay(pdMS_TO_TICKS(10));
        }
        display++;
        if (display > 3)
          display = 0;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void loop() {}
//Checkpoint 12.1
int soundPin = 2;
int relayPin = 4;
unsigned long previous_time = 0;
int state = 0;   // 0 = หลอดดับ, 1 = หลอดติด

bool check_clap() {
  int sw = digitalRead(soundPin);
  if (sw) {
    return false;               // เงียบ -> ไม่ใช่การปรบมือ
  } else {
    unsigned long current_time = millis();
    if ((current_time - previous_time) > 25) {
      previous_time = millis();
      return true;               // ปรบมือจริง (พ้นช่วง debounce)
    } else {
      previous_time = millis();
      return false;
    }
  }
}

void setup() {
  pinMode(relayPin, OUTPUT);
  pinMode(soundPin, INPUT);
  Serial.begin(9600);
  state = 0;
  digitalWrite(relayPin, LOW);
  Serial.println("Lamp is OFF");
}

void loop() {
  bool val = check_clap();
  if (val) {
    state = !state;              // สลับสถานะทุกครั้งที่ปรบมือ 1 ครั้ง
    digitalWrite(relayPin, state ? HIGH : LOW);
    Serial.println(state ? "Lamp is turned ON" : "Lamp is turned OFF");
  }
}
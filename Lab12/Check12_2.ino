//Checkpoint 12.2
int soundPin = 2;
int relayPin = 4;
unsigned long previous_time = 0;

int lampState = 0;
const int MAX_CLAPS = 5;
unsigned long clapTimes[MAX_CLAPS];
int clapIndex = 0;

const unsigned long RAPID_GAP     = 350;  // ช่องว่างสูงสุดที่ยังนับว่า "รัว"
const unsigned long WINDOW_TIMEOUT = 800; // เงียบเกินนี้ถือว่าจบชุดปรบมือ

bool check_clap() {
  int sw = digitalRead(soundPin);
  if (sw) {
    return false;
  } else {
    unsigned long current_time = millis();
    if ((current_time - previous_time) > 25) {
      previous_time = millis();
      return true;
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
  digitalWrite(relayPin, LOW);
  Serial.println("Lamp is OFF");
}

void loop() {
  bool val = check_clap();
  if (val && clapIndex < MAX_CLAPS) {
    clapTimes[clapIndex] = millis();
    clapIndex++;
    Serial.print("Clap #");
    Serial.println(clapIndex);
  }

  if (clapIndex > 0 && (millis() - clapTimes[clapIndex - 1] > WINDOW_TIMEOUT)) {
    bool patternMatch = false;

    if (clapIndex == 3) {
      unsigned long gap1 = clapTimes[1] - clapTimes[0];   // ระหว่างครั้ง 1-2 (ต้องรัว)
      unsigned long gap2 = clapTimes[2] - clapTimes[1];   // ระหว่างครั้ง 2-3 (ต้องเว้นจังหวะ)
      if (gap1 <= RAPID_GAP && gap2 > RAPID_GAP) {
        patternMatch = true;
      }
    }

    if (patternMatch) {
      lampState = !lampState;
      digitalWrite(relayPin, lampState ? HIGH : LOW);
      Serial.println(lampState ? "Lamp is turned ON" : "Lamp is turned OFF");
    } else {
      Serial.println("Pattern not matched, no action");
    }
    clapIndex = 0;   // เคลียร์เพื่อเริ่มจับชุดใหม่
  }
}
//Checkpoint 12.2 (Interrupt Version)
int soundPin = 2;
int relayPin = 4;
volatile unsigned long previous_time = 0;

int lampState = 0;
const int MAX_CLAPS = 5;
volatile unsigned long clapTimes[MAX_CLAPS];
volatile int clapIndex = 0;

const unsigned long RAPID_GAP     = 350;  // ช่องว่างสูงสุดที่ยังนับว่า "รัว"
const unsigned long WINDOW_TIMEOUT = 800; // เงียบเกินนี้ถือว่าจบชุดปรบมือ

void check_clap() {
  unsigned long current_time = millis();
  if ((current_time - previous_time) > 25) {
    if (clapIndex < MAX_CLAPS) {
      clapTimes[clapIndex] = current_time;
      clapIndex++;
      Serial.print("Clap #");
      Serial.println(clapIndex);
    }
    previous_time = current_time;
  }
}

void setup() {
  pinMode(relayPin, OUTPUT);
  pinMode(soundPin, INPUT);
  Serial.begin(9600);
  digitalWrite(relayPin, LOW);
  Serial.println("Lamp is OFF");
  
  attachInterrupt(digitalPinToInterrupt(soundPin), check_clap, FALLING);//เเทนdigitalRead(soundPin)
}

void loop() {
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
      if (lampState) {
        digitalWrite(relayPin, HIGH);
      } else {
        digitalWrite(relayPin, LOW);
      }
      Serial.println(lampState ? "Lamp is turned ON" : "Lamp is turned OFF");
    } else {
      Serial.println("Pattern not matched, no action");
    }
    clapIndex = 0;   // เคลียร์เพื่อเริ่มจับชุดใหม่
  }
}
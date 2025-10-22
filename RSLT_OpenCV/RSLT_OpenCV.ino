#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);

// --- tune these for your servos / horn geometry ---
const int CHANNELS[5] = {0,1,2,3,4};   // thumb,index,middle,ring,pinky
const int ANG_MIN[5]  = {15,10,10,10,10};
const int ANG_MAX[5]  = {165,170,170,170,170};

// typical 50 Hz servo
const float SERVO_FREQ = 50.0f;          // Hz
const int   PWM_RES    = 4096;           // PCA9685 12-bit
// pulse in microseconds for 0°..180° (adjust for your servo)
const int   PULSE_MIN_US = 500;          // 0°
const int   PULSE_MAX_US = 2500;         // 180°

int angleToTicks(int angleDeg) {
  angleDeg = constrain(angleDeg, 0, 180);
  float us = PULSE_MIN_US + (PULSE_MAX_US - PULSE_MIN_US) * (angleDeg / 180.0f);
  // ticks = us / (1e6 / (freq * 4096))
  float ticks = us * SERVO_FREQ * PWM_RES / 1000000.0f;
  int ti = (int)(ticks + 0.5f);
  if (ti < 0) ti = 0;
  if (ti > 4095) ti = 4095;
  return ti;
}

void setServoDeg(int ch, int deg) {
  int t = angleToTicks(deg);
  pca.setPWM(ch, 0, t);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();                  // defaults to SDA=21, SCL=22 on ESP32
  pca.begin();
  pca.setPWMFreq(SERVO_FREQ);    // 50 Hz

  // move to neutral
  for (int i=0;i<5;i++) setServoDeg(CHANNELS[i], 90);
  Serial.println("Ready. Waiting for angles like: 90,80,75,70,60");
}

String line;

bool parseFiveInts(const String& s, int out[5]) {
  // simple CSV parser: expects five integers separated by commas
  int last = 0, idx = 0;
  for (int i=0; i<=s.length(); ++i) {
    if (i == s.length() || s[i] == ',' || s[i] == '\n' || s[i] == '\r') {
      if (i > last) {
        if (idx >= 5) return false;
        out[idx++] = s.substring(last, i).toInt();
      }
      last = i + 1;
    }
  }
  return idx == 5;
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 0) {
        int ang[5];
        if (parseFiveInts(line, ang)) {
          // clamp to per-finger limits
          for (int i=0;i<5;i++) {
            ang[i] = constrain(ang[i], ANG_MIN[i], ANG_MAX[i]);
            setServoDeg(CHANNELS[i], ang[i]);
          }
          // --- PRINT to Serial Monitor ---
          Serial.printf("ESP32 received angles: T=%d I=%d M=%d R=%d P=%d\n",
                        ang[0], ang[1], ang[2], ang[3], ang[4]);
        } else {
          Serial.printf("Parse error: '%s'\n", line.c_str());
        }
      }
      line = ""; // reset buffer
    } else if (c != '\r') {
      line += c;
      // guard against runaway lines
      if (line.length() > 64) line = "";
    }
  }
}

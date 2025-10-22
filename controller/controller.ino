#include <Arduino.h>
#include <BleGamepad.h>

// ---------------- BUTTON SETUP ----------------
#define BUTTON_PIN1 12   // Light Attack
#define BUTTON_PIN2 14   // Heavy Attack
#define BUTTON_PIN3 27   // Block / Shield
#define BUTTON_PIN4 26   // Dodge / Roll
#define NUM_BUTTONS 4

BleGamepad bleGamepad("God of War Gamepad", "Tech Talkies", 100);

// ---------------- JOYSTICK SETUP ----------------
// Use ADC1 pins on ESP32 (stable with BLE): 32, 33, 34, 35, 36, 39
#define VRX_PIN 34
#define VRY_PIN 35

#define INVERT_X true
#define INVERT_Y false

// Tuning
static const int   ADC_MIN        = 0;
static const int   ADC_MAX        = 4095;
static const float DEADZONE_PCT   = 0.06f;   // 6% deadzone
static const float SMOOTH_ALPHA   = 0.35f;   // 0..1 (higher = snappier)
static const uint8_t REPORT_HZ    = 100;     // 100 reports per second

int buttonPins[NUM_BUTTONS] = { BUTTON_PIN1, BUTTON_PIN2, BUTTON_PIN3, BUTTON_PIN4 };
// PS→Xbox mapping from your comment
int buttons[NUM_BUTTONS] = {
  BUTTON_4,  // Light Attack → Square (X on Xbox)
  BUTTON_5,  // Heavy Attack → Triangle (Y on Xbox)
  BUTTON_6,  // Block / Parry → L1 (LB on Xbox)
  BUTTON_2   // Dodge / Roll → Circle (B on Xbox)
};

// Runtime state
int   centerX = 2048, centerY = 2048;
float filtX = 0, filtY = 0;
uint32_t lastReportMs = 0;
const uint32_t reportIntervalMs = 1000 / REPORT_HZ;

static inline int clampi(int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); }

static inline int16_t normToBle(float n){
  float v = (n * 0.5f + 0.5f) * 32767.0f;
  int iv = (int)roundf(v);
  return (int16_t)clampi(iv, 0, 32767);
}

static inline float applyDeadzone(float n, float dz){
  float a = fabsf(n);
  if (a <= dz) return 0.0f;
  float out = (a - dz) / (1.0f - dz);
  return (n < 0 ? -out : out);
}

void calibrateCenter(uint16_t samples=600){
  uint32_t sx=0, sy=0;
  for (uint16_t i=0;i<samples;i++){
    sx += analogRead(VRX_PIN);
    sy += analogRead(VRY_PIN);
    delay(1);
  }
  centerX = sx / samples;
  centerY = sy / samples;

  
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < NUM_BUTTONS; i++) pinMode(buttonPins[i], INPUT_PULLUP);


  bleGamepad.begin();
  delay(200);
  calibrateCenter();

  filtX = centerX;
  filtY = centerY;
}

void loop() {
  // Read analog
  int rx = analogRead(VRX_PIN); // 0..4095
  int ry = analogRead(VRY_PIN); // 0..4095

  // Low-pass filter (1st order IIR)
  filtX = SMOOTH_ALPHA * rx + (1.0f - SMOOTH_ALPHA) * filtX;
  filtY = SMOOTH_ALPHA * ry + (1.0f - SMOOTH_ALPHA) * filtY;

  // Convert to normalized -1..+1 around centers
  float nx = (filtX - centerX) / (float)max(1, max(centerX - ADC_MIN, ADC_MAX - centerX));
  float ny = (filtY - centerY) / (float)max(1, max(centerY - ADC_MIN, ADC_MAX - centerY));

  // Clamp to [-1,1]
  nx = fminf(1.0f, fmaxf(-1.0f, nx));
  ny = fminf(1.0f, fmaxf(-1.0f, ny));

  // Invert if desired
  if (INVERT_X) nx = -nx;
  if (INVERT_Y) ny = -ny;

  // Deadzone
  nx = applyDeadzone(nx, DEADZONE_PCT);
  ny = applyDeadzone(ny, DEADZONE_PCT);

  // Send at fixed rate
  uint32_t now = millis();
  if (bleGamepad.isConnected() && (now - lastReportMs) >= reportIntervalMs) {
    bleGamepad.setLeftThumb(normToBle(nx), normToBle(ny));

    // Buttons
    for (int i = 0; i < NUM_BUTTONS; i++) {
      if (!digitalRead(buttonPins[i])) bleGamepad.press(buttons[i]);
      else bleGamepad.release(buttons[i]);
    }

    lastReportMs = now;
  }

  // Tiny idle to reduce CPU thrash
  delay(1);
}

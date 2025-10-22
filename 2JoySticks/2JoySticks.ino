#include <Arduino.h>
#include <BleGamepad.h>
#include <math.h>

// ---------------- BUTTON SETUP ----------------
#define BUTTON_PIN1 12   // Light Attack
#define BUTTON_PIN2 14   // Heavy Attack
#define BUTTON_PIN3 27   // Block / Shield
#define BUTTON_PIN4 26   // Dodge / Roll
#define NUM_BUTTONS 4

BleGamepad bleGamepad("God of War Gamepad", "Tech Talkies", 100);

// ---------------- JOYSTICK SETUP (ADC1 only) ----------------
// LEFT STICK (movement)
#define VRX_PIN   34     // ADC1_CH6  (input-only pin is fine)
#define VRY_PIN   35     // ADC1_CH7

// RIGHT STICK (camera)  -> use other ADC1 pins safe with BLE
#define VRX2_PIN  32     // ADC1_CH4
#define VRY2_PIN  33     // ADC1_CH5

// Inversion (game/feel preferences)
#define INVERT_X      true
#define INVERT_Y      false
#define INVERT_RX     false
#define INVERT_RY     false

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

// -------- Runtime state --------
// Left stick
int   centerX = 2048, centerY = 2048;
float filtX = 0,     filtY = 0;

// Right stick (camera)
int   centerX2 = 2048, centerY2 = 2048;
float filtX2 = 0,      filtY2 = 0;

uint32_t lastReportMs = 0;
const uint32_t reportIntervalMs = 1000 / REPORT_HZ;

// ---------- helpers ----------
static inline int clampi(int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); }

// Map -1.0..+1.0 to 0..32767 (BleGamepad expects 0..32767)
static inline int16_t normToBle(float n){
  // n in [-1,1] → [0,32767]
  float v = (n * 0.5f + 0.5f) * 32767.0f;
  int iv = (int)roundf(v);
  return (int16_t)clampi(iv, 0, 32767);
}

// Apply deadzone while preserving direction (1D)
static inline float applyDeadzone(float n, float dz){
  float a = fabsf(n);
  if (a <= dz) return 0.0f;
  float out = (a - dz) / (1.0f - dz);
  return (n < 0 ? -out : out);
}

// Average centers for any pair of pins (stick untouched)
void calibrateCenter2(int pinX, int pinY, int &cx, int &cy, uint16_t samples=600){
  uint32_t sx=0, sy=0;
  for (uint16_t i=0;i<samples;i++){
    sx += analogRead(pinX);
    sy += analogRead(pinY);
    delay(1);
  }
  cx = sx / samples;
  cy = sy / samples;
}

void setup() {
  Serial.begin(115200);

  // Buttons
  for (int i = 0; i < NUM_BUTTONS; i++) pinMode(buttonPins[i], INPUT_PULLUP);

  // Optional: tune ADC
  // analogSetWidth(12);                 // default 12-bit → 0..4095
  // analogSetAttenuation(ADC_11db);     // improves high-end range near 3.3V

  // Power/wiring sanity:
  // VCC → 3.3V, GND → GND
  // LEFT:  VRx → VRX_PIN (34), VRy → VRY_PIN (35)
  // RIGHT: VRx → VRX2_PIN (32), VRy → VRY2_PIN (33)

  bleGamepad.begin();
  delay(200);

  // Ask user to release both sticks for ~0.5–1s at boot
  calibrateCenter2(VRX_PIN,  VRY_PIN,  centerX,  centerY);
  calibrateCenter2(VRX2_PIN, VRY2_PIN, centerX2, centerY2);

  // Initialize filters at centers
  filtX  = centerX;   filtY  = centerY;
  filtX2 = centerX2;  filtY2 = centerY2;

  Serial.println("Calibration done.");
  Serial.printf("Left center:  (%d, %d)\n", centerX, centerY);
  Serial.printf("Right center: (%d, %d)\n", centerX2, centerY2);
}

void loop() {
  // --------- Read analogs ---------
  // Left stick
  int rx  = analogRead(VRX_PIN);   // 0..4095
  int ry  = analogRead(VRY_PIN);   // 0..4095
  // Right stick (camera)
  int rxx = analogRead(VRX2_PIN);  // 0..4095
  int ryy = analogRead(VRY2_PIN);  // 0..4095

  // --------- Low-pass filter (1st order IIR) ---------
  filtX  = SMOOTH_ALPHA * rx  + (1.0f - SMOOTH_ALPHA) * filtX;
  filtY  = SMOOTH_ALPHA * ry  + (1.0f - SMOOTH_ALPHA) * filtY;
  filtX2 = SMOOTH_ALPHA * rxx + (1.0f - SMOOTH_ALPHA) * filtX2;
  filtY2 = SMOOTH_ALPHA * ryy + (1.0f - SMOOTH_ALPHA) * filtY2;

  // --------- Normalize to [-1, +1] around centers ---------
  float nx = (filtX  - centerX ) / (float)max(1, max(centerX  - ADC_MIN, ADC_MAX - centerX ));
  float ny = (filtY  - centerY ) / (float)max(1, max(centerY  - ADC_MIN, ADC_MAX - centerY ));
  float rxn= (filtX2 - centerX2) / (float)max(1, max(centerX2 - ADC_MIN, ADC_MAX - centerX2));
  float ryn= (filtY2 - centerY2) / (float)max(1, max(centerY2 - ADC_MIN, ADC_MAX - centerY2));

  // Clamp to [-1,1]
  nx = fminf(1.0f, fmaxf(-1.0f, nx));
  ny = fminf(1.0f, fmaxf(-1.0f, ny));
  rxn= fminf(1.0f, fmaxf(-1.0f, rxn));
  ryn= fminf(1.0f, fmaxf(-1.0f, ryn));

  // Invert if desired
  if (INVERT_X)  nx  = -nx;
  if (INVERT_Y)  ny  = -ny;
  if (INVERT_RX) rxn = -rxn;
  if (INVERT_RY) ryn = -ryn;

  // Deadzone
  nx  = applyDeadzone(nx,  DEADZONE_PCT);
  ny  = applyDeadzone(ny,  DEADZONE_PCT);
  rxn = applyDeadzone(rxn, DEADZONE_PCT);
  ryn = applyDeadzone(ryn, DEADZONE_PCT);

  // --------- Send at fixed rate ---------
  uint32_t now = millis();
  if (bleGamepad.isConnected() && (now - lastReportMs) >= reportIntervalMs) {
    // Left (movement) and Right (camera)
    bleGamepad.setLeftThumb (normToBle(nx),  normToBle(ny));
    bleGamepad.setRightThumb(normToBle(rxn), normToBle(ryn));

    // Buttons
    for (int i = 0; i < NUM_BUTTONS; i++) {
      if (!digitalRead(buttonPins[i])) bleGamepad.press(buttons[i]);
      else                             bleGamepad.release(buttons[i]);
    }

    lastReportMs = now;
  }

  // Tiny idle to reduce CPU thrash
  delay(1);
}

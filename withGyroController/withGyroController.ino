#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <BleGamepad.h>

// ---------------- BUTTONS ----------------
#define BUTTON_PIN1 12
#define BUTTON_PIN2 14
#define BUTTON_PIN3 27
#define BUTTON_PIN4 26
#define NUM_BUTTONS 4

BleGamepad bleGamepad("God of War Gamepad", "Tech Talkies", 100);

int buttonPins[NUM_BUTTONS] = { BUTTON_PIN1, BUTTON_PIN2, BUTTON_PIN3, BUTTON_PIN4 };
int buttons[NUM_BUTTONS] = {
  BUTTON_4,  // Square (X on Xbox)
  BUTTON_5,  // Triangle (Y)
  BUTTON_6,  // L1 (LB)
  BUTTON_2   // Circle (B)
};

// ---------------- LEFT STICK (analog) ----------------
// Use ADC1 pins on ESP32 (stable with WiFi/BLE)
#define VRX_PIN 34
#define VRY_PIN 35
#define INVERT_X true
#define INVERT_Y false

static const int   ADC_MIN = 0;
static const int   ADC_MAX = 4095;
static const float DEADZONE_PCT = 0.06f;
static const float SMOOTH_ALPHA = 0.35f;

int centerX = 2048, centerY = 2048;
float filtX = 2048, filtY = 2048;

// ---------------- RIGHT STICK (gyro → camera) ----------------
Adafruit_MPU6050 mpu;

// Integrated gyro state (arbitrary units; we re-scale before sending)
float camX = 0.0f; // yaw   (→ right stick X)
float camY = 0.0f; // pitch (→ right stick Y)

float gyroSensitivity = 0.35f; // camera speed; raise if too slow
float gyroDamping     = 0.92f; // 0..1; lower = stronger decay (less drift)

uint32_t lastTickMs = 0;

// ---------- helpers ----------
static inline int clampi(int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); }
static inline float clampf(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }

// Map normalized [-1..1] to 0..32767 for BleGamepad
static inline int16_t normToBle(float n){
  n = clampf(n, -1.0f, 1.0f);
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

void calibrateJoystick(uint16_t samples=600){
  uint32_t sx=0, sy=0;
  for (uint16_t i=0;i<samples;i++){
    sx += analogRead(VRX_PIN);
    sy += analogRead(VRY_PIN);
    delay(1);
  }
  centerX = sx / samples;
  centerY = sy / samples;
  filtX = centerX;
  filtY = centerY;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); // SDA=21 SCL=22 by default on most ESP32 dev boards

  for (int i = 0; i < NUM_BUTTONS; i++) pinMode(buttonPins[i], INPUT_PULLUP);

  bleGamepad.begin();
  delay(200);

  calibrateJoystick();

  // MPU6050 init
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found — check SDA/SCL wiring and power");
    while (true) delay(1000);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  delay(100);

  lastTickMs = millis();
}

void loop() {
  if (!bleGamepad.isConnected()) { delay(2); return; }

  // ----- timing -----
  uint32_t now = millis();
  float dt = (now - lastTickMs) / 1000.0f;
  if (dt <= 0.0f || dt > 0.2f) dt = 0.01f; // guard for long pauses
  lastTickMs = now;

  // ----- left stick (analog) -----
  int rx = analogRead(VRX_PIN);
  int ry = analogRead(VRY_PIN);

  // smooth
  filtX = SMOOTH_ALPHA * rx + (1.0f - SMOOTH_ALPHA) * filtX;
  filtY = SMOOTH_ALPHA * ry + (1.0f - SMOOTH_ALPHA) * filtY;

  // normalize around centers
  float nx = (filtX - centerX) / (float)max(1, max(centerX - ADC_MIN, ADC_MAX - centerX));
  float ny = (filtY - centerY) / (float)max(1, max(centerY - ADC_MIN, ADC_MAX - centerY));
  nx = clampf(nx, -1.0f, 1.0f);
  ny = clampf(ny, -1.0f, 1.0f);

  if (INVERT_X) nx = -nx;
  if (INVERT_Y) ny = -ny;

  nx = applyDeadzone(nx, DEADZONE_PCT);
  ny = applyDeadzone(ny, DEADZONE_PCT);

  // ----- right stick (gyro → camera) -----
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  // gyro.gyro.{x,y,z} are in radians/sec or deg/sec? (Adafruit returns rad/s)
  // Adafruit_MPU6050 returns rad/s. Convert to deg/s if you prefer; here we keep rad/s and tune sensitivity.
  // yaw   ← z, pitch ← x
  camX += gyro.gyro.z * gyroSensitivity * dt; // yaw → RS X
  camY += gyro.gyro.x * gyroSensitivity * dt; // pitch → RS Y

  // damping to control drift and provide spring-like feel
  camX *= gyroDamping;
  camY *= gyroDamping;

  // Convert camX/camY to normalized stick range [-1..1]
  // Scale factor limits how far the stick deflects for a given integrated motion.
  const float CAM_SCALE = 1.2f; // lower = less deflection per motion
  float rsx = clampf(camX * CAM_SCALE, -1.0f, 1.0f);
  float rsy = clampf(camY * CAM_SCALE, -1.0f, 1.0f);

  // ----- send to BLE -----
  bleGamepad.setLeftThumb( normToBle(nx), normToBle(ny) );
  bleGamepad.setRightThumb( normToBle(rsx), normToBle(rsy) );

  // buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!digitalRead(buttonPins[i])) bleGamepad.press(buttons[i]);
    else bleGamepad.release(buttons[i]);
  }

  delay(1);
}

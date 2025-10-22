#include <Arduino.h>
#line 1 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

#define TFT_CS    5
#define TFT_DC    27
#define TFT_RST   26
#define TOUCH_CS  14
#define TOUCH_IRQ 35

// -------------------- ESP32 VSPI pins -----------------------
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

// -------------------- Screen geometry -----------------------
#define SCREEN_W  320
#define SCREEN_H  240

// "NEXT" button geometry
#define BTN_W     90
#define BTN_H     44
#define BTN_X     (SCREEN_W - BTN_W - 12)   // right padding
#define BTN_Y     (SCREEN_H - BTN_H - 12)   // bottom padding

// -------------------- Touch calibration ---------------------
// Start with these; adjust after printing RAW & MAPPED in Serial.
#define RAW_X_MIN  200
#define RAW_X_MAX  3900
#define RAW_Y_MIN  300
#define RAW_Y_MAX  3800
// If X or Y is mirrored, swap MIN/MAX in mapconstrain() call below.
// If rotated weirdly, try ts.setRotation(0/1/2/3).

// -------------------- Objects -------------------------------
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Adafruit_PWMServoDriver pwm(0x40);

// -------------------- Servo config --------------------------
const uint16_t SERVO_MIN_TICKS[5] = {150, 150, 150, 150, 150};
const uint16_t SERVO_MAX_TICKS[5] = {600, 600, 600, 600, 600};
const bool     SERVO_INVERT[5]    = {false, false, false, false, false};

const uint16_t SERVO_FREQ_HZ       = 60;
const uint16_t TRANSITION_MS       = 150;   // tween duration
const uint16_t STEP_DELAY_MS       = 5;     // tween step delay
const uint16_t HOLD_MS             = 700;   // hold on target pose

// Poses (0..1)
const float POSE_OPEN[5]  = {0.8, 1.0, 1.0, 1.0, 1.0};
const float POSE_A[5]     = {0.35, 0.05, 0.05, 0.05, 0.05};
const float POSE_B[5]     = {0.10, 0.95, 0.95, 0.95, 0.95};
const float POSE_C[5]     = {0.55, 0.55, 0.55, 0.55, 0.55};
const float POSE_D[5]     = {0.40, 0.95, 0.10, 0.10, 0.10};
const float POSE_E[5]     = {0.10, 0.25, 0.25, 0.25, 0.25};

const float* SEQ[]        = { POSE_A, POSE_B, POSE_C, POSE_D, POSE_E };
const char   SEQ_LABELS[] = { 'A',     'B',     'C',     'D',     'E'  };
const uint8_t SEQ_LEN     = sizeof(SEQ) / sizeof(SEQ[0]);

// -------------------- State -------------------------------
uint8_t selIdx = 0; // which letter we’re on (0..SEQ_LEN-1)
unsigned long lastTouchMs = 0;

// -------------------- Utils -------------------------------
#line 70 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
static float clamp01(float x);
#line 72 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
static int mapconstrain(long v, long in_min, long in_max, long out_min, long out_max);
#line 80 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
uint16_t pctToTicks(uint8_t s, float v01);
#line 91 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
void writeServo(uint8_t s, float v01);
#line 95 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
static uint16_t u16max(uint16_t a, uint16_t b);
#line 97 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
void betweenPose(const float fromPose[5], const float toPose[5]);
#line 117 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
void drawHeader(const char* title);
#line 126 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
void drawBigLetter(char ch);
#line 145 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
void showSelection(uint8_t idx);
#line 70 "C:\\Users\\matt\\AppData\\Local\\Temp\\.arduinoIDE-unsaved202598-23788-gxk1td.ze7fc\\sketch_oct8a\\sketch_oct8a.ino"
static inline float clamp01(float x){ if(x<0) return 0; if(x>1) return 1; return x; }

static inline int mapconstrain(long v, long in_min, long in_max, long out_min, long out_max) {
  // linear map + clamp
  v = (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if (v < out_min) v = out_min;
  if (v > out_max) v = out_max;
  return (int)v;
}

uint16_t pctToTicks(uint8_t s, float v01) {
  v01 = clamp01(v01);
  if (SERVO_INVERT[s]) v01 = 1.0f - v01;
  const uint16_t lo = SERVO_MIN_TICKS[s];
  const uint16_t hi = SERVO_MAX_TICKS[s];
  float t = lo + (hi - lo) * v01;
  if (t < 0) t = 0;
  if (t > 4095) t = 4095;
  return (uint16_t)t;
}

void writeServo(uint8_t s, float v01) {
  pwm.setPWM(s, 0, pctToTicks(s, v01));
}

static inline uint16_t u16max(uint16_t a, uint16_t b){ return (a>b)?a:b; }

void betweenPose(const float fromPose[5], const float toPose[5]) {
  if (TRANSITION_MS == 0) {
    for (uint8_t i = 0; i < 5; ++i) writeServo(i, toPose[i]);
    return;
  }
  uint16_t div   = (STEP_DELAY_MS == 0) ? 1 : STEP_DELAY_MS;
  uint16_t steps = TRANSITION_MS / div;
  steps = u16max((uint16_t)1, steps);

  for (uint16_t k = 0; k <= steps; ++k) {
    float t = (float)k / (float)steps;  // 0..1
    for (uint8_t i = 0; i < 5; ++i) {
      float v = fromPose[i] + (toPose[i] - fromPose[i]) * t;
      writeServo(i, v);
    }
    delay(STEP_DELAY_MS);
  }
}

// -------------------- UI -------------------------------
void drawHeader(const char* title) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(10, 10);
  tft.print(title);
  tft.drawFastHLine(0, 35, SCREEN_W, ILI9341_DARKGREY);
}

void drawBigLetter(char ch) {
  tft.fillRect(0, 50, SCREEN_W, 140, ILI9341_BLACK);
  tft.setTextSize(7);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor((SCREEN_W/2) - 24, 90); // rough center tweak
  tft.print(ch);
}

void drawNextButton(bool pressed=false) {
  uint16_t fill = pressed ? ILI9341_DARKGREY : ILI9341_NAVY;
  tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, fill);
  tft.drawRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  int16_t tx = BTN_X + 18, ty = BTN_Y + 12;
  tft.setCursor(tx, ty);
  tft.print("NEXT");
}

void showSelection(uint8_t idx) {
  drawHeader("PLEASE WORK

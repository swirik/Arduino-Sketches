#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_PWMServoDriver.h>

// -------------------- TFT & Touch Pins --------------------
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4

#define TOUCH_CS  14
#define TOUCH_IRQ 35

#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

// -------------------- Display Setup --------------------
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// Choose your rotation: 0,1,2,3 (must match TFT and the mapping below)
static const uint8_t SCREEN_ROTATION = 1;  // 320x240 in landscape

// Screen size for chosen rotation=1
static const int16_t SCREEN_W = 320;
static const int16_t SCREEN_H = 240;

// ---- Touch calibration (tune these for accurate mapping) ----
// Raw XPT2046 values typically near 200..3900 but vary by panel
// Start with these, then adjust so touch aligns to drawn buttons.
#define TS_MINX  250
#define TS_MAXX  3900
#define TS_MINY  320
#define TS_MAXY  3800

// -------------------- PCA9685 / Servo Setup --------------------
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40); // default addr 0x40

// Typical MG90S safe range; widen/narrow if needed after testing:
static const int SERVO_MIN_US = 500;   // microseconds at 0°
static const int SERVO_MAX_US = 2400;  // microseconds at 180°
static const float SERVO_FREQ   = 50.0f; // Hz (MG90S works fine at 50-60Hz)

// 5 channels (0..4) on the PCA9685, one per finger
static const uint8_t SERVO_CH[5] = {0,1,2,3,4};

// If any finger turns opposite direction, flip its angle here (0=normal, 1=invert)
static const uint8_t INVERT[5] = {0,0,0,0,0};

// -------------------- UI Layout --------------------
struct Rect { int16_t x, y, w, h; };
Rect btn1 = { 20,  60,  90, 60 };
Rect btn2 = { 115, 60,  90, 60 };
Rect btn3 = { 210, 60,  90, 60 };

// -------------------- POSES (EDIT THESE ANGLES ONLY) --------------------
// Each pose: {Thumb, Index, Middle, Ring, Pinky} in degrees [0..180]
int pose1[5] = {  0,  0, 0,  0, 0 };  // <-- CHANGE ANGLES HERE (POSE 1)
int pose2[5] = { 90,  90,  90,  90,  90 };  // <-- CHANGE ANGLES HERE (POSE 2)
int pose3[5] = { 180, 180, 180,  180,  180 };  // <-- CHANGE ANGLES HERE (POSE 3)

// -------------------- Helpers --------------------
uint16_t usToTicks(float us) {
  // PCA9685: 4096 ticks / cycle; tick_us = 1e6 / (freq * 4096)
  float tick_us = 1000000.0f / (SERVO_FREQ * 4096.0f);
  return (uint16_t)(us / tick_us);
}

uint16_t angleToTicks(float angle_deg) {
  if (angle_deg < 0)   angle_deg = 0;
  if (angle_deg > 180) angle_deg = 180;
  float pulse = SERVO_MIN_US + (angle_deg / 180.0f) * (SERVO_MAX_US - SERVO_MIN_US);
  return usToTicks(pulse);
}

void writeServo(uint8_t ch, float angle_deg) {
  uint16_t ticks = angleToTicks(angle_deg);
  // Set PWM from 'on' tick 0 to 'off' tick = ticks
  pwm.setPWM(ch, 0, ticks);
}

void setPose(const int target[5]) {
  for (int i = 0; i < 5; ++i) {
    float a = target[i];
    if (INVERT[i]) a = 180.0f - a; // flip if needed
    writeServo(SERVO_CH[i], a);
  }
}

// Simple hit-test
bool hit(const Rect& r, int16_t x, int16_t y) {
  return (x >= r.x && x < (r.x + r.w) && y >= r.y && y < (r.y + r.h));
}

void drawButton(const Rect& r, const char* label, uint16_t bg, uint16_t fg = ILI9341_WHITE) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, bg);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, ILI9341_WHITE);
  tft.setTextColor(fg);
  tft.setTextSize(2);
  int16_t tw = 6 * strlen(label) * 2; // rough width estimate (6 px per char * size)
  int16_t th = 16 * 2;                // rough height estimate
  int16_t cx = r.x + (r.w - tw) / 2;
  int16_t cy = r.y + (r.h - th) / 2 + 4;
  tft.setCursor(cx, cy);
  tft.print(label);
}

void drawUI(const char* status = "Tap a POSE") {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.print("Robotic Hand - 3 Poses");

  drawButton(btn1, "POSE 1", ILI9341_DARKGREEN);
  drawButton(btn2, "POSE 2", ILI9341_DARKCYAN);
  drawButton(btn3, "POSE 3", ILI9341_MAROON);

  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 150);
  tft.print("Status: ");
  tft.print(status);
}

// Map raw XPT2046 to screen coords (handles rotation=1 nicely)
bool readTouch(int16_t& sx, int16_t& sy) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint(); // raw

  // For rotation=1 (landscape), swap & invert appropriately
  // Adjust these mappings if your touches land in the wrong place.
  int16_t x = map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_W); // note: raw Y -> screen X
  int16_t y = map(p.x, TS_MINX, TS_MAXX, SCREEN_H, 0); // note: raw X -> screen Y (inverted)

  // Clamp
  if (x < 0) x = 0; if (x >= SCREEN_W) x = SCREEN_W - 1;
  if (y < 0) y = 0; if (y >= SCREEN_H) y = SCREEN_H - 1;

  sx = x; sy = y;
  return true;
}

// -------------------- Setup / Loop --------------------
void setup() {
  // Serial optional
  Serial.begin(115200);

  // SPI for TFT/Touch
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // TFT
  tft.begin();
  tft.setRotation(SCREEN_ROTATION);

  // Touch
  ts.begin();
  // (Some XPT2046 libraries have setRotation; if available, keep it aligned.)
  // ts.setRotation(SCREEN_ROTATION);

  // I2C + PCA9685
  Wire.begin(21, 22);
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);

  // Initialize to a neutral pose (optional)
  int neutral[5] = {90, 90, 90, 90, 90};
  setPose(neutral);

  drawUI("Ready");
}

void loop() {
  static uint32_t lastTouchMs = 0;
  int16_t x, y;

  if (readTouch(x, y)) {
    // basic debounce
    uint32_t now = millis();
    if (now - lastTouchMs > 150) {
      lastTouchMs = now;

      if (hit(btn1, x, y)) {
        setPose(pose1);  // <<< POSE 1
        drawUI("POSE 1 applied");
      } else if (hit(btn2, x, y)) {
        setPose(pose2);  // <<< POSE 2
        drawUI("POSE 2 applied");
      } else if (hit(btn3, x, y)) {
        setPose(pose3);  // <<< POSE 3
        drawUI("POSE 3 applied");
      }
    }
  }
}

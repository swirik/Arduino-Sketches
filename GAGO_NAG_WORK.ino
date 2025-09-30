#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TFT_MISO  12
#define TFT_MOSI  11
#define TFT_CLK   13


#define BTN_LEFT   2
#define BTN_OK     3
#define BTN_RIGHT  4
#define BTN_ACTIVE LOW  

struct Btn { uint8_t pin; bool last; unsigned long t; };
Btn bLeft{BTN_LEFT, HIGH, 0}, bOk{BTN_OK, HIGH, 0}, bRight{BTN_RIGHT, HIGH, 0};

bool justPressed(Btn &b, unsigned long db=30) {
  bool s = digitalRead(b.pin);
  if (s != b.last && (millis() - b.t) > db) {
    b.last = s; b.t = millis();
    if (s == BTN_ACTIVE) return true; 
  }
  return false;
}


uint8_t selIdx = 0;  


Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

const uint16_t SERVO_MIN_TICKS[5] = {150, 150, 150, 150, 150};
const uint16_t SERVO_MAX_TICKS[5] = {600, 600, 600, 600, 600};
const bool     SERVO_INVERT[5]    = {false, false, false, false, false}; 

const uint16_t SERVO_FREQ_HZ       = 60;
const uint16_t TRANSITION_MS       = 150;  
const uint16_t STEP_DELAY_MS       = 5;     
const uint16_t LETTER_INTERVAL_MS  = 3000;  

const float POSE_OPEN[5]  = {0.8, 1.0, 1.0, 1.0, 1.0};
const float POSE_A[5]     = {0.35, 0.05, 0.05, 0.05, 0.05}; 
const float POSE_B[5]     = {0.10, 0.95, 0.95, 0.95, 0.95}; 
const float POSE_C[5]     = {0.55, 0.55, 0.55, 0.55, 0.55}; 
const float POSE_D[5]     = {0.40, 0.95, 0.10, 0.10, 0.10}; 
const float POSE_E[5]     = {0.10, 0.25, 0.25, 0.25, 0.25}; 

const float* SEQ[]        = { POSE_A, POSE_B, POSE_C, POSE_D, POSE_E };
const char   SEQ_LABELS[] = { 'A',     'B',     'C',     'D',     'E'  };
const uint8_t SEQ_LEN     = sizeof(SEQ) / sizeof(SEQ[0]);

static inline float clamp01(float x){ if(x<0) return 0; if(x>1) return 1; return x; }




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
    float t = (float)k / (float)steps;  
    for (uint8_t i = 0; i < 5; ++i) {
      float v = fromPose[i] + (toPose[i] - fromPose[i]) * t;
      writeServo(i, v);
    }
    delay(STEP_DELAY_MS);
  }
}


void drawHeader(const char* title) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(10, 10);
  tft.print(title);
  tft.drawFastHLine(0, 35, 320, ILI9341_DARKGREY);
}

void drawBigLetter(char ch) {
  tft.fillRect(0, 50, 320, 180, ILI9341_BLACK);
  tft.setTextSize(7);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(130, 90); // adjust for your screen framing
  tft.print(ch);
}

void drawFooter(const char* line1, const char* line2 = nullptr) {
  tft.fillRect(0, 230, 320, 10, ILI9341_DARKGREY);
  tft.fillRect(0, 240, 320, 80, ILI9341_NAVY);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(8, 248);
  tft.print(line1);
  if (line2) { tft.setCursor(8, 272); tft.print(line2); }
}

void showHome() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN); tft.setCursor(10, 10);
  tft.println("Robotic Sign Tutor");
  tft.setTextColor(ILI9341_WHITE); tft.setCursor(10, 40);
  tft.println("A–E Demonstration");
  tft.setCursor(10, 64);  tft.println("3s each | Fast tween");
  tft.setCursor(10, 88);  tft.println("PCA9685 + ILI9341");
}

void showSelection(uint8_t idx) {
  drawHeader("Select A–E with buttons");
  drawBigLetter(SEQ_LABELS[idx]);
  drawFooter("<   OK   >");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ_HZ);
  delay(150);
  tft.begin();
  tft.setRotation(1); 
  // --- Buttons: internal pull-ups
pinMode(BTN_LEFT,  INPUT_PULLUP);
pinMode(BTN_OK,    INPUT_PULLUP);
pinMode(BTN_RIGHT, INPUT_PULLUP);
// init debounce baselines
bLeft.last  = digitalRead(BTN_LEFT);
bOk.last    = digitalRead(BTN_OK);
bRight.last = digitalRead(BTN_RIGHT);

// Show the initial selection on screen
showSelection(selIdx);


  showHome();

  for (uint8_t i = 0; i < 5; ++i) writeServo(i, POSE_OPEN[i]);
  delay(300);

  Serial.println(F("TEST"));
}

void loop() {
  static float current[5] = {
    POSE_OPEN[0], POSE_OPEN[1], POSE_OPEN[2], POSE_OPEN[3], POSE_OPEN[4]
  };

  // Navigate
  if (justPressed(bLeft)) {
    selIdx = (selIdx == 0) ? (SEQ_LEN - 1) : (selIdx - 1);
    showSelection(selIdx);
  }
  if (justPressed(bRight)) {
    selIdx = (selIdx + 1) % SEQ_LEN;
    showSelection(selIdx);
  }

  // Play pose when OK
  if (justPressed(bOk)) {
    const float* target = SEQ[selIdx];
    betweenPose(current, target);         // go to pose
    for (uint8_t i=0; i<5; ++i) current[i] = target[i];
    delay(700);                            // hold (tweak if you want)
    betweenPose(current, POSE_OPEN);       // relax
    for (uint8_t i=0; i<5; ++i) current[i] = POSE_OPEN[i];
    showSelection(selIdx);                 // redraw letter after motion
  }
}

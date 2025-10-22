#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_PWMServoDriver.h>

#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4
#define TOUCH_CS  14
#define TOUCH_IRQ 35
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

#define SCREEN_W 320
#define SCREEN_H 240

#define RAW_X_MIN  300
#define RAW_X_MAX  3800
#define RAW_Y_MIN  300
#define RAW_Y_MAX  3800

#define TOUCH_SWAP_XY  0   
#define TOUCH_INVERT_X 1   
#define TOUCH_INVERT_Y 1   

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Adafruit_PWMServoDriver pwm(0x40);

const uint16_t SERVO_MIN_TICKS[5] = {150, 150, 150, 150, 150};
const uint16_t SERVO_MAX_TICKS[5] = {600, 600, 600, 600, 600};
const bool     SERVO_INVERT[5]    = {false, false, false, false, false};

const float POSE_OPEN[5] = {1.00, 1.00, 1.00, 1.00, 1.00};
const float POSE_A[5]    = {0.35, 0.05, 0.05, 0.05, 0.05};
const float POSE_B[5]    = {1, 1, 1, 1, 1};
const float POSE_C[5]    = {0.55, 0.55, 0.55, 0.55, 0.55};

const char* LETTERS = "ABC";

float currentPose[5] = {POSE_OPEN[0], POSE_OPEN[1], POSE_OPEN[2], POSE_OPEN[3], POSE_OPEN[4]};

struct Rect { 
  int x, y, w, h; 
  };

struct Tap { 
  int x, y; 
  bool ok; 
  };

enum Mode { 
  WELCOME, 
  MENU, 
  DEMO, 
  QUIZ, 
  SPELLOUT 
  };

Mode mode = WELCOME;

const Rect BTN_PLAY = { 20,  180, 90, 40};
const Rect BTN_NEXT = { 120, 180, 90, 40};
const Rect BTN_BACK = { 220, 180, 90, 40};

const Rect BOX_DEMO     = { 30, 70,  260, 40};
const Rect BOX_QUIZ     = { 30, 120, 260, 40};
const Rect BOX_SPELLOUT = { 30, 170, 260, 40};

int demoIdx = 0; 
char quizTarget = 'A';
String spellWord = "ABC";
int spellPos = 0;

void header(const char* title) {
  tft.fillRect(0, 0, SCREEN_W, 36, ILI9341_DARKCYAN);
  tft.drawRect(0, 0, SCREEN_W, 36, ILI9341_WHITE);
  tft.setTextColor(ILI9341_WHITE, ILI9341_DARKCYAN);
  tft.setTextSize(2);
  tft.setCursor(8, 10);
  tft.print(title);
}

void centerLetter(char ch) {
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setTextSize(7);
  int16_t x = (SCREEN_W / 2) - 24;
  int16_t y = 70;
  tft.setCursor(x, y);
  tft.print(ch);
}

void drawButton(const Rect& r, const char* label, uint16_t bg) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, bg);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, ILI9341_WHITE);
  tft.setTextColor(ILI9341_WHITE, bg);
  tft.setTextSize(2);
  int16_t tx = r.x + 12, ty = r.y + (r.h/2) - 6;
  tft.setCursor(tx, ty);
  tft.print(label);
}

void drawWelcome() {
  tft.fillScreen(ILI9341_BLACK);
  header("Robotic Sign Language Tutor");
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(18, 90);
  tft.print("Touch to start");
}

void drawMenu() {
  tft.fillScreen(ILI9341_BLACK);
  header("Menu");
  drawButton(BOX_DEMO,     "Demonstration Mode", ILI9341_NAVY);
  drawButton(BOX_QUIZ,     "Quiz Mode",          ILI9341_MAROON);
  drawButton(BOX_SPELLOUT, "Spell Out Mode",      ILI9341_DARKGREEN);

  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 220);
}

void drawDemo() {
  tft.fillScreen(ILI9341_BLACK);
  header("Demonstration");
  char letter = LETTERS[demoIdx];
  centerLetter(letter);

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 140);
  tft.print("Tap PLAY to move, NEXT to change");

  drawButton(BTN_PLAY, "PLAY", ILI9341_DARKGREEN);
  drawButton(BTN_NEXT, "NEXT", ILI9341_NAVY);
  drawButton(BTN_BACK, "<",    ILI9341_DARKGREY);
}

void drawQuiz() {
  tft.fillScreen(ILI9341_BLACK);
  header("Quiz Mode");
  centerLetter('?'); 
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 140);
  tft.print("Guess the letter");
  tft.setCursor(10, 164);
  tft.print("Tap PLAY to show pose");
  drawButton(BTN_PLAY, "PLAY", ILI9341_DARKGREEN);
  drawButton(BTN_NEXT, "NEXT", ILI9341_NAVY);
  drawButton(BTN_BACK, "<",    ILI9341_DARKGREY);
}

void drawSpell() {
  tft.fillScreen(ILI9341_BLACK);
  header("SpellOut");
  char letter = spellWord[spellPos];
  centerLetter(letter);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 140);
  tft.print("Tap PLAY to pose letter");
  tft.setCursor(10, 164);
  tft.print("Word: "); tft.print(spellWord);
  tft.setCursor(10, 188);
  tft.printf("Pos: %d/%d", spellPos + 1, (int)spellWord.length());
  drawButton(BTN_PLAY, "PLAY", ILI9341_DARKGREEN);
  drawButton(BTN_NEXT, "NEXT", ILI9341_NAVY);
  drawButton(BTN_BACK, "<",    ILI9341_DARKGREY);
}

uint16_t pctToTicks(uint8_t s, float v01) {
  v01 = clamp01(v01);
  if (SERVO_INVERT[s]) {
    v01 = 1.0f - v01;
  }
  const uint16_t lo = SERVO_MIN_TICKS[s];
  const uint16_t hi = SERVO_MAX_TICKS[s];
  float t = lo + (hi - lo) * v01;
  if (t < 0){ 
    t = 0;
  }
  if (t > 4095) {
    t = 4095;
  }
  return (uint16_t)t;
}

void applyPoseInstant(const float pose[5]) {
  for (uint8_t s = 0; s < 5; ++s) {
    pwm.setPWM(s, 0, pctToTicks(s, pose[s]));
    currentPose[s] = pose[s];
  }
}

void tweenToPose(const float pose[5], uint16_t ms = 450, uint8_t steps = 24) {
  for (uint8_t k = 1; k <= steps; ++k) {
    for (uint8_t s = 0; s < 5; ++s) {
      float v = currentPose[s] + (pose[s] - currentPose[s]) * (float)k / steps;
      pwm.setPWM(s, 0, pctToTicks(s, v));
    }
    delay(ms / steps);
  }
  for (uint8_t s = 0; s < 5; ++s) { 
  currentPose[s] = pose[s];
  }
}

const float* poseForLetter(char c) {
  switch (c) {
    case 'A':
     return POSE_A;
    case 'B': 
    return POSE_B;
    case 'C': 
    return POSE_C;
    default:  
    return POSE_OPEN;
  }
}


void playLetter(char c) {
  const float* tgt = poseForLetter(c);
  tweenToPose(tgt, 450, 24);
  delay(300);
  tweenToPose(POSE_OPEN, 350, 18);
}
static inline int imap(long v, long in_min, long in_max, long out_min, long out_max) {
  v = (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if (v < out_min) {
  v = out_min;
  } 
    
  if (v > out_max) {
    v = out_max;
  } 
  return (int)v;
}

bool hit(const Rect& r, int x, int y) {
  return (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
}

bool readTouchPoint(int &sx, int &sy) {
  if (!ts.touched()) return false;

  const int N = 4;
  long rx = 0, ry = 0;
  for (int i = 0; i < N; ++i) {
    auto p = ts.getPoint();   
    rx += p.x;
    ry += p.y;
    delay(2);
  }
  rx /= N; ry /= N;

  long rxs = TOUCH_SWAP_XY ? ry : rx;
  long rys = TOUCH_SWAP_XY ? rx : ry;

  int x = imap(rxs, RAW_X_MIN, RAW_X_MAX, 0, SCREEN_W - 1);
  int y = imap(rys, RAW_Y_MIN, RAW_Y_MAX, 0, SCREEN_H - 1);

  if (TOUCH_INVERT_X) {
    x = (SCREEN_W - 1) - x;
  }

  if (TOUCH_INVERT_Y) {
    y = (SCREEN_H - 1) - y;
  }

  sx = x; 
  sy = y;
  return true;
}

Tap getTap(uint16_t hold_ms = 600) {
  Tap t{0, 0, false};

  uint32_t t0 = millis();
  while (!ts.touched()) {
    if (millis() - t0 > hold_ms) {
       return t;
    }
    delay(2);
  }
  int x, y, cnt = 0;
  long ax = 0, ay = 0;
  uint32_t t1 = millis();
  while (ts.touched() && (millis() - t1) < hold_ms) {
    if (readTouchPoint(x, y)) {
      ax += x; ay += y; cnt++;
    }
    delay(2);
  }
  if (cnt == 0) {
    return t;
  }
  t.x = (int)(ax / cnt);
  t.y = (int)(ay / cnt);
  t.ok = true;
  delay(40); 
  return t;
}

float clamp01(float x) { 
    if (x < 0) 
      return 0; 
    if (x > 1) 
    return 1; 
  return x; 
  }



void setup() {
  Serial.begin(115200);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  tft.begin();
  tft.setRotation(1);   
  ts.begin();           
  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(50); 
  delay(10);
  applyPoseInstant(POSE_OPEN);
  randomSeed(esp_random());
  drawWelcome();
}

void loop() {
  Tap tap = getTap();  
  if (tap.ok) {
    tft.fillCircle(tap.x, tap.y, 3, ILI9341_YELLOW);
  }

  switch (mode) {

    case WELCOME:
      if (tap.ok) { 
        mode = MENU; 
        drawMenu(); 
        }
      break;

    case MENU:
      if (tap.ok) {
        if (hit(BOX_DEMO, tap.x, tap.y)) {
          demoIdx = 0; 
          mode = DEMO; 
          drawDemo();
        } 
        else if (hit(BOX_QUIZ, tap.x, tap.y)) {
          quizTarget = LETTERS[random(0, 3)];
          mode = QUIZ; 
          drawQuiz();
        } 
        else if (hit(BOX_SPELLOUT, tap.x, tap.y)) {
          spellPos = 0; 
          mode = SPELLOUT; 
          drawSpell();
        }
      }
      break;

    case DEMO:
      if (tap.ok) {
        if (hit(BTN_PLAY, tap.x, tap.y)) {
          playLetter(LETTERS[demoIdx]);
        } 
        else if (hit(BTN_NEXT, tap.x, tap.y)) {
          demoIdx = (demoIdx + 1) % 3; 
          drawDemo();
        } 
        else if (hit(BTN_BACK, tap.x, tap.y)) {
          mode = MENU; 
          drawMenu();
        }
      }
      break;

    case QUIZ:
      if (tap.ok) {
        if (hit(BTN_PLAY, tap.x, tap.y)) {
          playLetter(quizTarget);
        } 
        else if (hit(BTN_NEXT, tap.x, tap.y)) {
          quizTarget = LETTERS[random(0, 3)];
          drawQuiz();
        } 
        else if (hit(BTN_BACK, tap.x, tap.y)) {
          mode = MENU; 
          drawMenu();
        }
      }
      break;

    case SPELLOUT:
      if (tap.ok) {
        if (hit(BTN_PLAY, tap.x, tap.y)) {
          playLetter(spellWord[spellPos]);
        }
        else if (hit(BTN_NEXT, tap.x, tap.y)) {
          spellPos++;
          if (spellPos >= spellWord.length())
          spellPos = 0; 
          drawSpell();
        } 
        else if (hit(BTN_BACK, tap.x, tap.y)) {
          mode = MENU; 
          drawMenu();
        }
      }
      break;
  }
}

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

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

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

#define RAW_X_MIN  200
#define RAW_X_MAX  3900
#define RAW_Y_MIN  300
#define RAW_Y_MAX  3800

struct Rect {
  int x, y, w, h;
};z

static inline int mapconstrain(long v, long in_min, long in_max, long out_min, long out_max) {
  v = (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if (v < out_min) v = out_min;
  if (v > out_max) v = out_max;
  return (int)v;
}

static inline bool hit(const Rect& r, int x, int y) {
  return (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h);
}

void drawButton(const Rect& r, const char* label, uint16_t fill, uint16_t frame=ILI9341_WHITE) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, fill);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, frame);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  int16_t tx = r.x + 10;
  int16_t ty = r.y + (r.h/2) - 8;
  tft.setCursor(tx, ty);
  tft.print(label);
}


void header(const char* title) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(10, 10);
  tft.print(title);
  tft.drawFastHLine(0, 35, SCREEN_W, ILI9341_DARKGREY);
}

void centerLetter(char ch) {
  tft.setTextSize(7);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  int16_t x = (SCREEN_W/2) - 24; // rough center
  int16_t y = 90;
  tft.setCursor(x, y);
  tft.print(ch);
}

// -------------------- Modes --------------------
enum Mode { WELCOME, MENU, DEMO, QUIZ, SPELLOUT };
Mode mode = WELCOME;

const char LETTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const uint8_t NLET = 26;

uint8_t idx = 0;            
String spellWord = "HELLO"; 
uint8_t spellPos = 0;

bool quizReveal = false;
unsigned long lastTouchMs = 0;

const Rect BTN_CONT  { 60,  150, 200, 50 };      
const Rect BTN_DEMO  { 20,   70, 120, 50 };
const Rect BTN_QUIZ  { 180,  70, 120, 50 };
const Rect BTN_SPELL { 20,  140, 280, 50 };

const Rect BTN_NEXT   { SCREEN_W - 100, SCREEN_H - 52, 90, 40 };
const Rect BTN_REVEAL { 10,              SCREEN_H - 52, 90, 40 };
const Rect BTN_BACK   { 10,              40,             60, 32 };

void drawWelcome() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(22, 60);
  tft.print("Robotic Sign");
  tft.setCursor(22, 90);
  tft.print("Language Tutor");

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(22, 130);
  tft.print("Touch to continue");

  drawButton(BTN_CONT, "CONTINUE", ILI9341_NAVY);
}

void drawMenu() {
  header("Select Mode");
  drawButton(BTN_DEMO,  "Demonstration", ILI9341_NAVY);
  drawButton(BTN_QUIZ,  "Quiz Mode",     ILI9341_DARKGREEN);
  drawButton(BTN_SPELL, "SpellOut",      ILI9341_MAROON);
}

void drawDemo() {
  header("Demonstration");
  centerLetter(LETTERS[idx]);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(10, 150);
  tft.print("Tap NEXT to advance");

  drawButton(BTN_NEXT, "NEXT", ILI9341_NAVY);
  drawButton(BTN_BACK, "<",    ILI9341_DARKGREY);
}

void drawQuiz() {
  header("Quiz");
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(10, 80);
  tft.print("Imagine hand sign here :)");

  if (quizReveal) {
    centerLetter(LETTERS[idx]);
  } else {
    tft.fillRect(0, 80+40, SCREEN_W, 120, ILI9341_BLACK);
  }

  drawButton(BTN_REVEAL, "REVEAL", ILI9341_DARKGREY);
  drawButton(BTN_NEXT,   "NEXT",   ILI9341_NAVY);
  drawButton(BTN_BACK,   "<",      ILI9341_DARKGREY);
}

void drawSpell() {
  header("SpellOut");
  char letter = spellWord[spellPos];
  centerLetter(letter);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(10, 160);
  tft.print("Word: "); tft.print(spellWord);
  tft.setCursor(10, 184);
  tft.printf("Pos: %d/%d", spellPos+1, (int)spellWord.length());

  drawButton(BTN_NEXT, "NEXT", ILI9341_NAVY);
  drawButton(BTN_BACK, "<",    ILI9341_DARKGREY);
}

void gotoWelcome() { mode = WELCOME; drawWelcome(); }
void gotoMenu()    { mode = MENU;    drawMenu(); }
void gotoDemo()    { mode = DEMO;    idx = 0;     drawDemo(); }
void gotoQuiz()    { mode = QUIZ;    idx = 0; quizReveal = false; drawQuiz(); }
void gotoSpell()   { mode = SPELLOUT; spellPos = 0; drawSpell(); }

void setup() {
  Serial.begin(115200);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  pinMode(TFT_CS, OUTPUT);   digitalWrite(TFT_CS, HIGH);
  pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);

  tft.begin();
  tft.setRotation(1);
  ts.begin(SPI);
  ts.setRotation(1);

  gotoWelcome();
}

void loop() {
  if (ts.tirqTouched() && ts.touched()) {
    digitalWrite(TFT_CS, HIGH);
    TS_Point p = ts.getPoint();

    int x = mapconstrain(p.x, RAW_X_MAX, RAW_X_MIN, 0, SCREEN_W - 1); 
    int y = mapconstrain(p.y, RAW_Y_MAX, RAW_Y_MIN, 0, SCREEN_H - 1);


    unsigned long now = millis();
    if (now - lastTouchMs < 140) return;
    lastTouchMs = now;

    switch (mode) {
      case WELCOME:
        if (hit(BTN_CONT, x, y) || true) { 
          gotoMenu();
        }
        break;

      case MENU:
        if (hit(BTN_DEMO, x, y))  gotoDemo();
        else if (hit(BTN_QUIZ, x, y))  gotoQuiz();
        else if (hit(BTN_SPELL, x, y)) gotoSpell();
        break;

      case DEMO:
        if (hit(BTN_BACK, x, y)) { gotoMenu(); break; }
        if (hit(BTN_NEXT, x, y)) {
          idx = (idx + 1) % NLET;
          drawDemo();
        }
        break;

      case QUIZ:
        if (hit(BTN_BACK, x, y)) { gotoMenu(); break; }
        if (hit(BTN_REVEAL, x, y)) { quizReveal = true; drawQuiz(); }
        if (hit(BTN_NEXT, x, y))   { idx = random(NLET); quizReveal = false; drawQuiz(); }
        break;

      case SPELLOUT:
        if (hit(BTN_BACK, x, y)) { gotoMenu(); break; }
        if (hit(BTN_NEXT, x, y)) {
          spellPos = (spellPos + 1) % spellWord.length();
          drawSpell();
        }
        break;
    }
  }
}

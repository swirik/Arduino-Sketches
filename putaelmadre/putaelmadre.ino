#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   4
#define TOUCH_CS  14
#define TOUCH_IRQ 35

// ESP32 VSPI pins (explicit!)
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// --- Calib (rough defaults; adjust after you see touches)
#define RAW_X_MIN  200
#define RAW_X_MAX  3900
#define RAW_Y_MIN  300
#define RAW_Y_MAX  3800

enum ScreenState { MENU, BOX1, BOX2, BOX3 };
ScreenState current = MENU;
uint8_t touchCount = 0;

#define SCREEN_W 320
#define SCREEN_H 240
#define BOX_W    (SCREEN_W / 3)
#define BOX_H    100

static inline int mapconstrain(long v, long in_min, long in_max, long out_min, long out_max) {
  v = (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if (v < out_min) v = out_min;
  if (v > out_max) v = out_max;
  return (int)v;
}

void drawMenu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);

  tft.fillRect(0, 70, BOX_W - 2, BOX_H, ILI9341_RED);
  tft.setCursor(30, 110); tft.print("BOX 1");

  tft.fillRect(BOX_W + 1, 70, BOX_W - 2, BOX_H, ILI9341_GREEN);
  tft.setCursor(BOX_W + 30, 110); tft.print("BOX 2");

  tft.fillRect(2 * BOX_W + 1, 70, BOX_W - 2, BOX_H, ILI9341_BLUE);
  tft.setCursor(2 * BOX_W + 30, 110); tft.print("BOX 3");

  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(50, 200); tft.print("Touch a box to activate");
}

void showBoxPage(uint8_t boxNumber, uint16_t color, const char* label) {
  tft.fillScreen(color);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(60, 100);
  tft.printf("%s ACTIVE", label);
  tft.setTextSize(2);
  tft.setCursor(50, 180);
  tft.print("Double-tap to go back");
}

void setup() {
  Serial.begin(115200);

  // Bind SPI to VSPI pins explicitly
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  pinMode(TFT_CS, OUTPUT);   digitalWrite(TFT_CS, HIGH);
  pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);

  tft.begin();            // library will toggle TFT_CS as needed
  tft.setRotation(1);

  ts.begin(SPI);          // use the same SPI bus
  ts.setRotation(1);      // match display rotation

  drawMenu();
}

unsigned long lastTouchMs = 0;

void loop() {
  // Cheap IRQ gate prevents false reads
  if (ts.tirqTouched() && ts.touched()) {
    // Make sure TFT is not selected while we talk to touch
    digitalWrite(TFT_CS, HIGH);

    TS_Point p = ts.getPoint(); // typically raw 0..4095
    // Map raw -> screen coordinates (landscape rotation 1)
// Option A – flip X
    int x = mapconstrain(p.x, RAW_X_MAX, RAW_X_MIN, 0, SCREEN_W - 1);

// Option B – flip Y (if it’s vertically flipped instead)
    int y = mapconstrain(p.y, RAW_Y_MAX, RAW_Y_MIN, 0, SCREEN_H - 1);


    Serial.printf("RAW x=%d y=%d  MAPPED x=%d y=%d\n", p.x, p.y, x, y);

    // simple debounce
    unsigned long now = millis();
    if (now - lastTouchMs < 120) return;
    lastTouchMs = now;

    if (current == MENU) {
      if (y > 70 && y < (70 + BOX_H)) {
        if (x < BOX_W) {
          current = BOX1; showBoxPage(1, ILI9341_RED, "BOX 1");
        } else if (x < 2 * BOX_W) {
          current = BOX2; showBoxPage(2, ILI9341_GREEN, "BOX 2");
        } else {
          current = BOX3; showBoxPage(3, ILI9341_BLUE, "BOX 3");
        }
        touchCount = 0;
      }
    } else {
      // In a box page — require double tap to go back
      touchCount++;
      if (touchCount >= 2) {
        drawMenu();
        current = MENU;
        touchCount = 0;
      }
    }
  }
}

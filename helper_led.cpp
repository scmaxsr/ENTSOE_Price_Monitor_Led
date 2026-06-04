#include "helper_led.h"
#include "helper_config.h"

// Define matrix and related globals
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, D3,
  NEO_MATRIX_BOTTOM     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

const uint16_t colors[] = {
  matrix.Color(0, 255, 0),    // Level 1: Green
  matrix.Color(85, 255, 0),   // Level 2: Yellow-Green
  matrix.Color(255, 180, 0),  // Level 3: Amber/Yellow
  matrix.Color(255, 80, 0),   // Level 4: Orange
  matrix.Color(255, 0, 0)     // Level 5: Red
};

bool blinkState = false;
bool apModeActive = false;

void matrixInitialize() {
  Serial.println("Initialize LED Matrix");
  matrix.begin();
  uint8_t brightness = config.ledBrightness > 0 ? config.ledBrightness : defaultLedBrightness;
  matrix.setBrightness(brightness);
  matrix.fillScreen(0);
  matrix.show();
  blinkState = false;
}

void drawPixelRemapped(int x, int y, uint16_t color) {
  x = 7 - x;
  if (x % 2 == 0) {
    y = 7 - y;
  }
  matrix.drawPixel(x, y, color);
}

void matrixLine(int column, int height, uint16_t color) {
  for (int y = 0; y < height; y++) {
    drawPixelRemapped(column, 7 - y, color);
  }
}

void matrixShowTest() {
  matrix.fillScreen(0);
  matrixLine(0, 1, colors[0]);
  matrixLine(1, 2, colors[1]);
  matrixLine(2, 3, colors[2]);
  matrixLine(3, 4, colors[3]);
  matrixLine(4, 5, colors[4]);
  matrixLine(5, 6, colors[3]);
  matrixLine(6, 7, colors[2]);
  matrixLine(7, 8, colors[1]);
  matrix.show();
  delay(2000);
}

void matrixShowEntsoe() {
  matrix.fillScreen(0);
  Serial.println("Updating LED Matrix with price data...");
  for (int i = 0; i < MATRIX_DISPLAY_HOURS; i++) {
    if (!PRICES.price[i].isNull) {
      int height;
      if (PRICES.maximumPrice == PRICES.minimumPrice) {
        height = 4;
      } else {
        height = (int)(7 * (PRICES.price[i].price - PRICES.minimumPrice) / 
                      (PRICES.maximumPrice - PRICES.minimumPrice)) + 1;
      }
      if (height < 1) height = 1;
      if (height > 8) height = 8;
      int colorIndex = PRICES.price[i].level - 1;
      if (colorIndex < 0) colorIndex = 0;
      if (colorIndex > 4) colorIndex = 4;
      matrixLine(i, height, colors[colorIndex]);
      Serial.printf("  Col %d: price=%d, height=%d, level=%d\n", 
                    i, PRICES.price[i].price, height, PRICES.price[i].level);
    } else {
      drawPixelRemapped(i, 7, matrix.Color(10, 10, 10));
    }
  }

  if (!PRICES.price[0].isNull) {
    int animHeight;
    if (PRICES.maximumPrice == PRICES.minimumPrice) {
      animHeight = 4;
    } else {
      animHeight = (int)(7 * (PRICES.price[0].price - PRICES.minimumPrice) / 
                        (PRICES.maximumPrice - PRICES.minimumPrice)) + 1;
    }
    if (animHeight < 1) animHeight = 1;
    if (animHeight > 8) animHeight = 8;
    if (blinkState) {
      // normal
    } else {
      for (int y = 0; y < animHeight; y++) {
        drawPixelRemapped(0, 7 - y, matrix.Color(0, 0, 0));
      }
    }
    blinkState = !blinkState;
  } else {
    blinkState = false;
  }

  matrix.show();
}

void matrixShowConnecting() {
  static int step = 0;
  matrix.fillScreen(0);
  int x = step % 8;
  drawPixelRemapped(x, 0, matrix.Color(0, 0, 255));
  drawPixelRemapped(7 - x, 7, matrix.Color(0, 0, 255));
  matrix.show();
  step = (step + 1) % 8;
  delay(100);
}

void matrixShowAPMode() {
  static unsigned long lastToggle = 0;
  static bool apState = false;
  unsigned long now = millis();
  if (now - lastToggle > 500) {
    lastToggle = now;
    apState = !apState;
    matrix.fillScreen(0);
    if (apState) {
      for (int y = 0; y < 8; y++) {
        drawPixelRemapped(0, y, matrix.Color(0, 0, 80));
        drawPixelRemapped(7, y, matrix.Color(0, 0, 80));
      }
      for (int x = 2; x <= 5; x++) {
        for (int y = 2; y <= 5; y++) {
          drawPixelRemapped(x, y, matrix.Color(0, 0, 120));
        }
      }
    } else {
      for (int x = 0; x < 8; x++) {
        drawPixelRemapped(x, 0, matrix.Color(0, 0, 60));
        drawPixelRemapped(x, 7, matrix.Color(0, 0, 60));
      }
    }
    matrix.show();
  }
}

void matrixUpdateAPMode() {
  if (apModeActive) {
    matrixShowAPMode();
  }
}

void matrixShowError() {
  matrix.fillScreen(0);
  for (int i = 0; i < MATRIX_DISPLAY_HOURS; i++) {
    for (int j = 0; j < 8; j++) {
      drawPixelRemapped(i, j, matrix.Color(255, 0, 0));
    }
  }
  matrix.show();
  delay(500);
  matrix.fillScreen(0);
  matrix.show();
  delay(500);
}

void matrixPowerOff() {
  matrix.fillScreen(0);
  matrix.show();
  matrix.setBrightness(0);
  Serial.println("LED Matrix: powered off for deep sleep");
}

void matrixWakeUp() {
  matrix.begin();
  uint8_t brightness = config.ledBrightness > 0 ? config.ledBrightness : defaultLedBrightness;
  matrix.setBrightness(brightness);
  matrix.fillScreen(0);
  matrix.show();
  Serial.println("LED Matrix: re-initialized after wake");
}

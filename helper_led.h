#ifndef HELPER_LED_H
#define HELPER_LED_H

/*
 * LED Matrix Display Helper
 * 
 * Controls an 8x8 NeoPixel LED matrix to display electricity prices as a bar chart.
 * Each column represents one hour. Height = relative price, Color = price level.
 * 
 * Color mapping:
 *   Level 1 (Very Cheap):  Green
 *   Level 2 (Cheap):        Yellow-Green
 *   Level 3 (Normal):        Amber/Yellow
 *   Level 4 (Expensive):    Orange
 *   Level 5 (Very Expensive): Red
 */

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include "helper_entsoe.h"
#include "settings.h"

// MATRIX DECLARATION:
// Pin D3 = GPIO0 on Wemos D1 Mini
// Adjust pin if needed for your board
#ifndef D3
  #define D3 0
#endif

// Matrix object and globals are defined in helper_led.cpp
extern Adafruit_NeoMatrix matrix;
extern const uint16_t colors[];
extern bool blinkState;

// External reference to price data
extern struct entsoe_prices PRICES;
extern int getHoursOfDay();

void matrixInitialize();

// Function to remap pixels with horizontal and vertical flipping
// Adjust this if your matrix orientation is different
void drawPixelRemapped(int x, int y, uint16_t color);

// Draw a column with specified height and color
void matrixLine(int column, int height, uint16_t color);

// Show test pattern on startup
void matrixShowTest();

// Show electricity prices on the LED matrix
void matrixShowEntsoe();

// Show a simple animation when WiFi is connecting / portal is active
void matrixShowConnecting();

// Show AP mode indicator - alternating blue 'A' pattern on LED matrix
// Visual: two vertical blue bars that alternate left/right to indicate config mode
void matrixShowAPMode();

// Non-blocking version - call this in loop() without delay
// Set to true when AP mode is active
extern bool apModeActive;
void matrixUpdateAPMode();

// Show an error pattern (all red flash)
void matrixShowError();

// Turn off LED matrix completely (for deep sleep)
// Sets all LEDs to off and powers down the matrix driver
void matrixPowerOff();
void matrixWakeUp();

#endif // HELPER_LED_H

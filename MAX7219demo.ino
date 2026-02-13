// MAX7219 32x8 LED Matrix Demo — Arduino/MD_Parola version
// Target: ESP32-C6
// Libraries: MD_MAX72XX, MD_Parola (install via Arduino Library Manager)
//
// Drives a chain of 4 MAX7219 modules (32x8 LED matrix) with:
//   - Scrolling text display
//   - Static text display (uncomment to use)
//   - Automatic brightness control via photoresistor

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// ---------------------------------------------------------------------------
// Hardware configuration
// ---------------------------------------------------------------------------
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  // Common generic 4-in-1 module
#define MAX_DEVICES   4

// Pin assignments (matching original ESP-IDF project)
#define PIN_CLK   4
#define PIN_DATA  0   // DIN / MOSI
#define PIN_CS    2

#define PIN_PHOTORESISTOR 5  // ADC input for ambient light sensor

// ---------------------------------------------------------------------------
// Display modes — uncomment exactly one
// ---------------------------------------------------------------------------
#define MODE_SCROLLING
// #define MODE_STATIC

// ---------------------------------------------------------------------------
// Display parameters
// ---------------------------------------------------------------------------
#define SCROLL_SPEED    50    // milliseconds per animation frame (lower = faster)
#define SCROLL_PAUSE  2000    // pause at end of scroll (ms)

static const char *MESSAGE_SCROLL = "Hello from Claude!";
static const char *MESSAGE_STATIC = "09:17";

// ---------------------------------------------------------------------------
// Brightness tuning
// ---------------------------------------------------------------------------
// ADC readings from photoresistor (12-bit: 0-4095)
#define ADC_BRIGHT_LIMIT   100   // reading in bright ambient light
#define ADC_DARK_LIMIT    3500   // reading in dark ambient light

// MAX7219 intensity range: 0-15
#define INTENSITY_MIN  0   // dark room  → dimmest
#define INTENSITY_MAX 15   // bright room → brightest

// How often to re-read the photoresistor (ms)
#define BRIGHTNESS_UPDATE_MS 200

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------
// Using software SPI so we can pick arbitrary GPIOs
MD_Parola display = MD_Parola(HARDWARE_TYPE, PIN_DATA, PIN_CLK, PIN_CS, MAX_DEVICES);

unsigned long lastBrightnessUpdate = 0;

// ---------------------------------------------------------------------------
// Map photoresistor reading to MAX7219 intensity (0-15)
// Inverted: high ADC (dark) → low intensity, low ADC (bright) → high intensity
// ---------------------------------------------------------------------------
uint8_t readBrightness() {
  int adc = analogRead(PIN_PHOTORESISTOR);

  // Clamp to expected range
  if (adc < ADC_BRIGHT_LIMIT) adc = ADC_BRIGHT_LIMIT;
  if (adc > ADC_DARK_LIMIT)   adc = ADC_DARK_LIMIT;

  // Map: bright (low ADC) → high intensity, dark (high ADC) → low intensity
  int range_adc = ADC_DARK_LIMIT - ADC_BRIGHT_LIMIT;
  int range_int = INTENSITY_MAX - INTENSITY_MIN;
  uint8_t intensity = INTENSITY_MAX - ((adc - ADC_BRIGHT_LIMIT) * range_int / range_adc);

  return intensity;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("MAX7219 32x8 LED Matrix Demo (MD_Parola)");

  // Configure photoresistor pin (ESP32 ADC defaults to 12-bit: 0-4095)
  pinMode(PIN_PHOTORESISTOR, INPUT);

  // Initialize display
  display.begin();
  display.setIntensity(0);  // start dim; brightness loop will adjust
  display.displayClear();

#ifdef MODE_SCROLLING
  display.displayScroll(MESSAGE_SCROLL, PA_LEFT, PA_SCROLL_LEFT, SCROLL_SPEED);
  Serial.print("Scrolling message: ");
  Serial.println(MESSAGE_SCROLL);
#else
  display.setTextAlignment(PA_CENTER);
  display.print(MESSAGE_STATIC);
  Serial.print("Static message: ");
  Serial.println(MESSAGE_STATIC);
#endif
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
  // --- Brightness update (non-blocking) ---
  unsigned long now = millis();
  if (now - lastBrightnessUpdate >= BRIGHTNESS_UPDATE_MS) {
    lastBrightnessUpdate = now;
    uint8_t intensity = readBrightness();
    display.setIntensity(intensity);
  }

  // --- Display update ---
#ifdef MODE_SCROLLING
  if (display.displayAnimate()) {
    // Animation cycle complete — reset to scroll again
    display.displayReset();
  }
#endif
  // In static mode, nothing to do here; display holds its content.
}

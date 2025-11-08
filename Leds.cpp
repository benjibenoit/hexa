#include "Leds.h"

CRGB leds[NUM_LEDS];

void ledsBegin() {
  delay(200);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS_MAX);
  #if USE_COLOR_TEMP
    FastLED.setTemperature(COLOR_TEMP);
  #endif
}

void ledsShow() {
  #if USE_VIDEO_DITHER
    FastLED.setDither(BINARY_DITHER);
  #endif
  FastLED.show();
}

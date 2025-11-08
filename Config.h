#pragma once

// === Hardware ===
#define LED_PIN         4
#define LED_TYPE        WS2812B
#define COLOR_ORDER     GRB
#define NUM_LEDS        169
#define BRIGHTNESS_MAX  255

// === Global options ===
#define USE_COLOR_TEMP  1
#define COLOR_TEMP      Candle     // Candle, Tungsten40W, Halogen, Neutral, Daylight, Overcast, ClearBlueSky
#define USE_VIDEO_DITHER 1

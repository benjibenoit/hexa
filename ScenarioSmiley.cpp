// ScenarioSmiley.cpp
#include <FastLED.h>
#include "Config.h"
#include "Leds.h"
#include "ScenarioSmiley.h"

static constexpr uint8_t SMILE_BRIGHT = 220;

// Indices à allumer pour le smiley (yeux + bouche)
static const uint16_t SMILE_IDXS[] = {
  41, 46,              // yeux
  115, 122,            // haut de bouche
  138, 137, 136, 135, 134, 127, 109 // courbe de bouche
};
static constexpr uint8_t SMILE_COUNT = sizeof(SMILE_IDXS)/sizeof(SMILE_IDXS[0]);

void ScenarioSmiley::begin() {
  randomSeed(analogRead(A0));
}

void ScenarioSmiley::tick(uint32_t now) {

  // Smiley par-dessus le fond
  for (uint8_t k = 0; k < SMILE_COUNT; ++k) {
    uint16_t idx = SMILE_IDXS[k];
    if (idx >= NUM_LEDS) continue;
    leds[idx] = CRGB(SMILE_BRIGHT, SMILE_BRIGHT, SMILE_BRIGHT);
  }

  ledsShow();
}

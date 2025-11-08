#include "Background.h"
#include <FastLED.h>

// --- Réglages du fond (valeurs en % de BRIGHTNESS_MAX) ---
static constexpr float  BG_MIN_PCT      = 0.05f;   // 5%
static constexpr float  BG_MAX_PCT      = 0.30f;   // 15%
static constexpr uint16_t BG_MIN_MS     = 1000;    // durée min d'une transition
static constexpr uint16_t BG_MAX_MS     = 3000;    // durée max d'une transition

// --- État par LED ---
struct BgLED {
  uint8_t  v0;        // départ (0..255)
  uint8_t  v1;        // cible (0..255)
  uint32_t start;     // ms
  uint16_t dur;       // ms
};
static BgLED bg[NUM_LEDS];

static inline uint8_t pctToByte(float p) {
  float x = p * (float)BRIGHTNESS_MAX;
  if (x < 0) x = 0;
  if (x > 255.f) x = 255.f;
  return (uint8_t)(x + 0.5f);
}

static inline float easeInOutSine(float t) {
  return 0.5f * (1.0f - cosf(3.14159265f * t));
}

static inline uint16_t randDur() {
  return BG_MIN_MS + random(BG_MAX_MS - BG_MIN_MS + 1);
}

static inline uint8_t randLevel() {
  float r = random(0, 10001) / 10000.0f; // 0..1
  float p = BG_MIN_PCT + r * (BG_MAX_PCT - BG_MIN_PCT);
  return pctToByte(p);
}

void backgroundBegin() {
  randomSeed(analogRead(A0));
  uint32_t now = millis();
  for (int i = 0; i < NUM_LEDS; ++i) {
    uint8_t v = randLevel();
    bg[i].v0 = v;
    bg[i].v1 = randLevel();
    bg[i].start = now - random(0, (int)BG_MAX_MS); // déphase
    bg[i].dur = randDur();
  }
}

void backgroundTick(uint32_t now) {
  for (int i = 0; i < NUM_LEDS; ++i) {
    uint32_t t = now - bg[i].start;
    if (t >= bg[i].dur) {
      bg[i].v0 = bg[i].v1;
      bg[i].v1 = randLevel();
      bg[i].start = now;
      bg[i].dur = randDur();
      t = 0;
    }
  }
}

uint8_t backgroundGet(uint16_t index) {
  const BgLED &b = bg[index];
  uint32_t now = millis();
  uint32_t t = now - b.start;
  if (t >= b.dur) t = b.dur;
  float x = (float)t / (float)b.dur;
  float e = easeInOutSine(x);
  int v = (int)((1.0f - e) * b.v0 + e * b.v1);
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return (uint8_t)v;
}

#include <FastLED.h>
#include "Leds.h"
#include "ScenarioCloud.h"
#include "Background.h"

// === Tuning ===
static constexpr uint8_t  ACTIVE_COUNT      = 40;     // nb de LEDs en pulsation simultanées
static constexpr uint16_t PULSE_MIN_MS      = 1200;   // durée min
static constexpr uint16_t PULSE_MAX_MS      = 2600;   // durée max
static constexpr uint8_t  PEAK_BRIGHTNESS   = 220;    // crête d'intensité par pulsation
static constexpr uint16_t COOLDOWN_MS       = 400;    // délai mini réutilisation

// Anti-scintillement / rendu doux (EMA asymétrique)
static constexpr uint8_t  ATTACK_ALPHA_256  = 220;    // montée rapide
static constexpr uint8_t  DECAY_ALPHA_256   = 80;     // descente plus douce

struct Pulse {
  int      idx = -1;
  uint32_t start = 0;
  uint16_t duration = 0;
  bool     inUse = false;
  uint32_t lastUseEnd = 0;
};

static Pulse pulses[ACTIVE_COUNT];
static bool  ledIsActive[NUM_LEDS];
static uint32_t ledLastEnded[NUM_LEDS];

static uint8_t baseVals[NUM_LEDS];    // fond (5..15%) issu de Background
static uint8_t smoothVals[NUM_LEDS];  // sortie lissée (EMA)

static uint8_t clamp8i(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static float easeInOutUpDown(float t) { // 0..1..0
  return sinf(3.14159265f * t);
}

static bool isIndexAlreadyActive(int idx) {
  for (int i = 0; i < ACTIVE_COUNT; ++i)
    if (pulses[i].inUse && pulses[i].idx == idx) return true;
  return false;
}

static int pickRandomAvailableIndex(uint32_t now) {
  for (int attempt = 0; attempt < 40; ++attempt) {
    int idx = random(NUM_LEDS);
    if (isIndexAlreadyActive(idx)) continue;
    if (now - ledLastEnded[idx] < COOLDOWN_MS) continue;
    return idx;
  }
  for (int idx = 0; idx < NUM_LEDS; ++idx)
    if (!isIndexAlreadyActive(idx)) return idx;
  return -1;
}

static uint16_t randDuration() {
  return PULSE_MIN_MS + random(PULSE_MAX_MS - PULSE_MIN_MS + 1);
}

static void startPulse(Pulse &p, uint32_t now) {
  int idx = pickRandomAvailableIndex(now);
  if (idx < 0) { p.inUse = false; return; }
  p.idx      = idx;
  p.start    = now;
  p.duration = randDuration();
  p.inUse    = true;
  ledIsActive[idx] = true;
}

static void endPulse(Pulse &p, uint32_t now) {
  p.inUse = false;
  ledIsActive[p.idx] = false;
  p.lastUseEnd = now;
  ledLastEnded[p.idx] = now;
}

void ScenarioCloud::begin() {
  randomSeed(analogRead(A0));

  for (int i = 0; i < ACTIVE_COUNT; ++i) {
    pulses[i] = Pulse{};
  }
  for (int i = 0; i < NUM_LEDS; ++i) {
    ledIsActive[i] = false;
    ledLastEnded[i] = 0;
    smoothVals[i] = 0;
  }

  backgroundBegin(); // fond global (5..15%) partagé entre scénarios
}

void ScenarioCloud::tick(uint32_t now) {
  // --- Fond global (anime chaque LED entre 5% et 15%) ---
  backgroundTick(now);
  for (int i = 0; i < NUM_LEDS; ++i) {
    baseVals[i] = backgroundGet(i);
  }

  // --- Maintenir ACTIVE_COUNT pulsations actives ---
  int active = 0;
  for (int i = 0; i < ACTIVE_COUNT; ++i) if (pulses[i].inUse) active++;
  for (int i = 0; i < ACTIVE_COUNT; ++i) {
    if (active >= ACTIVE_COUNT) break;
    if (!pulses[i].inUse) { startPulse(pulses[i], now); active++; }
  }

  // --- Construire la cible (target) : max(fond, pulsations) ---
  static uint8_t target[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; ++i) target[i] = baseVals[i];

  for (int i = 0; i < ACTIVE_COUNT; ++i) {
    if (!pulses[i].inUse) continue;
    uint32_t elapsed = now - pulses[i].start;
    if (elapsed >= pulses[i].duration) {
      endPulse(pulses[i], now);
      startPulse(pulses[i], now); // relance immédiate pour garder le compte constant
      continue;
    }
    float t = (float)elapsed / (float)pulses[i].duration; // 0..1
    float e = easeInOutUpDown(t);                         // 0..1..0
    int val = (int)(e * PEAK_BRIGHTNESS);
    int idx = pulses[i].idx;

    // superposer la pulsation par-dessus le fond
    if (val > target[idx]) target[idx] = clamp8i(val);
  }

  // --- Lissage temporel asymétrique (fade rapide à l'allumage, plus doux à l'extinction) ---
  for (int i = 0; i < NUM_LEDS; ++i) {
    uint16_t s   = smoothVals[i];
    uint16_t tar = target[i];
    uint16_t a   = (tar > s) ? ATTACK_ALPHA_256 : DECAY_ALPHA_256;
    uint16_t ns  = ((s * (256 - a)) + (tar * a)) >> 8;
    smoothVals[i] = (uint8_t)ns;
    leds[i] = CRGB(smoothVals[i], smoothVals[i], smoothVals[i]);
  }

  ledsShow();
}

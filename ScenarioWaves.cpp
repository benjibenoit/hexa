#include <FastLED.h>
#include "Config.h"
#include "Leds.h"
#include "ScenarioWaves.h"
#include "Background.h"

// ===== Tuning =====
static constexpr uint8_t  WAVE_PEAK          = 230;   // intensité crête de la tête
static constexpr uint8_t  TRAIL_PEAK         = 180;   // intensité crête au départ de la traîne
static constexpr float    TRAIL_HEX          = 10.0f;  // fade arrière (épaisseurs)
static constexpr float    HEAD_WIDTH         = 1.60f; // largeur de tête (>= 1.0 pour recouvrement)

static constexpr float    FRONT_HEX          = 3.0f;  // fade devant (épaisseurs)
static constexpr float    FRONT_FACTOR       = 0.33f; // % de la crête pour le fade avant

// Motion / cadence
static constexpr uint16_t PER_RING_DELAY     = 110;   // ms pour avancer de ~1 distance hex

// Spawns (plusieurs vagues simultanées)
static constexpr uint16_t WAIT_MIN_MS        = 1000;
static constexpr uint16_t WAIT_MAX_MS        = 5000;
static constexpr uint8_t  WAVE_SLOTS         = 3;

// Anti-scintillement + FADE rapide à l'allumage (EMA asymétrique)
static constexpr uint8_t  ATTACK_ALPHA_256   = 220;   // montée (allumage) -> fade rapide
static constexpr uint8_t  DECAY_ALPHA_256    = 80;    // descente (extinction) -> plus doux

// ===== Grid =====
static constexpr int R = 7;                         // rayon du grand hex (8 par côté -> R=7)
static constexpr int SIDE = 2*R+1;                  // 15
static constexpr int INVALID = -1;

struct Axial { int8_t q, r; };
static Axial coords[NUM_LEDS];
static int16_t idxMap[SIDE][SIDE];

// ===== State =====
static uint8_t baseVals[NUM_LEDS];
static uint8_t smoothVals[NUM_LEDS];   // EMA asymétrique (sortie finale)

struct Wave {
  bool     inUse = false;
  uint32_t start = 0;
  int16_t  seedIdx = -1;
  uint8_t  maxDist = 0;
};
static Wave waves[WAVE_SLOTS];
static uint32_t nextSpawnAt = 0;

// ===== Utils =====
static uint8_t clamp8i(int v){ return v<0?0:(v>255?255:v); }

// Fenêtre cosinus relevée centrée en c, largeur w (en unités de distance hex), renvoie 0..1
static float raisedCosine(float x, float c, float w){
  float d = fabsf(x - c);
  float hw = w * 0.5f;
  if (d >= hw) return 0.0f;
  float t = d / hw;                // 0..1
  return 0.5f * (1.0f + cosf(3.14159265f * t));
}

static inline bool validCoord(int q, int r){
  return (abs(q) <= R) && (abs(r) <= R) && (abs(q + r) <= R);
}
static int idxOf(int q, int r){
  if (!validCoord(q, r)) return INVALID;
  return idxMap[q + R][r + R];
}
static uint8_t hexDistance(const Axial& a, const Axial& b){
  int dq = a.q - b.q;
  int dr = a.r - b.r;
  int ds = -(a.q + a.r) - (-(b.q + b.r));
  dq = abs(dq); dr = abs(dr); ds = abs(ds);
  return (uint8_t)((dq + dr + ds) / 2);
}

static void buildGridMapping(){
  for (int i = 0; i < SIDE; ++i)
    for (int j = 0; j < SIDE; ++j)
      idxMap[i][j] = INVALID;

  struct Tile { int q, r; };
  Tile tiles[NUM_LEDS];
  int n = 0, rowIdx = 0;
  int rowStart[2*R+1];
  int rowLen[2*R+1];

  for (int r = -R; r <= R; ++r) {
    int q1 = max(-R, -r - R);
    int q2 = min(R,  -r + R);
    rowStart[rowIdx] = n;
    rowLen[rowIdx]   = q2 - q1 + 1;
    for (int q = q1; q <= q2; ++q) tiles[n++] = {q, r};
    rowIdx++;
  }
  for (int i = 0; i < rowIdx; ++i) {
    if (i % 2 == 1) {
      int s = rowStart[i], e = s + rowLen[i] - 1;
      while (s < e) { Tile t = tiles[s]; tiles[s] = tiles[e]; tiles[e] = t; s++; e--; }
    }
  }
  for (int i = 0; i < NUM_LEDS; ++i) {
    coords[i].q = (int8_t)tiles[i].q;
    coords[i].r = (int8_t)tiles[i].r;
    idxMap[tiles[i].q + R][tiles[i].r + R] = i;
  }
}

static uint8_t maxDistanceToEdge(int seed){
  uint8_t md = 0;
  for (int i = 0; i < NUM_LEDS; ++i) {
    uint8_t d = hexDistance(coords[i], coords[seed]);
    if (d > md) md = d;
  }
  return md;
}

// --- Choix biaisé du délai entre vagues : plus petit => moins probable ---
static uint32_t weightedRandomWait() {
  float r = random(0, 10001) / 10000.0f;   // 0..1
  const float power = 2.2f;                // >1 => biais vers WAIT_MAX
  float biased = powf(r, power);           // r^power
  uint32_t val = WAIT_MIN_MS + (uint32_t)((WAIT_MAX_MS - WAIT_MIN_MS) * biased);
  return val;
}

static void trySpawnWave(uint32_t now){
  for (uint8_t i = 0; i < WAVE_SLOTS; ++i) {
    if (!waves[i].inUse) {
      waves[i].inUse   = true;
      waves[i].start   = now - (uint16_t)random(0, PER_RING_DELAY/2 + 1);
      waves[i].seedIdx = random(NUM_LEDS);
      waves[i].maxDist = maxDistanceToEdge(waves[i].seedIdx);
      nextSpawnAt = now + weightedRandomWait();
      return;
    }
  }
  nextSpawnAt = now + weightedRandomWait();
}

// ===== Public API =====
void ScenarioWaves::begin() {
  buildGridMapping();
  randomSeed(analogRead(A0));
  for (uint8_t i = 0; i < WAVE_SLOTS; ++i) waves[i] = Wave{};
  for (int i = 0; i < NUM_LEDS; ++i) smoothVals[i] = 0;
  backgroundBegin(); // initialise le fond
  nextSpawnAt = millis() + weightedRandomWait();
}

void ScenarioWaves::tick(uint32_t now){
  // 1) Fond global (via Background)
  backgroundTick(now);
  for (int i = 0; i < NUM_LEDS; ++i) {
    baseVals[i] = backgroundGet(i);
  }

  // 2) Spawns
  if ((int32_t)(now - nextSpawnAt) >= 0) {
    trySpawnWave(now);
  }

  // 3) Target brut (max des vagues + fond)
  static uint8_t target[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; ++i) target[i] = baseVals[i];

  for (uint8_t w = 0; w < WAVE_SLOTS; ++w) {
    if (!waves[w].inUse) continue;

    uint32_t t = now - waves[w].start;
    float head = (float)t / (float)PER_RING_DELAY; // position radiale continue

    if (head > (float)waves[w].maxDist + TRAIL_HEX + (HEAD_WIDTH*0.5f)) {
      waves[w].inUse = false;
      continue;
    }

    const Axial seed = coords[waves[w].seedIdx];

    for (int i = 0; i < NUM_LEDS; ++i) {
      float d = (float)hexDistance(coords[i], seed);

      float wHead  = raisedCosine(d, head, HEAD_WIDTH);  // 0..1
      int   vHead  = (int)(wHead * (float)WAVE_PEAK);

      float deltaB = head - d;                            // >0 derrière la tête
      float wTrail = 0.0f;
      if (deltaB > 0.0f && deltaB <= TRAIL_HEX) {
        float tnorm = 1.0f - (deltaB / TRAIL_HEX);        // 1 à la tête -> 0 au bout
        wTrail = tnorm * tnorm;                           // quadratique douce
      }
      int vTrail = (int)(wTrail * (float)TRAIL_PEAK);

      float deltaF = d - head;                            // >0 devant la tête
      float wFront = 0.0f;
      if (deltaF > 0.0f && deltaF <= FRONT_HEX) {
        float tnormF = 1.0f - (deltaF / FRONT_HEX);       // 1 -> 0
        wFront = tnormF * tnormF;                         // fade doux
      }
      int vFront = (int)(wFront * (float)(WAVE_PEAK * FRONT_FACTOR));

      uint8_t v = (uint8_t)max((int)target[i], max(vHead, max(vTrail, vFront)));
      target[i] = v;
    }
  }

  // 4) EMA asymétrique (fade rapide à l'allumage, plus doux à l'extinction)
  for (int i = 0; i < NUM_LEDS; ++i) {
    uint16_t s   = smoothVals[i];
    uint16_t tar = target[i];
    uint16_t a   = (tar > s) ? ATTACK_ALPHA_256 : DECAY_ALPHA_256; // montée vs descente
    uint16_t ns  = ((s * (256 - a)) + (tar * a)) >> 8;
    smoothVals[i] = (uint8_t)ns;
    leds[i] = CRGB(smoothVals[i], smoothVals[i], smoothVals[i]);
  }

  // 5) Affichage
  ledsShow();
}

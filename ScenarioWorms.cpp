#include <FastLED.h>
#include "Config.h"
#include "Leds.h"
#include "ScenarioWorms.h"

// ===== Tuning =====
static constexpr uint8_t  BASE_MIN         = 6;    // lueur de fond min
static constexpr uint8_t  BASE_MAX         = 20;   // lueur de fond max
static constexpr uint16_t NOISE_SPEED_MS   = 45;
static constexpr uint8_t  NOISE_SCALE      = 13;

static constexpr uint8_t  WORMS_PEAK        = 220;  // amplitude de crête de la vague
static constexpr uint16_t PER_STEP_DELAY   = 95;   // délai par pas le long de la direction
static constexpr uint16_t LOCAL_PULSE_MS   = 700;  // durée locale montée+descente par LED

// spawn aléatoire de vagues (plusieurs en même temps)
static constexpr uint16_t WAIT_MIN_MS      = 100;  // temps entre spawns
static constexpr uint16_t WAIT_MAX_MS      = 500;
static constexpr uint8_t  WORMS_SLOTS       = 8;    // nb maximum de vagues simultanées

static constexpr int R = 7;                        // rayon du grand hex (8 par côté -> R=7)
static constexpr int SIDE = 2*R+1;                 // 15
static constexpr int INVALID = -1;

// ===== Storage =====
struct Axial { int8_t q, r; };
static Axial coords[NUM_LEDS];
static int16_t idxMap[SIDE][SIDE]; // map (q+R, r+R) -> index or -1
static uint8_t baseVals[NUM_LEDS];

// vague = un rayon 1 LED de large le long d'une unique direction
struct Worms {
  bool     inUse = false;
  uint32_t start = 0;
  int16_t  seedIdx = -1;
  uint8_t  dir = 0;       // 0..5
};
static Worms worms[WORMS_SLOTS];
static uint32_t nextSpawnAt = 0;

// ===== Utils =====
static uint8_t clamp8i(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static float easeInOutUpDown(float t) { return sinf(3.14159265f * t); } // 0..1..0

// directions (axial, flat-top): E, NE, NW, W, SW, SE
static const int8_t DIRS[6][2] = { {1,0},{1,-1},{0,-1},{-1,0},{-1,1},{0,1} };

static inline bool validCoord(int q, int r) {
  return (abs(q) <= R) && (abs(r) <= R) && (abs(q + r) <= R);
}

static int idxOf(int q, int r) {
  if (!validCoord(q, r)) return INVALID;
  return idxMap[q + R][r + R];
}

// renvoie l'index du voisin dans la direction 'dir' (ou -1 si en dehors)
static int neighborIdx(int idx, uint8_t dir) {
  const Axial a = coords[idx];
  int q = (int)a.q + DIRS[dir][0];
  int r = (int)a.r + DIRS[dir][1];
  return idxOf(q, r);
}

// renvoie l'index situé à 'steps' pas dans la direction 'dir' à partir de 'seedIdx'
static int stepIdx(int seedIdx, uint8_t dir, uint16_t steps) {
  int cur = seedIdx;
  for (uint16_t k = 0; k < steps; ++k) {
    cur = neighborIdx(cur, dir);
    if (cur == INVALID) return INVALID;
  }
  return cur;
}

static void buildGridMapping() {
  // Clear map
  for (int i = 0; i < SIDE; ++i)
    for (int j = 0; j < SIDE; ++j)
      idxMap[i][j] = INVALID;

  // Generate axial coords in hex radius R
  // Rows ordered by r asc (top to bottom), left->right by q asc, serpentin: odd row index reversed
  int n = 0;
  struct Tile { int q, r; };
  Tile tiles[NUM_LEDS];
  int rowStart[2*R+1];  // offsets per row in tiles[]
  int rowLen[2*R+1];

  int rowIdx = 0;
  for (int r = -R; r <= R; ++r) {
    int q1 = max(-R, -r - R);
    int q2 = min(R,  -r + R);
    rowStart[rowIdx] = n;
    rowLen[rowIdx] = q2 - q1 + 1;
    for (int q = q1; q <= q2; ++q) {
      tiles[n++] = {q, r};
    }
    rowIdx++;
  }

  // Apply serpentine: reverse each odd row index
  for (int i = 0; i < rowIdx; ++i) {
    if (i % 2 == 1) {
      int s = rowStart[i];
      int e = s + rowLen[i] - 1;
      while (s < e) { Tile tmp = tiles[s]; tiles[s] = tiles[e]; tiles[e] = tmp; s++; e--; }
    }
  }

  // Write coords[] and idxMap
  for (int i = 0; i < NUM_LEDS; ++i) {
    coords[i].q = (int8_t)tiles[i].q;
    coords[i].r = (int8_t)tiles[i].r;
    idxMap[tiles[i].q + R][tiles[i].r + R] = i;
  }
}

// tente de démarrer une nouvelle vague si un slot est libre
static void trySpawnWorm(uint32_t now) {
  for (uint8_t i = 0; i < WORMS_SLOTS; ++i) {
    if (!worms[i].inUse) {
      worms[i].inUse  = true;
      worms[i].start  = now;
      worms[i].seedIdx= random(NUM_LEDS);
      worms[i].dir    = random(6);
      // programme le prochain spawn
      nextSpawnAt = now + (WAIT_MIN_MS + random(WAIT_MAX_MS - WAIT_MIN_MS + 1));
      return;
    }
  }
  // si aucun slot libre, reporte simplement le prochain spawn
  nextSpawnAt = now + (WAIT_MIN_MS + random(WAIT_MAX_MS - WAIT_MIN_MS + 1));
}

void ScenarioWorms::begin() {
  buildGridMapping();
  randomSeed(analogRead(A0));
  for (uint8_t i = 0; i < WORMS_SLOTS; ++i) worms[i] = Worms{};
  nextSpawnAt = millis() + (WAIT_MIN_MS + random(WAIT_MAX_MS - WAIT_MIN_MS + 1));
}

void ScenarioWorms::tick(uint32_t now) {
  // lueur de fond
  for (int i = 0; i < NUM_LEDS; ++i) {
    uint8_t n = inoise8(i * NOISE_SCALE, now / NOISE_SPEED_MS);
    int base = BASE_MIN + ((int)(BASE_MAX - BASE_MIN) * n) / 255;
    baseVals[i] = clamp8i(base);
    leds[i] = CRGB(baseVals[i], baseVals[i], baseVals[i]);
  }

  // spawn de nouvelles vagues
  if ((int32_t)(now - nextSpawnAt) >= 0) {
    trySpawnWorm(now);
  }

  // rendu des vagues existantes
  for (uint8_t w = 0; w < WORMS_SLOTS; ++w) {
    if (!worms[w].inUse) continue;

    uint32_t t = now - worms[w].start;
    // l'indice le plus avancé potentiellement actif = floor(t / PER_STEP_DELAY)
    uint16_t maxStep = (uint16_t)(t / PER_STEP_DELAY);

    bool anyActive = false;
    // Parcourt le rayon le long de la direction (1 LED de large)
    for (uint16_t k = 0; k <= maxStep + 1; ++k) {
      int idx = stepIdx(worms[w].seedIdx, worms[w].dir, k);
      if (idx == INVALID) break;

      // temps local sur cette LED
      int32_t tau = (int32_t)t - (int32_t)k * (int32_t)PER_STEP_DELAY;
      if (tau < 0) continue;
      if (tau > LOCAL_PULSE_MS) continue;

      anyActive = true;
      float x = (float)tau / (float)LOCAL_PULSE_MS; // 0..1
      float e = easeInOutUpDown(x);                 // 0..1..0
      int val = (int)(e * WORMS_PEAK);
      int combined = max((int)baseVals[idx], val);
      leds[idx] = CRGB(combined, combined, combined);
    }

    // fin de la vague si plus aucune LED du rayon n'est active
    if (!anyActive) {
      worms[w].inUse = false;
    }
  }

  ledsShow();
}

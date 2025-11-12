// main.ino — démarre sur Background animé, clic bouton => fait défiler les scénarios

#include <FastLED.h>
#include "Config.h"
#include "Leds.h"
#include "Scenario.h"
#include "ScenarioCloud.h"
#include "ScenarioWaves.h"
#include "ScenarioSmiley.h"
#include "Background.h"
#include "Button.h"

static ScenarioCloud  scCloud;
static ScenarioWaves  scWaves;
static ScenarioSmiley scSmiley;

static Scenario* scenarios[] = {
  &scCloud,
  &scWaves,
  &scSmiley,
};
static const uint8_t NUM_SCEN = sizeof(scenarios)/sizeof(scenarios[0]);
static uint8_t curIdx = 0;
static Scenario* current = nullptr;

static Button btn;

enum class Mode : uint8_t { BACKGROUND_ONLY, SCENARIO };
static Mode mode = Mode::BACKGROUND_ONLY;

void setup() {
  // Optionnel pour debug:
  // Serial.begin(115200);

  ledsBegin();

  // Pull-up interne -> bouton entre PIN_BUTTON et GND
  btn.begin(PIN_BUTTON, /*usePullup=*/true);

  // Démarre uniquement le fond
  backgroundBegin();
  mode = Mode::BACKGROUND_ONLY;
  current = nullptr;
  curIdx = 0;

  FastLED.clear(true);
}

void loop() {
  uint32_t now = millis();
  btn.tick(now);

  if (mode == Mode::BACKGROUND_ONLY) {
    if (btn.clicked()) {
      mode = Mode::SCENARIO;
      current = scenarios[curIdx];
      FastLED.clear(true);
      current->begin();
      // Serial.println(F("-> SCENARIO mode"));
    } else {
      backgroundTick(now);
      for (int i = 0; i < NUM_LEDS; ++i) {
        uint8_t v = backgroundGet(i);  // 5..15% animé
        leds[i] = CRGB(v, v, v);
      }
      ledsShow();
    }
    // Sur ESP8266, éviter le WDT:
    // yield();
    return;
  }

  // Mode scénarios : clique => suivant
  if (btn.clicked()) {
    curIdx = (curIdx + 1) % NUM_SCEN;
    current = scenarios[curIdx];
    FastLED.clear(true);
    current->begin();
    // Serial.print(F("Scenario idx=")); Serial.println(curIdx);
  }

  current->tick(now);
  // yield();
}

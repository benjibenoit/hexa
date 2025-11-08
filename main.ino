#include <FastLED.h>
#include "Config.h"
#include "Leds.h"
#include "Scenario.h"
#include "ScenarioWaves.h"
// #include "ScenarioCloud.h"
// #include "ScenarioWorms.h"

ScenarioWaves scenarioWaves;
// ScenarioCloud scenarioCloud;
// ScenarioWorms scenarioWorms;

Scenario* current = &scenarioWaves;

void setup() {
  ledsBegin();
  current->begin();
}

void loop() {
  current->tick(millis());
}

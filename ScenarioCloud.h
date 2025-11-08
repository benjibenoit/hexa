#pragma once
#include "Scenario.h"

// Nuage de LEDs : lueur de fond + pulsations aléatoires (fade in/out)
class ScenarioCloud : public Scenario {
public:
  void begin() override;
  void tick(uint32_t now) override;
};

#pragma once
#include "Scenario.h"

class ScenarioWaves : public Scenario {
public:
  void begin() override;
  void tick(uint32_t now) override;
};

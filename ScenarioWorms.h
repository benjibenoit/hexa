#pragma once
#include "Scenario.h"

class ScenarioWorms : public Scenario {
public:
  void begin() override;
  void tick(uint32_t now) override;
};

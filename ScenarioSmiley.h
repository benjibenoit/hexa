// ScenarioSmiley.h
#pragma once
#include "Scenario.h"

class ScenarioSmiley : public Scenario {
public:
  void begin() override;
  void tick(uint32_t now) override;
};

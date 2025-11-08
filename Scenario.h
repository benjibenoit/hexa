#pragma once
#include <Arduino.h>

class Scenario {
public:
  virtual ~Scenario() {}
  virtual void begin() = 0;                 // called once in setup()
  virtual void tick(uint32_t now) = 0;      // called every loop()
};

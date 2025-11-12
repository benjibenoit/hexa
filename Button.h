#pragma once
#include <Arduino.h>

class Button {
public:
  void begin(uint8_t pin, bool usePullup = true);
  void tick(uint32_t now);
  bool clicked();  // true exactly once per press-release

private:
  uint8_t  _pin = 0xFF;
  bool     _pullup = true;
  bool     _lastStable = true;   // stable level (true = released on pullup wiring)
  bool     _lastRead   = true;   // last raw read
  uint32_t _lastChange = 0;      // ms
  uint16_t _debounceMs = 30;     // debounce time
  bool     _armed = false;       // armed after press to report click on release
  bool     _clickFlag = false;   // latched until read by clicked()
};

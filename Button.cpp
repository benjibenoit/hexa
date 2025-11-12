// Button.cpp
#include "Button.h"

void Button::begin(uint8_t pin, bool usePullup) {
  _pin = pin;
  _pullup = usePullup;
  if (_pullup) pinMode(_pin, INPUT_PULLUP);
  else         pinMode(_pin, INPUT);
  _lastStable = _pullup ? HIGH : LOW;
  _lastRead   = _lastStable;
  _lastChange = millis();
  _armed = false;
  _clickFlag = false;
}

void Button::tick(uint32_t now) {
  if (_pin == 0xFF) return;
  bool raw = digitalRead(_pin);
  if (raw != _lastRead) {
    _lastRead = raw;
    _lastChange = now;
  }
  if ((now - _lastChange) >= _debounceMs) {
    if (raw != _lastStable) {
      _lastStable = raw;
      // active level is LOW when using pullup wiring
      bool pressed  = _pullup ? (_lastStable == LOW) : (_lastStable == HIGH);
      bool released = !pressed;
      if (pressed)  _armed = true;
      if (released && _armed) { _clickFlag = true; _armed = false; }
    }
  }
}

bool Button::clicked() {
  bool f = _clickFlag;
  _clickFlag = false;
  return f;
}

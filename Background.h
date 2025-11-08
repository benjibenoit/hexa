#pragma once
#include <Arduino.h>
#include "Config.h"

void backgroundBegin();
void backgroundTick(uint32_t now);
uint8_t backgroundGet(uint16_t index);

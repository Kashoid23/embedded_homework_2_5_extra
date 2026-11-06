#pragma once

#include <stdbool.h>
#include <stdint.h>

// Starts the hardware timer that reads the encoder
void encoderStart(void);

// Clicks since start: + one direction, - the other
int32_t encoderSteps(void);

// True once per press of the encoder button
bool encoderButtonPressed(void);

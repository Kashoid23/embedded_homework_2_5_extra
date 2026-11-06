#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t hz;  // 0 = pause
    uint16_t ms;
} Note;

void buzzerStart(uint32_t hz);
void buzzerSetFrequency(uint32_t hz);
void buzzerSetSound(bool on);

// Plays notes one by one, then returns to the given frequency
void buzzerPlay(const Note *notes, size_t count, uint32_t backToHz);

#pragma once
#include "utils.h"

void PersonalBest_load(void);
int PersonalBest_get(int width, int height, int mines);
// Memory-only update. Safe to call from reveal recursion (no file I/O).
// Returns true if this result is a new personal best.
bool PersonalBest_record(int width, int height, int mines, int deciseconds);
// Write pending PB changes to MINESPB.DAT. Call only outside reveal/board recursion.
bool PersonalBest_flush(void);

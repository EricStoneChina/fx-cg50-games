#pragma once
#include "utils.h"

void PersonalBest_load(void);
// Returns -1 when this board configuration has no saved record.
int PersonalBest_get(int width, int height, int mines);
// Memory-only update. Safe to call from reveal recursion (no file I/O).
// Returns true if this result is a new personal best.
bool PersonalBest_record(int width, int height, int mines, int deciseconds);
// Flush outside reveal recursion; roll back the staged record on failure.
bool PersonalBest_flush(void);

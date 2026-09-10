#pragma once
#include "utils.h"

void PersonalBest_load(void);
int PersonalBest_get(int width, int height, int mines);
bool PersonalBest_record(int width, int height, int mines, int deciseconds);

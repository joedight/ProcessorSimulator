#pragma once

#include <signal.h>

#include "pipeline.h"

static volatile sig_atomic_t debugger_pause = 1;

struct breakpoint {
	word_u addr;
//	struct list_head list;
};

void debugger_print(const state_t *next, const char *arg);

int debugger(state_t *next, const uint8_t *mem /*, struct list_head breakpoints */);


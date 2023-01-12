#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#define A_DBG_OP_BREAK 1
#define A_DBG_OP_QUIT 2
#define A_DBG_OP_ABORT 3
#define A_DBG_OP_PRINT 4
#define A_DBG_OP_BENCH_BEGIN 5
#define A_DBG_OP_BENCH_END 6
#define A_DBG_OP_INPUT 7

enum dbg_op {
	DBG_OP_BREAK = A_DBG_OP_BREAK,
	DBG_OP_QUIT = A_DBG_OP_QUIT,
	DBG_OP_ABORT = A_DBG_OP_ABORT,
	DBG_OP_PRINT = A_DBG_OP_PRINT,
	DBG_OP_BENCH_BEGIN = A_DBG_OP_BENCH_BEGIN,
	DBG_OP_BENCH_END = A_DBG_OP_BENCH_END,
	DBG_OP_INPUT = A_DBG_OP_INPUT,
};

#define _STR(x) #x
#define STR(x) _STR(x)

static inline void SIM_BREAK()
{
	asm __volatile__("addi t3, zero, " STR(A_DBG_OP_BREAK) "; ebreak" ::: "t3");
}

static inline void SIM_QUIT()
{
	asm __volatile__("addi t3, zero, " STR(A_DBG_OP_QUIT) "; ebreak" ::: "t3");
}

static inline void SIM_ASSERT(bool x)
{
	if (!x) asm __volatile__("addi t3, zero, " STR(A_DBG_OP_ABORT) "; ebreak" ::: "t3");
}

static inline void SIM_PRINT(volatile const char *msg)
{
	asm __volatile__("addi t3, zero, " STR(A_DBG_OP_PRINT) "; addi t4, %[n], 0; ebreak" : : [n] "r" (msg) : "t3", "t4");
}

#define SIM_BENCH_BEGIN(name) \
{ \
	SIM_PRINT("Bench name: " name); \
	asm __volatile__("addi t3, zero, " STR(A_DBG_OP_BENCH_BEGIN) "; ebreak" ::: "t3"); \
}

static inline void SIM_BENCH_END()
{
	asm __volatile__("addi t3, zero, " STR(A_DBG_OP_BENCH_END) "; ebreak" ::: "t3");
}

static inline void SIM_INPUT(char* ch)
{
	asm __volatile__("addi t3, zero, " STR(A_DBG_OP_INPUT) "; ebreak; addi %[c], t3, 0" : [c] "=r" (*ch) :: "t3");
}


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "isa.h"

struct game_state {
	bool valid;
	int best_score;
	int best_move;
	struct game_state *children;
};

static inline int winning_config(int board[9])
{
	int x;
	if ((x = board[0])) {
		if (
			(board[1] == x && board[2] == x) ||
			(board[3] == x && board[6] == x) ||
			(board[4] == x && board[8] == x)
		)
			return x;
	}
	if ((x = board[3]) && board[4] == x && board[5] == x) {
		return x;
	}
	if ((x = board[6])) {
		if (
			(board[4] == x && board[2] == x) ||
			(board[7] == x && board[8] == x)
		)
			return x;
	}
	if ((x = board[1]) && board[4] == x && board[7] == x)
		return x;
	if ((x = board[2]) && board[5] == x && board[8] == x)
		return x;

	if (
		board[0] != 0 && board[1] != 0 && board[2] != 0 &&
		board[3] != 0 && board[4] != 0 && board[5] != 0 &&
		board[6] != 0 && board[7] != 0 && board[8] != 0
	)
		return 0;

	return 100;
}

struct game_state build_tree(int player)
{
	static int board[9];
	int x;
	if ((x = winning_config(board)) != 100) {
		if (player == 1)
			return (struct game_state) { .valid = 1, .best_score =  x, .children = NULL };
		else
			return (struct game_state) { .valid = 1, .best_score =  -x, .children = NULL };
	}
	struct game_state s = { 
		.valid = 1,
		.best_score = -10,
		.best_move = 0,
		.children = calloc(9, sizeof(struct game_state)),
	};
	for (int i = 0; i < 9; i++) {
		if (board[i] == 0) {
			board[i] = player;
			s.children[i] = build_tree(- player);
			board[i] = 0;
			int score = - s.children[i].best_score;
			if (score > s.best_score) {
				s.best_move = i;
				s.best_score = score;
			}
		}
	}
	return s;
}

int main(int argc, char **argv)
{
	SIM_BENCH_BEGIN("Noughts_crosses");
	struct game_state tree = build_tree(1);
	SIM_BENCH_END();
	bool player = 1;
	char grid[9] = {
		' ', ' ', ' ',
		' ', ' ', ' ',
		' ', ' ', ' '
	};
	while (1) {
		volatile char str[10];
		strcpy((char*)str, "_ | _ | _");
		for (int i = 0, j = 0; i < 3; i++) {
			str[0] = grid[j++];
			str[4] = grid[j++];
			str[8] = grid[j++];
			SIM_PRINT(str);
			if (i != 2)
				SIM_PRINT("---------");
		}

		SIM_PRINT("\n");

		if (tree.children == NULL) {
			if (player)
				SIM_PRINT("You win");
			else
				SIM_PRINT("Computer wins");
			SIM_QUIT();
		}
		player = !player;
		if (player) {
			int pos = 100;
			do {
				SIM_PRINT("Input move (1-9): \n");
				char c;
				SIM_INPUT(&c);
				pos = c - '1';
			} while (pos < 0 || pos > 8 || !tree.children[pos].valid);
			tree = tree.children[pos];
			grid[pos] = 'O';
		} else {
			grid[tree.best_move] = 'X';
			volatile char s[28];
			//         1234567890123456789012345678
			strcpy((char*)s, "Computer selects position _");
			s[26] = '1' + tree.best_move;
			tree = tree.children[tree.best_move];
			SIM_PRINT(s);
		}

		SIM_ASSERT(tree.valid);
	}
/*
//	SIM_PRINT("Bench: ackermann");
//	uint32_t x;


//	volatile char str[128];
	//           0123456789
//	strcpy(str, "Got char _.");
	///str[9] = (char)c;

	volatile uint32_t c;
	SIM_INPUT(c);
	volatile char str[128] = {'', 'a', 'a', '\0'};
	str[0] = (char)c;

	SIM_PRINT(str);
	SIM_BREAK();

	uint32_t x;

	x = ackermann(2, 2);
	SIM_ASSERT(x == 7);
	x = ackermann(2, 4);
	SIM_ASSERT(x == 11);
	x = ackermann(3, 1);
	SIM_ASSERT(x == 13);
	x = ackermann(3, 4);
	SIM_ASSERT(x == 125);

	SIM_BENCH_BEGIN();
	x = ackermann(3, 8);
	SIM_ASSERT(x == 2045);
	SIM_BENCH_END();

//	SIM_PRINT(str);


	//SIM_PRINTF("2, 2 is %d", x);
	SIM_QUIT();
*/
}

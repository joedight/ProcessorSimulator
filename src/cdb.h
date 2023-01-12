#pragma once

#include "config.h"
#include "word.h"

typedef struct {
	size_t rob_id;
	word_u data;
	bool exception;
} cdb_entry;

struct cdb {
	cdb_entry buffer[CDB_WIDTH];
};

/* Any free CDB. */
cdb_entry *cdb_find_free(struct cdb *cdb);

/* Any allocated CDB with this ROB id. */
const cdb_entry *cdb_with_rob(const struct cdb *cdb, size_t rob_id);

/* Clear the CDB. */
void cdb_clear(struct cdb *cdb);


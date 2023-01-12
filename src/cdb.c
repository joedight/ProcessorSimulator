#include "cdb.h"

#include <assert.h>

cdb_entry *cdb_find_free(struct cdb *cdb)
{
	for (size_t i = 0; i < CDB_WIDTH; i++) {
		if (cdb->buffer[i].rob_id == 0) {
			assert(cdb->buffer[i].data.u == 0);
			return &cdb->buffer[i];
		}
	}
	return NULL;
}

const cdb_entry *cdb_with_rob(const struct cdb *cdb, size_t rob_id)
{
	assert(rob_id);
	for (size_t i = 0; i < CDB_WIDTH; i++) {
		if (cdb->buffer[i].rob_id == rob_id) {
			return &cdb->buffer[i];
		}
	}
	return NULL;
}

void cdb_clear(struct cdb *cdb)
{
	for (size_t i = 0; i < CDB_WIDTH; i++) {
		cdb->buffer[i] = (cdb_entry){ 0 };
	}
}

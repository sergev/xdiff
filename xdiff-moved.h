/*
 * xdiff-moved.h - Moved block detection for xdiff
 * Similar to git diff --color-moved
 */

#ifndef XDIFF_MOVED_H
#define XDIFF_MOVED_H

#include <stddef.h>

#include "xdiff.h"

/* Move detection modes */
enum moved_mode {
    MOVED_MODE_NO = 0,
    MOVED_MODE_PLAIN,
    MOVED_MODE_BLOCKS,
    MOVED_MODE_ZEBRA,
    MOVED_MODE_DIMMED_ZEBRA
};

/* Whitespace handling modes for move detection */
enum moved_ws_mode {
    MOVED_WS_NO = 0,
    MOVED_WS_IGNORE_ALL,
    MOVED_WS_IGNORE_CHANGE,
    MOVED_WS_IGNORE_AT_EOL
};

/* Represents a block of lines */
struct moved_block {
    long start_line;    /* Starting line number (1-based) */
    long end_line;      /* Ending line number (1-based) */
    unsigned long hash; /* Hash value of block content */
    int is_deleted;     /* 1 if deleted block, 0 if added block */
    int matched;        /* 1 if matched to opposite block, 0 otherwise */
    long match_line;    /* Line number of matched block in opposite file */
    int zebra_index;    /* Index for zebra coloring */
    struct moved_block *next;
};

/* Move detection context */
struct moved_context {
    enum moved_mode mode;
    enum moved_ws_mode ws_mode;
    long min_block_size; /* Minimum alphanumeric characters for blocks mode */
    struct moved_block *deleted_blocks;
    struct moved_block *added_blocks;
    int zebra_counter; /* Counter for zebra mode */
};

/* Initialize move detection context */
void moved_context_init(struct moved_context *ctx, enum moved_mode mode,
                        enum moved_ws_mode ws_mode);

/* Free move detection context */
void moved_context_free(struct moved_context *ctx);

/* Collect blocks from diff changes */
int collect_blocks_from_diff(mmfile_t *mf1, mmfile_t *mf2, xpparam_t const *xpp,
                             struct moved_context *ctx);

/* Check if a line is marked as moved */
int is_line_moved(struct moved_context *ctx, long line_num, int is_deleted);

/* Get zebra index for a moved line */
int get_moved_zebra_index(struct moved_context *ctx, long line_num, int is_deleted);

/* Get dimmed status for a moved line (for dimmed-zebra mode) */
int is_line_dimmed(struct moved_context *ctx, long line_num, int is_deleted);

#endif /* XDIFF_MOVED_H */

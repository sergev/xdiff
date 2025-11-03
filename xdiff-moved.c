/*
 * xdiff-moved.c - Moved block detection for xdiff
 * Similar to git diff --color-moved
 */

#include "xdiff-moved.h"

#include <ctype.h>
#include <string.h>

#include "xinclude.h"

/* Minimum block size for blocks mode (alphanumeric characters) */
#define MIN_BLOCK_SIZE 20

/* DJB2 hash function */
static unsigned long djb2_hash(const char *str, size_t len)
{
    unsigned long hash = 5381;
    size_t i;
    for (i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)str[i];
    }
    return hash;
}

/* Normalize whitespace based on mode */
static void normalize_whitespace(const char *line, size_t len, enum moved_ws_mode ws_mode,
                                 char *out, size_t *out_len)
{
    const char *p = line;
    const char *end = line + len;
    char *q = out;
    int prev_space = 0;

    if (ws_mode == MOVED_WS_IGNORE_ALL) {
        /* Remove all whitespace */
        while (p < end) {
            if (!isspace((unsigned char)*p)) {
                *q++ = *p;
            }
            p++;
        }
    } else if (ws_mode == MOVED_WS_IGNORE_CHANGE) {
        /* Normalize whitespace (multiple spaces -> single space) */
        while (p < end) {
            if (isspace((unsigned char)*p)) {
                if (!prev_space) {
                    *q++ = ' ';
                    prev_space = 1;
                }
            } else {
                *q++ = *p;
                prev_space = 0;
            }
            p++;
        }
    } else if (ws_mode == MOVED_WS_IGNORE_AT_EOL) {
        /* Remove trailing whitespace */
        const char *trailing_start = end;
        while (trailing_start > line && isspace((unsigned char)trailing_start[-1])) {
            trailing_start--;
        }
        memcpy(out, line, trailing_start - line);
        q = out + (trailing_start - line);
    } else {
        /* No normalization */
        memcpy(out, line, len);
        q = out + len;
    }

    *out_len = q - out;
}

/* Count alphanumeric characters in a line */
static long count_alnum(const char *line, size_t len)
{
    long count = 0;
    size_t i;
    for (i = 0; i < len; i++) {
        if (isalnum((unsigned char)line[i])) {
            count++;
        }
    }
    return count;
}

/* Compute hash for a block of lines */
static unsigned long compute_block_hash(xrecord_t **recs, long start, long count,
                                        enum moved_ws_mode ws_mode)
{
    char normalized[4096]; /* Buffer for normalized line */
    size_t normalized_len;
    unsigned long hash = 5381;
    long i;

    for (i = 0; i < count; i++) {
        xrecord_t *rec = recs[start + i];
        normalize_whitespace(rec->ptr, rec->size, ws_mode, normalized, &normalized_len);
        unsigned long line_hash = djb2_hash(normalized, normalized_len);
        /* Combine hashes */
        hash = ((hash << 5) + hash) + line_hash;
    }

    return hash;
}

/* Count total alphanumeric characters in a block */
static long count_block_alnum(xrecord_t **recs, long start, long count)
{
    long total = 0;
    long i;
    for (i = 0; i < count; i++) {
        total += count_alnum(recs[start + i]->ptr, recs[start + i]->size);
    }
    return total;
}

/* Allocate a new moved_block */
static struct moved_block *moved_block_alloc(long start_line, long end_line, unsigned long hash,
                                             int is_deleted)
{
    struct moved_block *block = (struct moved_block *)xdl_malloc(sizeof(struct moved_block));
    if (!block)
        return NULL;

    block->start_line = start_line;
    block->end_line = end_line;
    block->hash = hash;
    block->is_deleted = is_deleted;
    block->matched = 0;
    block->match_line = -1;
    block->zebra_index = -1;
    block->next = NULL;

    return block;
}

/* Free a moved_block */
static void moved_block_free(struct moved_block *block)
{
    if (block) {
        xdl_free(block);
    }
}

/* Free a list of moved_blocks */
static void moved_block_list_free(struct moved_block *head)
{
    struct moved_block *block = head;
    while (block) {
        struct moved_block *next = block->next;
        moved_block_free(block);
        block = next;
    }
}

/* Initialize move detection context */
void moved_context_init(struct moved_context *ctx, enum moved_mode mode, enum moved_ws_mode ws_mode)
{
    ctx->mode = mode;
    ctx->ws_mode = ws_mode;
    ctx->min_block_size = MIN_BLOCK_SIZE;
    ctx->deleted_blocks = NULL;
    ctx->added_blocks = NULL;
    ctx->zebra_counter = 0;
}

/* Free move detection context */
void moved_context_free(struct moved_context *ctx)
{
    moved_block_list_free(ctx->deleted_blocks);
    moved_block_list_free(ctx->added_blocks);
    ctx->deleted_blocks = NULL;
    ctx->added_blocks = NULL;
}

/* Collect blocks from diff changes */
int collect_blocks_from_diff(mmfile_t *mf1, mmfile_t *mf2, xpparam_t const *xpp,
                             struct moved_context *ctx)
{
    xdfenv_t xe;
    xdchange_t *xscr = NULL;
    xdchange_t *xch;
    xrecord_t **recs1;
    xrecord_t **recs2;
    int ret = -1;

    /* Do diff computation */
    if (xdl_do_diff(mf1, mf2, xpp, &xe) < 0)
        return -1;

    /* Build change script */
    if (xdl_change_compact(&xe.xdf1, &xe.xdf2, xpp->flags) < 0 ||
        xdl_change_compact(&xe.xdf2, &xe.xdf1, xpp->flags) < 0 ||
        xdl_build_script(&xe, &xscr) < 0) {
        xdl_free_env(&xe);
        return -1;
    }

    if (!xscr) {
        xdl_free_env(&xe);
        return 0; /* No changes */
    }

    recs1 = xe.xdf1.recs;
    recs2 = xe.xdf2.recs;

    /* Iterate through all changes */
    for (xch = xscr; xch; xch = xch->next) {
        /* Skip ignored changes */
        if (xch->ignore)
            continue;

        /* Process deleted lines (chg1 > 0) */
        if (xch->chg1 > 0) {
            unsigned long hash = compute_block_hash(recs1, xch->i1, xch->chg1, ctx->ws_mode);
            long start_line = xch->i1 + 1; /* Convert to 1-based */
            long end_line = xch->i1 + xch->chg1;

            struct moved_block *block = moved_block_alloc(start_line, end_line, hash, 1);
            if (!block)
                goto cleanup;

            block->next = ctx->deleted_blocks;
            ctx->deleted_blocks = block;
        }

        /* Process added lines (chg2 > 0) */
        if (xch->chg2 > 0) {
            unsigned long hash = compute_block_hash(recs2, xch->i2, xch->chg2, ctx->ws_mode);
            long start_line = xch->i2 + 1; /* Convert to 1-based */
            long end_line = xch->i2 + xch->chg2;

            struct moved_block *block = moved_block_alloc(start_line, end_line, hash, 0);
            if (!block)
                goto cleanup;

            block->next = ctx->added_blocks;
            ctx->added_blocks = block;
        }
    }

    /* Match blocks */
    if (ctx->mode != MOVED_MODE_NO) {
        /* Simple hash-based matching */
        struct moved_block *deleted, *added;

        for (deleted = ctx->deleted_blocks; deleted; deleted = deleted->next) {
            if (deleted->matched)
                continue;

            for (added = ctx->added_blocks; added; added = added->next) {
                if (added->matched)
                    continue;

                if (deleted->hash == added->hash) {
                    deleted->matched = 1;
                    deleted->match_line = added->start_line;
                    added->matched = 1;
                    added->match_line = deleted->start_line;
                    break;
                }
            }
        }

        /* Filter by mode and assign zebra indices */
        if (ctx->mode == MOVED_MODE_BLOCKS || ctx->mode == MOVED_MODE_ZEBRA ||
            ctx->mode == MOVED_MODE_DIMMED_ZEBRA) {
            /* Filter blocks that don't meet minimum size requirement */
            struct moved_block *block;
            struct moved_block *added;

            for (block = ctx->deleted_blocks; block; block = block->next) {
                if (block->matched) {
                    /* Find matching added block to get line count */
                    for (added = ctx->added_blocks; added; added = added->next) {
                        if (added->matched && added->hash == block->hash &&
                            added->match_line == block->start_line) {
                            long alnum = count_block_alnum(recs1, block->start_line - 1,
                                                           block->end_line - block->start_line + 1);
                            if (alnum < ctx->min_block_size) {
                                block->matched = 0;
                                added->matched = 0;
                            }
                            break;
                        }
                    }
                }
            }
        }

        /* Assign zebra indices for zebra modes */
        if (ctx->mode == MOVED_MODE_ZEBRA || ctx->mode == MOVED_MODE_DIMMED_ZEBRA) {
            ctx->zebra_counter = 0;
            struct moved_block *block;
            struct moved_block *added;

            for (block = ctx->deleted_blocks; block; block = block->next) {
                if (block->matched && block->zebra_index == -1) {
                    block->zebra_index = ctx->zebra_counter;
                    /* Find matching added block and assign same index */
                    for (added = ctx->added_blocks; added; added = added->next) {
                        if (added->matched && added->hash == block->hash) {
                            added->zebra_index = ctx->zebra_counter;
                            break;
                        }
                    }
                    ctx->zebra_counter++;
                }
            }
        }
    }

    ret = 0;

cleanup:
    xdl_free_script(xscr);
    xdl_free_env(&xe);
    return ret;
}

/* Check if a line is marked as moved */
int is_line_moved(struct moved_context *ctx, long line_num, int is_deleted)
{
    struct moved_block *block;
    struct moved_block *list = is_deleted ? ctx->deleted_blocks : ctx->added_blocks;

    if (ctx->mode == MOVED_MODE_NO)
        return 0;

    for (block = list; block; block = block->next) {
        if (block->matched && line_num >= block->start_line && line_num <= block->end_line) {
            return 1;
        }
    }

    return 0;
}

/* Get zebra index for a moved line */
int get_moved_zebra_index(struct moved_context *ctx, long line_num, int is_deleted)
{
    struct moved_block *block;
    struct moved_block *list = is_deleted ? ctx->deleted_blocks : ctx->added_blocks;

    if (ctx->mode != MOVED_MODE_ZEBRA && ctx->mode != MOVED_MODE_DIMMED_ZEBRA)
        return -1;

    for (block = list; block; block = block->next) {
        if (block->matched && line_num >= block->start_line && line_num <= block->end_line) {
            return block->zebra_index;
        }
    }

    return -1;
}

/* Get dimmed status for a moved line (for dimmed-zebra mode) */
int is_line_dimmed(struct moved_context *ctx, long line_num, int is_deleted)
{
    struct moved_block *block;
    struct moved_block *list = is_deleted ? ctx->deleted_blocks : ctx->added_blocks;

    if (ctx->mode != MOVED_MODE_DIMMED_ZEBRA)
        return 0;

    for (block = list; block; block = block->next) {
        if (block->matched && line_num >= block->start_line && line_num <= block->end_line) {
            /* Dim interior lines, not first/last */
            if (line_num > block->start_line && line_num < block->end_line) {
                return 1;
            }
        }
    }

    return 0;
}

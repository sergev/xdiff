/*
 * xdiff CLI - Command-line interface for xdiff library
 * Similar to GNU diff
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include "xdiff.h"

/* Forward declarations */
static int read_file(const char *filename, mmfile_t *mf);
static void free_file(mmfile_t *mf);
static int out_hunk_cb(void *priv, long old_begin, long old_nr,
                      long new_begin, long new_nr,
                      const char *func, long funclen);
static int out_line_cb(void *priv, mmbuffer_t *mb, int nb);
static void usage(const char *progname);

/* Context for callbacks */
struct diff_context {
    const char *file1;
    const char *file2;
    int brief;
    int first_hunk;
    int has_differences;
};

/* Read a file into memory */
static int read_file(const char *filename, mmfile_t *mf)
{
    FILE *f;
    long size;
    char *buffer;
    size_t nread;

    f = fopen(filename, "rb");
    if (!f) {
        return -1;
    }

    /* Get file size */
    if (fseek(f, 0, SEEK_END) < 0) {
        fclose(f);
        return -1;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) < 0) {
        fclose(f);
        return -1;
    }

    /* Allocate buffer */
    buffer = (char *)xdl_malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    /* Read file */
    nread = fread(buffer, 1, size, f);
    if (nread != (size_t)size || ferror(f)) {
        xdl_free(buffer);
        fclose(f);
        return -1;
    }
    fclose(f);

    buffer[size] = '\0';
    mf->ptr = buffer;
    mf->size = size;

    return 0;
}

/* Free file memory */
static void free_file(mmfile_t *mf)
{
    if (mf->ptr) {
        xdl_free(mf->ptr);
        mf->ptr = NULL;
        mf->size = 0;
    }
}

/* Hunk callback - prints hunk header */
static int out_hunk_cb(void *priv, long old_begin, long old_nr,
                      long new_begin, long new_nr,
                      const char *func, long funclen)
{
    struct diff_context *ctx = (struct diff_context *)priv;

    /* Mark that we have differences */
    ctx->has_differences = 1;

    if (ctx->brief) {
        return 0;
    }

    if (ctx->first_hunk) {
        printf("--- %s\n", ctx->file1);
        printf("+++ %s\n", ctx->file2);
        ctx->first_hunk = 0;
    }

    printf("@@ -%ld,%ld +%ld,%ld @@", old_begin, old_nr, new_begin, new_nr);
    if (func && funclen > 0) {
        printf(" %.*s", (int)funclen, func);
    }
    printf("\n");

    return 0;
}

/* Line callback - prints diff lines */
static int out_line_cb(void *priv, mmbuffer_t *mb, int nb)
{
    struct diff_context *ctx = (struct diff_context *)priv;
    int i;

    if (ctx->brief) {
        return 0;
    }

    for (i = 0; i < nb; i++) {
        fwrite(mb[i].ptr, 1, mb[i].size, stdout);
    }

    return 0;
}

/* Print usage information */
static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [OPTIONS] FILE1 FILE2\n", progname);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -u, --unified[=N]        Output unified diff format (default: 3 context lines)\n");
    fprintf(stderr, "  -c, --context[=N]        Output context diff format (default: 3 context lines)\n");
    fprintf(stderr, "  -q, --brief             Output only whether files differ\n");
    fprintf(stderr, "  -w, --ignore-all-space  Ignore all whitespace\n");
    fprintf(stderr, "  -b, --ignore-space-change  Ignore whitespace changes\n");
    fprintf(stderr, "  -B, --ignore-blank-lines  Ignore blank lines\n");
    fprintf(stderr, "      --minimal           Produce minimal diff\n");
    fprintf(stderr, "      --patience          Use patience diff algorithm\n");
    fprintf(stderr, "      --histogram         Use histogram diff algorithm\n");
    fprintf(stderr, "  -h, --help              Show this help message\n");
}

int main(int argc, char *argv[])
{
    int opt;
    int option_index = 0;
    long context_lines = 3;
    int brief = 0;
    unsigned long xpp_flags = 0;
    unsigned long emit_flags = 0;
    int algorithm_set = 0;
    mmfile_t mf1, mf2;
    xpparam_t xpp;
    xdemitconf_t xecfg;
    xdemitcb_t ecb;
    struct diff_context ctx;
    int ret = 0;
    const char *file1 = NULL, *file2 = NULL;

    static struct option long_options[] = {
        {"unified", optional_argument, 0, 'u'},
        {"context", optional_argument, 0, 'c'},
        {"brief", no_argument, 0, 'q'},
        {"ignore-all-space", no_argument, 0, 'w'},
        {"ignore-space-change", no_argument, 0, 'b'},
        {"ignore-blank-lines", no_argument, 0, 'B'},
        {"minimal", no_argument, 0, 1},
        {"patience", no_argument, 0, 2},
        {"histogram", no_argument, 0, 3},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    /* Initialize file structures */
    mf1.ptr = NULL;
    mf1.size = 0;
    mf2.ptr = NULL;
    mf2.size = 0;

    /* Parse command-line options */
    while ((opt = getopt_long(argc, argv, "u::c::qwbBh", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'u':
            if (optarg) {
                context_lines = atol(optarg);
                if (context_lines < 0) {
                    fprintf(stderr, "%s: invalid number of context lines\n", argv[0]);
                    return 1;
                }
            }
            break;
        case 'c':
            if (optarg) {
                context_lines = atol(optarg);
                if (context_lines < 0) {
                    fprintf(stderr, "%s: invalid number of context lines\n", argv[0]);
                    return 1;
                }
            }
            break;
        case 'q':
            brief = 1;
            break;
        case 'w':
            xpp_flags |= XDF_IGNORE_WHITESPACE;
            break;
        case 'b':
            xpp_flags |= XDF_IGNORE_WHITESPACE_CHANGE;
            break;
        case 'B':
            xpp_flags |= XDF_IGNORE_BLANK_LINES;
            break;
        case 1: /* --minimal */
            xpp_flags |= XDF_NEED_MINIMAL;
            break;
        case 2: /* --patience */
            if (algorithm_set) {
                fprintf(stderr, "%s: only one diff algorithm can be specified\n", argv[0]);
                return 1;
            }
            xpp_flags |= XDF_PATIENCE_DIFF;
            algorithm_set = 1;
            break;
        case 3: /* --histogram */
            if (algorithm_set) {
                fprintf(stderr, "%s: only one diff algorithm can be specified\n", argv[0]);
                return 1;
            }
            xpp_flags |= XDF_HISTOGRAM_DIFF;
            algorithm_set = 1;
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        case '?':
            return 1;
        default:
            return 1;
        }
    }

    /* Get file arguments */
    if (optind + 2 != argc) {
        fprintf(stderr, "%s: exactly two file arguments required\n", argv[0]);
        usage(argv[0]);
        return 1;
    }

    file1 = argv[optind];
    file2 = argv[optind + 1];

    /* Read files */
    if (read_file(file1, &mf1) < 0) {
        fprintf(stderr, "%s: cannot read file '%s': %s\n", argv[0], file1, strerror(errno));
        ret = 1;
        goto cleanup;
    }

    if (read_file(file2, &mf2) < 0) {
        fprintf(stderr, "%s: cannot read file '%s': %s\n", argv[0], file2, strerror(errno));
        ret = 1;
        goto cleanup;
    }

    /* Configure xdiff parameters */
    memset(&xpp, 0, sizeof(xpp));
    xpp.flags = xpp_flags;

    memset(&xecfg, 0, sizeof(xecfg));
    xecfg.ctxlen = context_lines;
    xecfg.interhunkctxlen = 0;
    xecfg.flags = emit_flags;

    /* Set up callbacks */
    ctx.file1 = file1;
    ctx.file2 = file2;
    ctx.brief = brief;
    ctx.first_hunk = 1;
    ctx.has_differences = 0;

    memset(&ecb, 0, sizeof(ecb));
    ecb.priv = &ctx;
    ecb.out_hunk = out_hunk_cb;
    ecb.out_line = out_line_cb;

    /* Compute diff */
    ret = xdl_diff(&mf1, &mf2, &xpp, &xecfg, &ecb);

    if (ret < 0) {
        fprintf(stderr, "%s: diff computation failed\n", argv[0]);
        ret = 1;
    } else if (brief) {
        /* In brief mode, exit with status 1 if files differ, 0 if same */
        if (ctx.has_differences) {
            printf("Files %s and %s differ\n", file1, file2);
            ret = 1; /* Files differ */
        } else {
            ret = 0; /* Files are identical */
        }
    } else {
        ret = 0; /* Success */
    }

cleanup:
    free_file(&mf1);
    free_file(&mf2);

    return ret;
}


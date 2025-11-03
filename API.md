# xdiff C API Documentation

## Introduction

xdiff is a file differential library that provides efficient algorithms for computing differences between two files and performing three-way merges. This library is based on LibXDiff by Davide Libenzi, modified for use in git and other version control systems.

### Main Capabilities

- **File Diff**: Compute differences between two files using various algorithms (Myers, Patience, Histogram)
- **Three-way Merge**: Merge changes from two versions relative to a common ancestor
- **Configurable Options**: Control whitespace handling, context lines, diff algorithms, and merge strategies
- **Callback-based Output**: Flexible diff output through user-defined callbacks

### Basic Usage Pattern

1. Prepare input files using `mmfile_t` structures
2. Configure diff/merge parameters using `xpparam_t` and `xdemitconf_t` (for diffs) or `xmparam_t` (for merges)
3. Set up callbacks for output handling
4. Call `xdl_diff()` or `xdl_merge()` to perform the operation

---

## Data Structures

### mmfile_t

Represents a memory-mapped file or buffer containing the file content to be processed.

```c
typedef struct s_mmfile {
    char *ptr;      /* Pointer to file content */
    long size;      /* Size of file content in bytes */
} mmfile_t;
```

**Fields:**
- `ptr`: Pointer to the file content. The content should be in memory (not necessarily actually memory-mapped).
- `size`: Size of the file content in bytes.

**Usage:**
- Set `ptr` to point to your file content
- Set `size` to the size of the content
- The library does not take ownership of the memory; you are responsible for managing it

### mmbuffer_t

Represents an output buffer for merge results.

```c
typedef struct s_mmbuffer {
    char *ptr;      /* Pointer to output buffer */
    long size;      /* Size of output buffer in bytes */
} mmbuffer_t;
```

**Fields:**
- `ptr`: Pointer to the merge result. This is allocated by `xdl_merge()` and must be freed by the caller using `xdl_free()`.
- `size`: Size of the merge result in bytes.

**Usage:**
- Pass an uninitialized `mmbuffer_t` to `xdl_merge()`
- On success, the function allocates and fills `ptr` and `size`
- You are responsible for freeing the memory using `xdl_free(result->ptr)`

### xpparam_t

Preprocessing parameters for diff operations. Controls how files are parsed and compared.

```c
typedef struct s_xpparam {
    unsigned long flags;              /* Combination of XDF_* flags */
    xdl_regex_t **ignore_regex;      /* Array of regex patterns to ignore */
    size_t ignore_regex_nr;          /* Number of regex patterns */
    char **anchors;                   /* Array of anchor strings */
    size_t anchors_nr;                /* Number of anchor strings */
} xpparam_t;
```

**Fields:**
- `flags`: Bitmask of preprocessing flags (see Configuration Flags section)
- `ignore_regex`: Array of compiled regex patterns. Lines matching these patterns will be ignored in the diff.
- `ignore_regex_nr`: Number of regex patterns in the array
- `anchors`: Array of anchor strings for guided diff alignment
- `anchors_nr`: Number of anchor strings

**Usage:**
- Initialize `flags` to 0 or combine desired `XDF_*` flags
- Set `ignore_regex` and `ignore_regex_nr` if you want to ignore lines matching certain patterns
- Set `anchors` and `anchors_nr` for guided alignment (advanced usage)
- Can be zero-initialized for default behavior

### xdemitconf_t

Configuration for diff output emission. Controls how differences are formatted and presented.

```c
typedef struct s_xdemitconf {
    long ctxlen;                    /* Number of context lines around changes */
    long interhunkctxlen;           /* Context lines between hunks */
    unsigned long flags;            /* Combination of XDL_EMIT_* flags */
    find_func_t find_func;          /* Function to find function names in lines */
    void *find_func_priv;           /* Private data for find_func */
    xdl_emit_hunk_consume_func_t hunk_func;  /* Alternative hunk consumer */
} xdemitconf_t;
```

**Fields:**
- `ctxlen`: Number of context lines to include before and after each hunk
- `interhunkctxlen`: Minimum context lines between separate hunks before they are merged
- `flags`: Bitmask of output formatting flags (see Configuration Flags section)
- `find_func`: Optional function to identify function names in source code lines
- `find_func_priv`: Private data passed to `find_func`
- `hunk_func`: Optional alternative hunk processing function (if provided, `out_hunk` callback is not used)

**Usage:**
- Set `ctxlen` to control context around changes (typically 3)
- Set `interhunkctxlen` to control hunk merging (typically 0)
- Set `flags` to control output format
- Provide `find_func` for better function name detection in diffs

### xdemitcb_t

Callback structure for diff output. Your callbacks receive the diff output.

```c
typedef struct s_xdemitcb {
    void *priv;                     /* Private data passed to callbacks */
    int (*out_hunk)(void *priv,
                    long old_begin, long old_nr,
                    long new_begin, long new_nr,
                    const char *func, long funclen);
    int (*out_line)(void *priv, mmbuffer_t *mb, int nb);
} xdemitcb_t;
```

**Fields:**
- `priv`: Private data pointer passed to all callbacks
- `out_hunk`: Called for each diff hunk (range of changes)
- `out_line`: Called for each line in the diff output

**Callback Signatures:**

```c
int out_hunk(void *priv,
             long old_begin,    /* Starting line number in old file (1-based) */
             long old_nr,       /* Number of lines in old file */
             long new_begin,    /* Starting line number in new file (1-based) */
             long new_nr,       /* Number of lines in new file */
             const char *func,  /* Function name (if available) */
             long funclen);     /* Length of function name */

int out_line(void *priv,        /* Your private data */
             mmbuffer_t *mb,     /* Array of line buffers */
             int nb);            /* Number of buffers in array */
```

**out_line Details:**
- `mb` is an array of `mmbuffer_t` structures
- `nb` indicates how many buffers are in the array
- Each buffer represents one line (or part of a line) of diff output
- Buffer contents typically include prefixes like `" "`, `"-"`, `"+"` for context, removed, and added lines
- Return 0 on success, negative value on error

### xmparam_t

Parameters for three-way merge operations.

```c
typedef struct s_xmparam {
    xpparam_t xpp;           /* Diff preprocessing parameters */
    int marker_size;         /* Size of conflict markers (default: 7) */
    int level;               /* Merge simplification level */
    int favor;               /* Merge favor mode */
    int style;               /* Merge output style */
    const char *ancestor;    /* Label for original file (for conflict markers) */
    const char *file1;       /* Label for first file (for conflict markers) */
    const char *file2;       /* Label for second file (for conflict markers) */
} xmparam_t;
```

**Fields:**
- `xpp`: Preprocessing parameters (same as for diff)
- `marker_size`: Number of marker characters (use `DEFAULT_CONFLICT_MARKER_SIZE` or 7)
- `level`: Merge simplification level (see `XDL_MERGE_*` constants)
- `favor`: How to handle conflicts when simplifying (see `XDL_MERGE_FAVOR_*` constants)
- `style`: Output style for conflicts (see `XDL_MERGE_*` style constants)
- `ancestor`, `file1`, `file2`: Labels used in conflict markers (can be NULL)

### find_func_t

Function type for finding function names in source code lines.

```c
typedef long (*find_func_t)(const char *line,      /* Line content */
                            long line_len,          /* Length of line */
                            char *buffer,           /* Output buffer for function name */
                            long buffer_size,       /* Size of output buffer */
                            void *priv);            /* Private data */
```

**Behavior:**
- Returns the length of the function name if found (and copies it to `buffer`)
- Returns -1 if no function name is found in the line
- Used to identify function context in diff hunks

---

## Core Functions

### xdl_diff

Compute the difference between two files.

```c
int xdl_diff(mmfile_t *mf1, mmfile_t *mf2,
             xpparam_t const *xpp,
             xdemitconf_t const *xecfg,
             xdemitcb_t *ecb);
```

**Parameters:**
- `mf1`: Pointer to first file (old/left side)
- `mf2`: Pointer to second file (new/right side)
- `xpp`: Preprocessing parameters (can be NULL for defaults)
- `xecfg`: Output emission configuration (can be NULL for defaults)
- `ecb`: Callback structure for receiving diff output (must be valid)

**Returns:**
- `0` on success
- Negative value on error

**Behavior:**
- Compares the two files and calls the callbacks in `ecb` for each hunk and line
- Uses the preprocessing flags from `xpp` to control how files are compared
- Uses the emission configuration from `xecfg` to control output formatting
- The `out_hunk` callback is called for each range of changes
- The `out_line` callback is called for each line of diff output

### xdl_merge

Perform a three-way merge of three files.

```c
int xdl_merge(mmfile_t *orig, mmfile_t *mf1, mmfile_t *mf2,
              xmparam_t const *xmp,
              mmbuffer_t *result);
```

**Parameters:**
- `orig`: Common ancestor file
- `mf1`: First variant (typically "ours")
- `mf2`: Second variant (typically "theirs")
- `xmp`: Merge parameters (can be NULL for defaults)
- `result`: Output buffer (must be a valid pointer, will be filled on success)

**Returns:**
- `0` on success (no conflicts)
- Positive value: number of conflicts found
- Negative value on error

**Behavior:**
- Merges changes from `mf1` and `mf2` relative to `orig`
- On success, allocates memory for `result->ptr` and sets `result->size`
- You must free `result->ptr` using `xdl_free()` when done
- If conflicts occur, they are marked in the output using conflict markers
- The return value indicates how many conflict regions were found

**Conflict Markers:**
The merge result includes conflict markers when changes overlap:
```
<<<<<<< file1
content from mf1
=======
content from mf2
>>>>>>> file2
```

### xdl_mmfile_first

Get a pointer to the first byte of a memory-mapped file.

```c
void *xdl_mmfile_first(mmfile_t *mmf, long *size);
```

**Parameters:**
- `mmf`: Pointer to the memory-mapped file structure
- `size`: Output parameter that receives the file size

**Returns:**
- Pointer to the first byte of the file content
- Same as `mmf->ptr`

**Usage:**
- Convenience function to get both pointer and size
- The size is written to `*size`

### xdl_mmfile_size

Get the size of a memory-mapped file.

```c
long xdl_mmfile_size(mmfile_t *mmf);
```

**Parameters:**
- `mmf`: Pointer to the memory-mapped file structure

**Returns:**
- Size of the file in bytes
- Same as `mmf->size`

**Usage:**
- Convenience function to get the file size

---

## Configuration Flags

### Preprocessing Flags (xpparam_t.flags)

These flags control how files are parsed and compared before diff computation.

#### Whitespace Handling

- **`XDF_IGNORE_WHITESPACE`**: Ignore all whitespace differences
- **`XDF_IGNORE_WHITESPACE_CHANGE`**: Ignore changes in amount of whitespace
- **`XDF_IGNORE_WHITESPACE_AT_EOL`**: Ignore whitespace at end of line
- **`XDF_IGNORE_CR_AT_EOL`**: Ignore carriage return at end of line
- **`XDF_WHITESPACE_FLAGS`**: Combined mask of all whitespace flags

#### Other Preprocessing Flags

- **`XDF_IGNORE_BLANK_LINES`**: Ignore blank lines when computing diff
- **`XDF_NEED_MINIMAL`**: Produce minimal diff (may be slower but more compact)

#### Diff Algorithm Selection

- **`XDF_PATIENCE_DIFF`**: Use patience diff algorithm (better for some code)
- **`XDF_HISTOGRAM_DIFF`**: Use histogram diff algorithm (often faster)
- **`XDF_DIFF_ALGORITHM_MASK`**: Mask to extract algorithm bits
- **`XDF_DIFF_ALG(x)`**: Macro to extract algorithm from flags

If no algorithm flag is set, the default Myers algorithm is used.

#### Heuristics

- **`XDF_INDENT_HEURISTIC`**: Use indent heuristic to improve diff quality

### Output Formatting Flags (xdemitconf_t.flags)

These flags control how the diff output is formatted.

- **`XDL_EMIT_FUNCNAMES`**: Include function names in hunk headers
- **`XDL_EMIT_NO_HUNK_HDR`**: Do not emit hunk headers (only lines)
- **`XDL_EMIT_FUNCCONTEXT`**: Extend context to include function boundaries

### Merge Constants

#### Merge Simplification Levels (xmparam_t.level)

- **`XDL_MERGE_MINIMAL`** (0): Mark all overlapping changes as conflicts
- **`XDL_MERGE_EAGER`** (1): Mark overlapping changes as conflicts only if not identical
- **`XDL_MERGE_ZEALOUS`** (2): Analyze non-identical changes for minimal conflict set
- **`XDL_MERGE_ZEALOUS_ALNUM`** (3): Like `ZEALOUS`, but treat hunks without letters/numbers as conflicting

#### Merge Favor Modes (xmparam_t.favor)

- **`XDL_MERGE_FAVOR_OURS`** (1): Prefer first file when resolving conflicts
- **`XDL_MERGE_FAVOR_THEIRS`** (2): Prefer second file when resolving conflicts
- **`XDL_MERGE_FAVOR_UNION`** (3): Include both sides in conflict resolution

#### Merge Output Styles (xmparam_t.style)

- **`XDL_MERGE_DIFF3`** (1): Show base version in conflict markers
- **`XDL_MERGE_ZEALOUS_DIFF3`** (2): Like `DIFF3` but with zealous refinement

#### Other Merge Constants

- **`DEFAULT_CONFLICT_MARKER_SIZE`** (7): Default number of marker characters in conflict markers

---

## Callbacks

### out_hunk Callback

Called once per diff hunk (continuous region of changes).

```c
int out_hunk(void *priv,
             long old_begin,    /* 1-based line number in old file */
             long old_nr,       /* Number of lines in old file range */
             long new_begin,    /* 1-based line number in new file */
             long new_nr,       /* Number of lines in new file range */
             const char *func,  /* Function name or NULL */
             long funclen);     /* Length of function name */
```

**Parameters:**
- `priv`: Your private data pointer from `xdemitcb_t.priv`
- `old_begin`: Starting line number in the old file (1-based)
- `old_nr`: Number of lines in the old file for this hunk
- `new_begin`: Starting line number in the new file (1-based)
- `new_nr`: Number of lines in the new file for this hunk
- `func`: Function name if available (when `XDL_EMIT_FUNCNAMES` is set), otherwise NULL
- `funclen`: Length of the function name string

**Returns:**
- `0` on success
- Negative value to abort the diff operation

**Usage:**
- Called before the lines of a hunk are emitted
- Use this to print hunk headers in unified diff format
- Line numbers are 1-based (first line is 1)

### out_line Callback

Called for each line (or line segment) in the diff output.

```c
int out_line(void *priv,        /* Your private data */
             mmbuffer_t *mb,    /* Array of line buffers */
             int nb);           /* Number of buffers */
```

**Parameters:**
- `priv`: Your private data pointer from `xdemitcb_t.priv`
- `mb`: Array of `mmbuffer_t` structures, each representing a line or line segment
- `nb`: Number of buffers in the array (typically 1, but can be more for multi-part lines)

**Buffer Contents:**
Each buffer typically contains:
- `" "` prefix: Context line (unchanged)
- `"-"` prefix: Removed line (from old file)
- `"+"` prefix: Added line (from new file)
- `"\\"` prefix: Line continuation marker (rare)

The buffer contains the entire line including the prefix character.

**Returns:**
- `0` on success
- Negative value to abort the diff operation

**Usage:**
- Called for each line of diff output
- Print or process the line content as needed
- The prefix character indicates the line type

---

## Examples

### Example 1: Basic Diff

```c
#include "xdiff.h"
#include <stdio.h>
#include <stdlib.h>

int my_out_hunk(void *priv, long ob, long on, long nb, long nn,
                const char *func, long funclen) {
    printf("@@ -%ld,%ld +%ld,%ld @@", ob, on, nb, nn);
    if (func && funclen > 0) {
        printf(" %.*s", (int)funclen, func);
    }
    printf("\n");
    return 0;
}

int my_out_line(void *priv, mmbuffer_t *mb, int nb) {
    int i;
    for (i = 0; i < nb; i++) {
        fwrite(mb[i].ptr, 1, mb[i].size, stdout);
    }
    return 0;
}

int main() {
    mmfile_t mf1, mf2;
    xpparam_t xpp;
    xdemitconf_t xecfg;
    xdemitcb_t ecb;
    
    // Initialize file 1
    mf1.ptr = "line 1\nline 2\nline 3\n";
    mf1.size = strlen(mf1.ptr);
    
    // Initialize file 2
    mf2.ptr = "line 1\nline 2 modified\nline 3\n";
    mf2.size = strlen(mf2.ptr);
    
    // Configure preprocessing (default)
    memset(&xpp, 0, sizeof(xpp));
    
    // Configure output
    memset(&xecfg, 0, sizeof(xecfg));
    xecfg.ctxlen = 3;
    xecfg.interhunkctxlen = 1;
    
    // Set up callbacks
    ecb.priv = NULL;
    ecb.out_hunk = my_out_hunk;
    ecb.out_line = my_out_line;
    
    // Compute diff
    if (xdl_diff(&mf1, &mf2, &xpp, &xecfg, &ecb) < 0) {
        fprintf(stderr, "Diff failed\n");
        return 1;
    }
    
    return 0;
}
```

### Example 2: Diff with Whitespace Ignore

```c
// Same setup as Example 1, but with whitespace handling:
xpp.flags = XDF_IGNORE_WHITESPACE_CHANGE;
```

### Example 3: Three-Way Merge

```c
#include "xdiff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    mmfile_t orig, mf1, mf2;
    xmparam_t xmp;
    mmbuffer_t result;
    
    // Common ancestor
    orig.ptr = "line 1\nline 2\nline 3\n";
    orig.size = strlen(orig.ptr);
    
    // First variant
    mf1.ptr = "line 1\nline 2 modified\nline 3\n";
    mf1.size = strlen(mf1.ptr);
    
    // Second variant
    mf2.ptr = "line 1\nline 2 changed\nline 3\n";
    mf2.size = strlen(mf2.ptr);
    
    // Configure merge
    memset(&xmp, 0, sizeof(xmp));
    xmp.marker_size = DEFAULT_CONFLICT_MARKER_SIZE;
    xmp.level = XDL_MERGE_EAGER;
    xmp.ancestor = "original";
    xmp.file1 = "ours";
    xmp.file2 = "theirs";
    
    // Perform merge
    int conflicts = xdl_merge(&orig, &mf1, &mf2, &xmp, &result);
    
    if (conflicts < 0) {
        fprintf(stderr, "Merge failed\n");
        return 1;
    }
    
    if (conflicts > 0) {
        printf("Merge completed with %d conflicts\n", conflicts);
    } else {
        printf("Merge completed without conflicts\n");
    }
    
    // Print result
    fwrite(result.ptr, 1, result.size, stdout);
    
    // Free result
    xdl_free(result.ptr);
    
    return 0;
}
```

### Example 4: Using Different Diff Algorithms

```c
// Use patience diff algorithm
xpp.flags = XDF_PATIENCE_DIFF;

// Or use histogram diff algorithm
xpp.flags = XDF_HISTOGRAM_DIFF;

// Or use default Myers algorithm (no flag set)
xpp.flags = 0;
```

### Example 5: Custom Function Finder

```c
long find_function(const char *line, long line_len,
                   char *buffer, long buffer_size, void *priv) {
    // Simple example: look for "function" or "def" keywords
    if (line_len >= 8 && memcmp(line, "function ", 9) == 0) {
        // Extract function name
        const char *start = line + 9;
        const char *end = start;
        while (end < line + line_len && *end != '(' && *end != ' ') {
            end++;
        }
        long len = end - start;
        if (len > 0 && len < buffer_size) {
            memcpy(buffer, start, len);
            return len;
        }
    }
    return -1;  // No function found
}

// In your configuration:
xecfg.find_func = find_function;
xecfg.find_func_priv = NULL;  // Or pass custom data
xecfg.flags |= XDL_EMIT_FUNCNAMES;
```

---

## Error Handling

### Return Values

- **`xdl_diff()`**: Returns `0` on success, negative value on error
- **`xdl_merge()`**: Returns `0` on success (no conflicts), positive value (number of conflicts), or negative value on error

### Common Error Scenarios

1. **Memory allocation failures**: The library may return errors if memory cannot be allocated
2. **Invalid parameters**: Passing NULL for required parameters may cause errors
3. **Callback failures**: If your callbacks return negative values, the operation will abort

### Memory Management

- **Input files**: You manage the memory for `mmfile_t.ptr`
- **Merge results**: `xdl_merge()` allocates `result->ptr`; you must free it with `xdl_free()`
- **Other allocations**: The library manages its internal allocations

### Best Practices

1. Always check return values from `xdl_diff()` and `xdl_merge()`
2. Initialize structures to zero before setting specific fields
3. Free merge result buffers when done
4. Handle callback errors gracefully
5. Use appropriate preprocessing flags to improve diff quality for your use case

---

## Notes

- The library is C89/C90 compatible
- All line numbers are 1-based (first line is 1, not 0)
- File content should be in memory; the library does not perform actual file I/O
- The library uses callbacks for output to allow flexible formatting
- Conflict markers in merge results follow standard formats (similar to git)
- The `xdl_regex_t` type is defined in `git-xdiff.h` and may be `void*` on some platforms (e.g., MSVC without regex support)


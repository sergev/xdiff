xdiff (via git)
===============

This is the version of the xdiff file differential library used by [git](https://github.com/git/git).

This project began life as [LibXDiff](http://www.xmailserver.org/xdiff-lib.html) by Davide Libenzi, but has been modified to suit the git project's needs. Some unnecessary functionality has been removed and some new functionality has been added.

Fundamentally, this library is _meant for git_ but has been extracted into a standalone library for compatibility with other git-like projects, for example, [libgit2](https://github.com/libgit2/libgit2).

This repository tracks the git project as an upstream, and makes only minimal (with a goal of _zero_) changes to xdiff itself.

## Project Overview

This project provides:

1. **libxdiff**: A C library for computing differences between files and performing three-way merges
2. **xdiff CLI**: A command-line utility similar to GNU diff, providing a user-friendly interface to the xdiff library

### xdiff Library

The xdiff library provides efficient algorithms for:
- Computing differences between two files using various algorithms (Myers, Patience, Histogram)
- Performing three-way merges
- Configurable whitespace handling, context lines, and diff algorithms
- Flexible callback-based output system

For detailed API documentation, see [API.md](API.md).

### xdiff CLI Utility

The `xdiff` command-line utility provides a GNU diff-like interface to the xdiff library. It supports:

- Standard diff formats (unified, context)
- Multiple diff algorithms (Myers, Patience, Histogram)
- Whitespace handling options
- **Moved block detection** - Identifies blocks of text that have been relocated within a file (similar to `git diff --color-moved`)

## Building

### Prerequisites

- CMake 3.10 or higher
- C compiler (GCC, Clang, etc.)
- C++ compiler (for tests)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

This will build:
- `libxdiff` - The xdiff library (static library)
- `xdiff` - The CLI utility executable
- `test_xdiff_cli` - Unit tests (if GoogleTest is available via FetchContent)

### Installation

```bash
cmake --install . --prefix /usr/local
```

This installs:
- `libxdiff.a` to `PREFIX/lib/`
- `xdiff` executable to `PREFIX/bin/`

## Using the xdiff CLI

### Basic Usage

```bash
xdiff [OPTIONS] FILE1 FILE2
```

Compare two files and show the differences:

```bash
xdiff file1.txt file2.txt
```

### Common Options

#### Output Format

- `-u, --unified[=N]` - Unified diff format (default: 3 context lines)
- `-c, --context[=N]` - Context diff format (default: 3 context lines)
- `-q, --brief` - Only report whether files differ

#### Whitespace Handling

- `-w, --ignore-all-space` - Ignore all whitespace
- `-b, --ignore-space-change` - Ignore whitespace changes
- `-B, --ignore-blank-lines` - Ignore blank lines

#### Diff Algorithms

- `--patience` - Use patience diff algorithm
- `--histogram` - Use histogram diff algorithm
- `--minimal` - Produce minimal diff

#### Moved Block Detection

The `xdiff` utility detects when blocks of text have been moved within a file, similar to `git diff --color-moved`. Moved lines are marked with `<` for deleted lines and `>` for added lines (instead of the standard `-` and `+`).

- `--moved[=MODE]` - Enable moved block detection
  - `no` - Disable moved block detection
  - `plain` - Detect all moved blocks (default)
  - `blocks` - Detect moved blocks (minimum 20 alphanumeric characters)
  - `zebra` - Detect moved blocks with alternating highlighting
  - `dimmed-zebra` - Detect moved blocks with dimmed interior lines

- `--moved-ws=MODE` - Whitespace handling for moved block detection
  - `ignore-all` - Ignore all whitespace when matching blocks
  - `ignore-change` - Ignore whitespace changes
  - `ignore-at-eol` - Ignore whitespace at end of line

#### Help

- `-h, --help` - Show help message

### Examples

#### Basic diff, detect moved blocks

```bash
xdiff old.txt new.txt
```

#### Unified diff with 5 context lines

```bash
xdiff -u 5 old.txt new.txt
```

#### Detect moved blocks, ignoring whitespace at end of line

```bash
xdiff --moved-ws=ignore-at-eol old.txt new.txt
```

#### Brief mode (only report if files differ)

```bash
xdiff -q file1.txt file2.txt
```

### Output Format

The output format follows the standard unified diff format:

```
--- file1.txt
+++ file2.txt
@@ -1,3 +1,3 @@
 line1
-line2
+line2_modified
 line3
```

Moved lines are marked with `<` and `>`:

```
--- file1.txt
+++ file2.txt
@@ -1,5 +1,5 @@
 header
+new_line
<block1
<block2
-block3
+block3
>block1
>block2
 footer
```

In this example, `block1` and `block2` were moved from before `block3` to after it, indicated by the `<` and `>` markers.

## Testing

Unit tests are provided using GoogleTest. To run the tests:

```bash
cd build
ctest --output-on-failure
```

Tests are automatically discovered and run via CTest.

## Inclusion in Your Application

Although this project _is used by git_, it has no git-specific code explicitly inside it. git -- and other callers -- add application-specific code through the `git-xdiff.h` file. For example, if your application uses a custom `malloc`, then you can configure it in the `git-xdiff.h` file.

### Using the Library

Link against `libxdiff` and include `xdiff.h`:

```c
#include "xdiff.h"

// Prepare input files
mmfile_t mf1, mf2;
// ... populate mf1 and mf2 ...

// Configure diff parameters
xpparam_t xpp;
xpp.flags = 0;

xdemitconf_t xecfg;
xecfg.ctxlen = 3;
xecfg.flags = XDL_EMIT_BDIFFHUNK;

xdemitcb_t ecb;
ecb.out_line = my_line_callback;
ecb.out_hunk = my_hunk_callback;

// Perform diff
xdl_diff(&mf1, &mf2, &xpp, &xecfg, &ecb);
```

See [API.md](API.md) for detailed API documentation.

## Moved Block Detection

The moved block detection feature (implemented in `xdiff-moved.c` and `xdiff-moved.h`) extends the xdiff library to identify blocks of text that have been relocated within a file. This is useful for:

- Understanding code refactoring
- Identifying large-scale reorganizations
- Reducing noise in diff output when blocks are moved rather than deleted and re-added

The detection algorithm:
1. Collects deleted and added blocks from the diff
2. Computes hashes for each block (with optional whitespace normalization)
3. Matches blocks with identical hashes
4. Filters blocks based on the selected mode (plain, blocks, zebra, etc.)
5. Marks matched blocks in the output

## Contributions

Contributions to improve the build or compatibility of this library _as a standalone work of art_ are welcome. Contributions that change the diff functionality, however, _[should be made to git project itself](https://github.com/git/git/blob/master/Documentation/SubmittingPatches)_. (Once those changes land in git, they will be included here.)

## Credits

Many thanks to Davide Libenzi, the original author of LibXDiff, as well as the numerous contributors to git and libgit2 who have improved xdiff over the years.

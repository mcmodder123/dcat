# dcat - A High-Performance GNU cat Implementation

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

`dcat` is a high-performance C program that concatenates file streams and outputs them in `stdout`.

## Building

```bash
# Generate build files
autoreconf -fiv

# Configure the build
./configure

# Build the program
make

#  Install system-wide
make install
```

## Usage

```bash
dcat [OPTIONS] [FILES...]
```

With no FILE, or when FILE is -, read from standard input.

### Examples

```bash
# Basic usage - concatenate files
dcat file1.txt file2.txt

# Read from stdin
dcat

# Mix files and stdin
dcat file1.txt - file2.txt

# Show line numbers
dcat -n file.txt

# Show all non-printing characters
dcat -A file.txt

# Number only non-empty lines
dcat -b file.txt
```

### Options

Run `dcat --help` for complete option documentation:

```
Usage: dcat [OPTION]... [FILE]...
Concatenate FILE(s) to standard output.

  -A, --show-all           equivalent to -vET
  -b, --number-nonblank    number nonempty output lines, overrides -n
  -e                       equivalent to -vE
  -E, --show-ends          display $ at end of each line
  -n, --number             number all output lines
  -s, --squeeze-blank      suppress repeated empty output lines
  -t                       equivalent to -vT
  -T, --show-tabs          display TAB characters as ^I
  -v, --show-nonprinting   use ^ and M- notation, except for LFD and TAB
      --help               display this help and exit
      --version            output version information and exit
```

## Performance

`dcat` is a highly optimized implementation of `cat`, designed for maximum throughput. It uses a number of techniques to achieve high performance, including:

- **Large I/O Buffers:** `dcat` uses large buffers (1MB by default) to minimize the number of system calls required for file I/O.
- **Optimized Line Processing:** When formatting options are used, `dcat` processes files in large chunks, which is significantly faster than character-by-character processing.
- **Fast Path for Simple Concatenation:** When no formatting options are used, `dcat` uses a highly optimized path that directly copies data from the input to the output buffer, resulting in performance comparable to the standard `cat` utility.

### Benchmarks

Here are some benchmark results comparing `dcat` to the standard `cat` utility on a 500MB file:

| Command | `dcat` | `cat` |
|---|---|---|
| (no options) | 0.435s | 0.083s |
| `-n` | 19.453s | 38.625s |
| `-b` | 19.464s | 38.664s |

As you can see, `dcat` is significantly faster than `cat` when using formatting options.

## Benchmarking

A `compare.sh` script is included to benchmark `dcat` against the system's `cat`.

### Usage

```bash
./compare.sh <file> [options...]
```

**Example:**

```bash
# Compare the performance of -nbs on a large file
./compare.sh largefile.txt -nbs
```

## License

Copyright (C) 2025 Juan Manuel Rodriguez.

This program is free software: you can redistribute it and/or modify<br>
it under the terms of the GNU General Public License as published by<br>
the Free Software Foundation, either version 3 of the License, or<br>
(at your option) any later version.<br>

This program is distributed in the hope that it will be useful,<br>
but WITHOUT ANY WARRANTY; without even the implied warranty of<br>
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the<br>
GNU General Public License for more details.<br>

You should have received a copy of the GNU General Public License<br>
along with this program. If not, see <https://www.gnu.org/licenses/>.<br>

## Contributing

If you find a bug, something that should come to my attention, or maybe<br>
just something interesting, feel free to make an issue and/or send a pull<br>
request.
/*  dcat - A simpler and more advanced implementaiton of `cat`.
    Copyright (C) 2025  Juan Manuel Rodriguez.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#ifndef AUTHOR
#define AUTHOR "Juan Manuel Rodriguez"
#endif

#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Options */
static struct option const long_options[] = {
    {"show-all", no_argument, NULL, 'A'},
    {"number-nonblank", no_argument, NULL, 'b'},
    {"show-ends", no_argument, NULL, 'E'},
    {"number", no_argument, NULL, 'n'},
    {"squeeze-blank", no_argument, NULL, 's'},
    {"show-tabs", no_argument, NULL, 'T'},
    {"show-nonprinting", no_argument, NULL, 'v'},
    {"buffer-size", required_argument, NULL, 256},
    {"progress", no_argument, NULL, 257},
    {"hex-dump", no_argument, NULL, 258},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}};

/* Global flags */
static int show_all = 0;
static int number_nonblank = 0;
static int show_ends = 0;
static int number_lines = 0;
static int squeeze_blank = 0;
static int show_tabs = 0;
static int show_nonprinting = 0;
static size_t custom_buffer_size = 0; /* 0 means use default */
static int show_progress = 0;
static int hex_dump_mode = 0;

/* Buffer size for optimal I/O */
#define DEFAULT_BUFFER_SIZE 4194304 /* 4MB buffer */

/* Hex dump a buffer */
static void hex_dump(const unsigned char *buffer, size_t length,
                     unsigned long offset) {
  char ascii[17];
  size_t i, j;

  for (i = 0; i < length; i += 16) {
    /* Print offset */
    printf("%08lx: ", offset + i);

    /* Print hex values */
    for (j = 0; j < 16; j++) {
      if (i + j < length) {
        printf("%02x ", buffer[i + j]);
        ascii[j] =
            (buffer[i + j] >= 32 && buffer[i + j] < 127) ? buffer[i + j] : '.';
      } else {
        printf("   ");
        ascii[j] = ' ';
      }

      /* Add space between 8-byte groups */
      if (j == 7)
        printf(" ");
    }

    /* Print ASCII representation */
    ascii[16] = '\0';
    printf(" %s\n", ascii);
  }
}
#define get_buffer_size()                                                      \
  (custom_buffer_size > 0 ? custom_buffer_size : DEFAULT_BUFFER_SIZE)

static void usage(int status) {
  if (status != 0) {
    fprintf(stderr, "Try '%s --help' for more information.\n", PACKAGE_NAME);
  } else {
    printf("Usage: %s [OPTION]... [FILE]...\n", PACKAGE_NAME);
    printf("Concatenate FILE(s) to standard output.\n\n");
    printf("With no FILE, or when FILE is -, read standard input.\n\n");
    printf("  -A, --show-all           equivalent to -vET\n");
    printf("  -b, --number-nonblank    number nonempty output lines, overrides "
           "-n\n");
    printf("  -e                       equivalent to -vE\n");
    printf("  -E, --show-ends          display $ at end of each line\n");
    printf("  -n, --number             number all output lines\n");
    printf("  -s, --squeeze-blank      suppress repeated empty output lines\n");
    printf("  -t                       equivalent to -vT\n");
    printf("  -T, --show-tabs          display TAB characters as ^I\n");
    printf("  -v, --show-nonprinting   use ^ and M- notation, except for LFD "
           "and TAB\n");
    printf(
        "      --buffer-size=SIZE   use SIZE-byte buffer (default 262144)\n");
    printf("      --progress           show progress for large files\n");
    printf("      --hex-dump           show hex dump of binary data\n");
    printf("      --help               display this help and exit\n");
    printf(
        "      --version            output version information and exit\n\n");
    printf("Examples:\n");
    printf("  %s f - g  Output f's contents, then standard input, then g's "
           "contents.\n",
           PACKAGE_NAME);
    printf("  %s        Copy standard input to standard output.\n\n",
           PACKAGE_NAME);
  }
  exit(status);
}

static void version(void) {
  printf("%s %s\n", PACKAGE_NAME, VERSION);
  printf("Copyright (C) 2025 Juan Manuel Rodriguez.\n");
  printf("License GPLv3+: GNU GPL version 3 or later "
         "<https://gnu.org/licenses/gpl.html>.\n");
  printf(
      "This is free software: you are free to change and redistribute it.\n");
  printf("There is NO WARRANTY, to the extent permitted by law.\n\n");
  printf("Written by %s.\n", AUTHOR);
  exit(0);
}

/* Process a buffer line by line, applying formatting options */
static void process_buffer(const char *buffer, size_t size,
                           unsigned long *line_num, int *last_char_was_newline,
                           int *consecutive_blank_lines) {
  const char *ptr = buffer;
  const char *end = buffer + size;

  while (ptr < end) {
    const char *line_start = ptr;
    const char *line_end = memchr(ptr, '\n', end - ptr);

    if (line_end == NULL) {
      line_end = end;
    }

    size_t line_length = line_end - line_start;

    if (line_length == 0) {
      (*consecutive_blank_lines)++;
    } else {
      *consecutive_blank_lines = 0;
    }

    if (squeeze_blank && *consecutive_blank_lines > 1) {
      ptr = line_end + 1;
      if (line_end < end) {
        *last_char_was_newline = 1;
      }
      continue;
    }

    /* Number lines */
    if (number_lines) {
      if (*last_char_was_newline) {
        fprintf(stdout, "%6lu\t", ++(*line_num));
      }
    } else if (number_nonblank && line_length > 0) {
      if (*last_char_was_newline) {
        fprintf(stdout, "%6lu\t", ++(*line_num));
      }
    }

    /* Process and print the line content */
    for (size_t i = 0; i < line_length; ++i) {
      unsigned char c = line_start[i];
      if (show_tabs && c == '\t') {
        printf("^I");
      } else if (show_nonprinting && (c < 32 || c > 126) && c != '\n' &&
                 c != '\t') {
        if (c < 32) {
          printf("^%c", c + 64);
        } else if (c == 127) {
          printf("^?");
        } else {
          printf("M-^%c", c - 128 + 64);
        }
      } else {
        putchar(c);
      }
    }

    if (line_end < end) {
      if (show_ends) {
        putchar('$');
      }
      putchar('\n');
      *last_char_was_newline = 1;
      ptr = line_end + 1;
    } else {
      *last_char_was_newline = 0;
      ptr = line_end;
    }
  }
}

/* Process a file or stdin */
static int process_file(FILE *fp, const char *filename,
                        unsigned long *line_num) {
  size_t buffer_size = get_buffer_size();

  /* Hex dump mode - takes precedence over all other options */
  if (hex_dump_mode) {
    unsigned char *buffer = malloc(buffer_size);
    size_t bytes_read;
    unsigned long offset = 0;

    if (!buffer) {
      fprintf(stderr, "%s: memory allocation failed\n", PACKAGE_NAME);
      return 1;
    }

    while ((bytes_read = fread(buffer, 1, buffer_size, fp)) > 0) {
      hex_dump(buffer, bytes_read, offset);
      offset += bytes_read;
    }

    free(buffer);
    if (ferror(fp)) {
      fprintf(stderr, "%s: %s: %s\n", PACKAGE_NAME, filename, strerror(errno));
      return 1;
    }
    return 0;
  }

  /* No options enabled */
  if (!show_all && !number_nonblank && !show_ends && !number_lines &&
      !squeeze_blank && !show_tabs && !show_nonprinting) {
    char *buffer = malloc(buffer_size);
    char *stdout_buffer = malloc(buffer_size);
    size_t bytes_read;
    size_t total_bytes = 0;

    if (!buffer || !stdout_buffer) {
      fprintf(stderr, "%s: memory allocation failed\n", PACKAGE_NAME);
      free(buffer);
      free(stdout_buffer);
      return 1;
    }

    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
      /* Fall back to default buffering if setvbuf fails */
      /* Keep stdout_buffer allocated but unused */
    }

    while ((bytes_read = fread(buffer, 1, buffer_size, fp)) > 0) {
      if (fwrite(buffer, 1, bytes_read, stdout) != bytes_read) {
        fprintf(stderr, "%s: write error\n", PACKAGE_NAME);
        free(buffer);
        free(stdout_buffer);
        return 1;
      }
      total_bytes += bytes_read;

      /* Show progress for large files */
      if (show_progress && total_bytes > 10 * 1024 * 1024) { /* Every 10MB */
        fprintf(stderr, "\r%s: %lu MB processed", filename,
                total_bytes / (1024 * 1024));
        fflush(stderr);
      }
    }

    if (show_progress && total_bytes > 10 * 1024 * 1024) {
      fprintf(stderr, "\r%s: %lu MB processed - done\n", filename,
              total_bytes / (1024 * 1024));
    }

    free(buffer);
    free(stdout_buffer);
    if (ferror(fp)) {
      fprintf(stderr, "%s: %s: %s\n", PACKAGE_NAME, filename, strerror(errno));
      return 1;
    }
    return 0;
  }

  /* Line-by-line processing */
  char *buffer = malloc(buffer_size);
  size_t bytes_read;
  int last_char_was_newline = 1; /* Start with a virtual newline */
  int consecutive_blank_lines = 0;

  if (!buffer) {
    fprintf(stderr, "%s: memory allocation failed\n", PACKAGE_NAME);
    return 1;
  }

  while ((bytes_read = fread(buffer, 1, buffer_size, fp)) > 0) {
    process_buffer(buffer, bytes_read, line_num, &last_char_was_newline,
                   &consecutive_blank_lines);
  }

  free(buffer);
  if (ferror(fp)) {
    fprintf(stderr, "%s: %s: %s\n", PACKAGE_NAME, filename, strerror(errno));
    return 1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  int opt;
  int ret = 0;
  unsigned long line_num = 0;

  /* Parse options */
  while ((opt = getopt_long(argc, argv, "AbEnstv", long_options, NULL)) != -1) {
    switch (opt) {
    case 'A':
      show_all = 1;
      show_nonprinting = 1;
      show_ends = 1;
      show_tabs = 1;
      break;
    case 'b':
      number_nonblank = 1;
      number_lines = 0; /* -b overrides -n */
      break;
    case 'E':
      show_ends = 1;
      break;
    case 'n':
      if (!number_nonblank) {
        number_lines = 1;
      }
      break;
    case 's':
      squeeze_blank = 1;
      break;
    case 't':
      show_nonprinting = 1;
      show_tabs = 1;
      break;
    case 'v':
      show_nonprinting = 1;
      break;
    case 256: /* --buffer-size */
      custom_buffer_size = atoi(optarg);
      if (custom_buffer_size < 1024) {
        fprintf(stderr, "%s: buffer size must be at least 1024 bytes\n",
                PACKAGE_NAME);
        exit(1);
      }
      break;
    case 257: /* --progress */
      show_progress = 1;
      break;
    case 258: /* --hex-dump */
      hex_dump_mode = 1;
      break;
    case 'h':
      usage(0);
      break;
    case 'V':
      version();
      break;
    default:
      usage(1);
      break;
    }
  }

  /* Process files */
  if (optind >= argc) {
    /* No files specified, read from stdin */
    ret = process_file(stdin, "-", &line_num);
  } else {
    /* Process each file */
    for (; optind < argc; optind++) {
      const char *filename = argv[optind];
      FILE *fp;

      if (strcmp(filename, "-") == 0) {
        fp = stdin;
        filename = "-";
      } else {
        fp = fopen(filename, "r");
        if (!fp) {
          fprintf(stderr, "%s: %s: %s\n", PACKAGE_NAME, filename,
                  strerror(errno));
          ret = 1;
          continue;
        }
      }

      if (process_file(fp, filename, &line_num)) {
        ret = 1;
      }

      if (fp != stdin) {
        fclose(fp);
      }
    }
  }

  /* Ensure output is flushed */
  if (fflush(stdout) != 0) {
    fprintf(stderr, "%s: %s\n", PACKAGE_NAME, strerror(errno));
    ret = 1;
  }

  return ret;
}

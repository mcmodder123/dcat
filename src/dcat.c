#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PROGRAM_NAME
#define PROGRAM_NAME "dcat"
#endif

#ifndef VERSION
#define VERSION "1.0"
#endif

/* Options */
static struct option const long_options[] = {
    {"show-all", no_argument, NULL, 'A'},
    {"number-nonblank", no_argument, NULL, 'b'},
    {"show-ends", no_argument, NULL, 'E'},
    {"number", no_argument, NULL, 'n'},
    {"squeeze-blank", no_argument, NULL, 's'},
    {"show-tabs", no_argument, NULL, 'T'},
    {"show-nonprinting", no_argument, NULL, 'v'},
    {"buffer-size", required_argument, NULL, 256}, /* New: custom buffer size */
    {"progress", no_argument, NULL,
     257}, /* New: show progress for large files */
    {"hex-dump", no_argument, NULL, 258}, /* New: hex dump mode */
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

/* Buffer size for efficient I/O - increased for maximum performance */
#define DEFAULT_BUFFER_SIZE 262144 /* 256KB buffer for optimal I/O */

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
    fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
  } else {
    printf("Usage: %s [OPTION]... [FILE]...\n", PROGRAM_NAME);
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
           PROGRAM_NAME);
    printf("  %s        Copy standard input to standard output.\n\n",
           PROGRAM_NAME);
    printf("GNU coreutils online help: "
           "<https://www.gnu.org/software/coreutils/>\n");
    printf("Full documentation <https://www.gnu.org/software/coreutils/%s>\n",
           PROGRAM_NAME);
    printf("or available locally via: info '(coreutils) %s invocation'\n",
           PROGRAM_NAME);
  }
  exit(status);
}

static void version(void) {
  printf("%s %s\n", PROGRAM_NAME, VERSION);
  printf("Copyright (C) 2025 Juan Manuel Rodriguez.\n");
  printf("License GPLv3+: GNU GPL version 3 or later "
         "<https://gnu.org/licenses/gpl.html>.\n");
  printf(
      "This is free software: you are free to change and redistribute it.\n");
  printf("There is NO WARRANTY, to the extent permitted by law.\n\n");
  printf("Written by %s.\n", PROGRAM_NAME);
  exit(0);
}

/* Output a character with special handling for non-printing chars */
static void output_char(unsigned char c, int *line_pos) {
  int show_special = show_nonprinting || show_all;

  if (show_special && c != '\t' && c != '\n') {
    /* Handle non-printing characters except tab and newline */
    if (c < 32) {
      putchar('^');
      putchar(c + 64);
    } else if (c == 127) {
      putchar('^');
      putchar('?');
    } else if (c >= 128) {
      putchar('M');
      putchar('-');
      if (c < 128 + 32) {
        putchar('^');
        putchar(c - 128 + 64);
      } else if (c == 255) {
        putchar('^');
        putchar('?');
      } else {
        putchar(c - 128);
      }
    } else {
      putchar(c);
    }
  } else if (show_tabs && c == '\t') {
    putchar('^');
    putchar('I');
  } else {
    putchar(c);
  }

  if (line_pos) {
    (*line_pos)++;
  }
}

/* Process a file or stdin - optimized for maximum speed */
static int process_file(FILE *fp, const char *filename,
                        unsigned long *line_num) {
  size_t buffer_size = get_buffer_size();

  /* Hex dump mode - takes precedence over all other options */
  if (hex_dump_mode) {
    unsigned char *buffer = malloc(buffer_size);
    size_t bytes_read;
    unsigned long offset = 0;

    if (!buffer) {
      fprintf(stderr, "%s: memory allocation failed\n", PROGRAM_NAME);
      return 1;
    }

    while ((bytes_read = fread(buffer, 1, buffer_size, fp)) > 0) {
      hex_dump(buffer, bytes_read, offset);
      offset += bytes_read;
    }

    free(buffer);
    if (ferror(fp)) {
      fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, filename, strerror(errno));
      return 1;
    }
    return 0;
  }

  /* Fast path: no options enabled - direct buffer copy for maximum speed */
  if (!show_all && !number_nonblank && !show_ends && !number_lines &&
      !squeeze_blank && !show_tabs && !show_nonprinting) {
    char *buffer = malloc(buffer_size);
    size_t bytes_read;
    size_t total_bytes = 0;

    if (!buffer) {
      fprintf(stderr, "%s: memory allocation failed\n", PROGRAM_NAME);
      return 1;
    }

    /* Pure throughput mode - no processing, just copy */
    while ((bytes_read = fread(buffer, 1, buffer_size, fp)) > 0) {
      if (fwrite(buffer, 1, bytes_read, stdout) != bytes_read) {
        fprintf(stderr, "%s: write error\n", PROGRAM_NAME);
        free(buffer);
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
    if (ferror(fp)) {
      fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, filename, strerror(errno));
      return 1;
    }
    return 0;
  }

  /* Character-by-character processing */
  char *buffer_slow = malloc(buffer_size);
  size_t bytes_read;
  int prev_blank = 0;
  int line_pos = 0;
  int in_line = 0;
  size_t total_bytes = 0;

  if (!buffer_slow) {
    fprintf(stderr, "%s: memory allocation failed\n", PROGRAM_NAME);
    return 1;
  }

  while ((bytes_read = fread(buffer_slow, 1, buffer_size, fp)) > 0) {
    total_bytes += bytes_read;
    size_t i;
    for (i = 0; i < bytes_read; i++) {
      unsigned char c = buffer_slow[i];

      if (c == '\n') {
        /* End of line */
        if (show_ends || show_all) {
          putchar('$');
        }
        putchar('\n');

        /* Handle line numbering */
        if (number_lines || (number_nonblank && in_line)) {
          (*line_num)++;
        }

        /* Reset for next line */
        in_line = 0;
        line_pos = 0;

        /* Handle blank line squeezing */
        if (squeeze_blank && prev_blank) {
          continue;
        }
        prev_blank = 1;
      } else {
        /* Handle line numbering at start of line */
        if (!in_line && (number_lines || (number_nonblank && c != '\n'))) {
          if (number_lines) {
            fprintf(stdout, "%6lu\t", *line_num + 1);
          } else if (number_nonblank && c != '\n') {
            fprintf(stdout, "%6lu\t", *line_num + 1);
            (*line_num)++;
          }
        }

        /* Output character */
        output_char(c, &line_pos);
        in_line = 1;
        prev_blank = 0;
      }
    }

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

  free(buffer_slow);
  if (ferror(fp)) {
    fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, filename, strerror(errno));
    return 1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  int opt;
  int ret = 0;
  unsigned long line_num = 0;

  /* Parse options */
  while ((opt = getopt_long(argc, argv, "AbEe::n::s::t::T::v::hV", long_options,
                            NULL)) != -1) {
    switch (opt) {
    case 'A':
      show_all = 1;
      show_nonprinting = 1;
      show_ends = 1;
      show_tabs = 1;
      break;
    case 'b':
      number_nonblank = 1;
      break;
    case 'E':
      show_ends = 1;
      break;
    case 'e':
      show_nonprinting = 1;
      show_ends = 1;
      break;
    case 'n':
      number_lines = 1;
      break;
    case 's':
      squeeze_blank = 1;
      break;
    case 'T':
      show_tabs = 1;
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
                PROGRAM_NAME);
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
          fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, filename,
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
    fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
    ret = 1;
  }

  return ret;
}

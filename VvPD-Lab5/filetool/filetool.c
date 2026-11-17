#include <errno.h>
#include <stdio.h>
#include <string.h>

enum {
    EXIT_OK = 0,
    EXIT_USAGE = 64,
    EXIT_INPUT = 66,
    EXIT_OUTPUT = 73,
    EXIT_IO = 74
};

static void usage(FILE *stream)
{
    fprintf(stream,
            "Usage: filetool --input-file <file> --output-file <file> "
            "[--show-stats]\n"
            "       filetool --show-args [other options]\n"
            "\n"
            "Options:\n"
            "  --input-file <file>   source file\n"
            "  --output-file <file>  destination file\n"
            "  --show-stats          print the number of copied bytes to stdout\n"
            "  --show-args           print argc and every argv element\n"
            "  --help                show this help\n");
}

static void show_args(int argc, char *argv[])
{
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; ++i) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }
}

static int copy_file(const char *input_file, const char *output_file,
                     int show_stats)
{
    FILE *input = fopen(input_file, "rb");
    if (input == NULL) {
        fprintf(stderr, "Error: cannot open input file %s: %s\n",
                input_file, strerror(errno));
        return EXIT_INPUT;
    }

    FILE *output = fopen(output_file, "wb");
    if (output == NULL) {
        fprintf(stderr, "Error: cannot open output file %s: %s\n",
                output_file, strerror(errno));
        fclose(input);
        return EXIT_OUTPUT;
    }

    unsigned char buffer[4096];
    unsigned long long bytes = 0;
    int result = EXIT_OK;

    for (;;) {
        size_t read_count = fread(buffer, 1, sizeof buffer, input);
        if (read_count != 0) {
            if (fwrite(buffer, 1, read_count, output) != read_count) {
                fprintf(stderr, "Error: write failed: %s\n", strerror(errno));
                result = EXIT_IO;
                break;
            }
            bytes += read_count;
        }

        if (read_count < sizeof buffer) {
            if (ferror(input)) {
                fprintf(stderr, "Error: read failed: %s\n", strerror(errno));
                result = EXIT_IO;
            }
            break;
        }
    }

    if (fclose(input) != 0 && result == EXIT_OK) {
        fprintf(stderr, "Error: cannot close input file: %s\n", strerror(errno));
        result = EXIT_IO;
    }
    if (fclose(output) != 0 && result == EXIT_OK) {
        fprintf(stderr, "Error: cannot close output file: %s\n", strerror(errno));
        result = EXIT_IO;
    }

    if (result == EXIT_OK && show_stats) {
        printf("Copied: %llu bytes\n", bytes);
    }

    return result;
}

int main(int argc, char *argv[])
{
    const char *input_file = NULL;
    const char *output_file = NULL;
    int show_stats = 0;
    int show_arguments = 0;
    int help = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--input-file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr,
                        "Error: --input-file requires a file name\n");
                usage(stderr);
                return EXIT_USAGE;
            }
            input_file = argv[++i];
        } else if (strcmp(argv[i], "--output-file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr,
                        "Error: --output-file requires a file name\n");
                usage(stderr);
                return EXIT_USAGE;
            }
            output_file = argv[++i];
        } else if (strcmp(argv[i], "--show-stats") == 0) {
            show_stats = 1;
        } else if (strcmp(argv[i], "--show-args") == 0) {
            show_arguments = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            help = 1;
        } else {
            fprintf(stderr, "Error: unknown option: %s\n", argv[i]);
            usage(stderr);
            return EXIT_USAGE;
        }
    }

    if (help) {
        usage(stdout);
        return EXIT_OK;
    }

    if (show_arguments) {
        show_args(argc, argv);
        if (input_file == NULL && output_file == NULL) {
            return EXIT_OK;
        }
    }

    if (input_file == NULL || output_file == NULL) {
        fprintf(stderr,
                "Error: --input-file and --output-file are required\n");
        usage(stderr);
        return EXIT_USAGE;
    }

    return copy_file(input_file, output_file, show_stats);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "container.h"
#include "fileio.h"

#define CMPZ_VERSION_STRING "0.1.0"

/* Exit codes: scripts and other programs rely on these. */
enum {
    CMPZ_EXIT_OK = 0,
    CMPZ_EXIT_USAGE = 1,     /* wrong command line */
    CMPZ_EXIT_IO = 2,        /* a file could not be read or written */
    CMPZ_EXIT_BAD_FILE = 3,  /* the input is not a valid CMPZ file, or it is damaged */
    CMPZ_EXIT_INTERNAL = 4   /* out of memory or an unexpected failure */
};

static void print_usage(FILE *to) {
    fprintf(to,
            "Usage:\n"
            "  cmpz compress   [-f] <input> <output>   compress a file\n"
            "  cmpz decompress [-f] <input> <output>   restore a compressed file\n"
            "  cmpz test       <input>                 check a compressed file, write nothing\n"
            "  cmpz --help | --version\n"
            "\n"
            "Options:\n"
            "  -f, --force   overwrite the output file if it already exists\n"
            "\n"
            "Exit codes: 0 ok, 1 usage, 2 file error, 3 invalid or damaged file, 4 internal error\n");
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

/* Checks the output path before any work is done. Returns CMPZ_EXIT_OK if it is fine to write there. */
static int check_output_path(const char *input, const char *output, int force) {
    if (strcmp(input, output) == 0) {
        fprintf(stderr, "cmpz: error: input and output are the same file: %s\n", input);
        return CMPZ_EXIT_USAGE;
    }
    if (!force && file_exists(output)) {
        fprintf(stderr, "cmpz: error: %s already exists (use -f to overwrite)\n", output);
        return CMPZ_EXIT_IO;
    }
    return CMPZ_EXIT_OK;
}

static int cmd_compress(const char *input, const char *output, int force) {
    unsigned char *data = NULL, *packed = NULL;
    size_t size = 0, packed_size = 0;
    int code = CMPZ_EXIT_OK;

    if ((code = check_output_path(input, output, force)) != CMPZ_EXIT_OK) {
        return code;                       // nothing allocated yet, so a plain return is safe
    }
    if (read_file(input, &data, &size) != 0) {
        fprintf(stderr, "cmpz: error: cannot read %s\n", input);
        return CMPZ_EXIT_IO;
    }
    if (cmpz_compress(data, size, &packed, &packed_size) != 0) {
        fprintf(stderr, "cmpz: error: compression failed (out of memory?)\n");
        code = CMPZ_EXIT_INTERNAL;
        goto done;
    }
    if (write_file(output, packed, packed_size) != 0) {
        fprintf(stderr, "cmpz: error: cannot write %s\n", output);
        code = CMPZ_EXIT_IO;
        goto done;
    }
    if (size > 0) {
        printf("%s: %llu -> %llu bytes (%.1f%%)\n", input, (unsigned long long)size,
               (unsigned long long)packed_size, 100.0 * (double)packed_size / (double)size);
    } else {
        printf("%s: 0 -> %llu bytes\n", input, (unsigned long long)packed_size);
    }

done:
    free(data);                            // free(NULL) is safe, so one cleanup serves every path
    free(packed);
    return code;
}

static int cmd_decompress(const char *input, const char *output, int force) {
    unsigned char *packed = NULL, *data = NULL;
    size_t packed_size = 0, size = 0;
    int code = CMPZ_EXIT_OK;

    if ((code = check_output_path(input, output, force)) != CMPZ_EXIT_OK) {
        return code;
    }
    if (read_file(input, &packed, &packed_size) != 0) {
        fprintf(stderr, "cmpz: error: cannot read %s\n", input);
        return CMPZ_EXIT_IO;
    }
    if (cmpz_decompress(packed, packed_size, &data, &size) != 0) {
        fprintf(stderr, "cmpz: error: %s is not a valid CMPZ file, or it is damaged\n", input);
        code = CMPZ_EXIT_BAD_FILE;
        goto done;
    }
    if (write_file(output, data, size) != 0) {      // only now, after everything succeeded
        fprintf(stderr, "cmpz: error: cannot write %s\n", output);
        code = CMPZ_EXIT_IO;
        goto done;
    }
    printf("%s: %llu -> %llu bytes\n", input, (unsigned long long)packed_size,
           (unsigned long long)size);

done:
    free(packed);
    free(data);
    return code;
}

static int cmd_test(const char *input) {
    unsigned char *packed = NULL, *data = NULL;
    size_t packed_size = 0, size = 0;
    int code = CMPZ_EXIT_OK;

    if (read_file(input, &packed, &packed_size) != 0) {
        fprintf(stderr, "cmpz: error: cannot read %s\n", input);
        return CMPZ_EXIT_IO;
    }
    if (cmpz_decompress(packed, packed_size, &data, &size) != 0) {
        fprintf(stderr, "cmpz: error: %s is not a valid CMPZ file, or it is damaged\n", input);
        code = CMPZ_EXIT_BAD_FILE;
    } else {
        printf("%s: OK (%llu bytes when restored)\n", input, (unsigned long long)size);
    }
    free(packed);
    free(data);                            // the restored bytes are thrown away: nothing is written
    return code;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(stderr);
        return CMPZ_EXIT_USAGE;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(stdout);
        return CMPZ_EXIT_OK;
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
        printf("cmpz version %s\n", CMPZ_VERSION_STRING);
        return CMPZ_EXIT_OK;
    }

    const char *command = argv[1];
    const char *paths[2];
    int path_count = 0;
    int force = 0;

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            force = 1;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "cmpz: error: unknown option %s\n", argv[i]);
            return CMPZ_EXIT_USAGE;
        } else if (path_count < 2) {
            paths[path_count++] = argv[i];
        } else {
            fprintf(stderr, "cmpz: error: too many arguments\n");
            return CMPZ_EXIT_USAGE;
        }
    }

    if (strcmp(command, "compress") == 0 && path_count == 2) {
        return cmd_compress(paths[0], paths[1], force);
    }
    if (strcmp(command, "decompress") == 0 && path_count == 2) {
        return cmd_decompress(paths[0], paths[1], force);
    }
    if (strcmp(command, "test") == 0 && path_count == 1) {
        return cmd_test(paths[0]);
    }

    fprintf(stderr, "cmpz: error: bad command or wrong number of arguments\n\n");
    print_usage(stderr);
    return CMPZ_EXIT_USAGE;
}
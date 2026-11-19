/* e2e_cli.c - end-to-end tests: runs the real cmpz program on real files.
 * Run with: mingw32-make e2e        (the program path is passed in as argv[1])
 *
 * All temporary files live in build/ with the prefix e2e_, so "make clean" removes them. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

static const char *exe_path = "build/cmpz";
static int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                                \
        }                                                              \
    } while (0)

#define LOG "build/e2e_log.txt"

/* ---------- small helpers ---------- */

static uint32_t lcg_state = 31337u;
static uint32_t next_random(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

static int write_bytes(const char *path, const unsigned char *data, size_t n) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) return -1;
    if (n > 0 && fwrite(data, 1, n, f) != n) { fclose(f); return -1; }
    return fclose(f);
}

/* Reads a whole file into a new buffer (never NULL on success). */
static int read_bytes(const char *path, unsigned char **data, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return -1;
    size_t cap = 4096, len = 0;
    unsigned char *buf = malloc(cap);
    if (buf == NULL) { fclose(f); return -1; }
    for (;;) {
        if (len == cap) {
            unsigned char *bigger = realloc(buf, cap * 2);
            if (bigger == NULL) { free(buf); fclose(f); return -1; }
            buf = bigger;
            cap *= 2;
        }
        size_t got = fread(buf + len, 1, cap - len, f);
        if (got == 0) break;
        len += got;
    }
    fclose(f);
    *data = buf;
    *n = len;
    return 0;
}

static int exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
}

/* true when both files exist and hold exactly the same bytes */
static int same_contents(const char *a, const char *b) {
    unsigned char *da = NULL, *db = NULL;
    size_t na = 0, nb = 0;
    int same = 0;
    if (read_bytes(a, &da, &na) == 0 && read_bytes(b, &db, &nb) == 0) {
        same = (na == nb) && (na == 0 || memcmp(da, db, na) == 0);
    }
    free(da);
    free(db);
    return same;
}

static void remove_if_exists(const char *path) {
    remove(path);
}

/* Runs "cmpz <args>", saves its output to LOG, returns its exit code (or -1). */
static int run(const char *args) {
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s %s > %s 2>&1", exe_path, args, LOG);
#ifdef _WIN32
    for (char *p = cmd; *p; ++p) {       /* cmd.exe wants backslashes in program paths */
        if (*p == '/') *p = '\\';
    }
    return system(cmd);
#else
    int status = system(cmd);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

/* true when the last command's output contains the text */
static int log_contains(const char *text) {
    unsigned char *data = NULL;
    size_t n = 0;
    int found = 0;
    if (read_bytes(LOG, &data, &n) == 0) {
        char *copy = malloc(n + 1);
        if (copy != NULL) {
            memcpy(copy, data, n);
            copy[n] = '\0';
            found = strstr(copy, text) != NULL;
            free(copy);
        }
    }
    free(data);
    return found;
}

/* ---------- test data ---------- */

typedef struct {
    const char *name;     /* used in the file names */
    unsigned char *data;
    size_t size;
} Sample;

static unsigned char *make_buffer(size_t n) {
    unsigned char *p = malloc(n > 0 ? n : 1);
    if (p == NULL) { printf("out of memory\n"); exit(2); }
    return p;
}

static Sample make_text(void) {
    const char *s = "the quick brown fox jumps over the lazy dog. ";
    size_t len = strlen(s), n = len * 400;
    Sample x = {"text", make_buffer(n), n};
    for (size_t i = 0; i < n; ++i) x.data[i] = (unsigned char)s[i % len];
    return x;
}

static Sample make_skewed(size_t n, const char *name) {
    Sample x = {name, make_buffer(n), n};
    for (size_t i = 0; i < n; ++i) {
        uint32_t r = (next_random() >> 16) % 1000;
        x.data[i] = (unsigned char)(r * r / 1000 * 40 / 1000);
    }
    return x;
}

static Sample make_random(size_t n) {
    Sample x = {"random", make_buffer(n), n};
    for (size_t i = 0; i < n; ++i) x.data[i] = (unsigned char)(next_random() >> 16);
    return x;
}

static Sample make_all_values(void) {
    Sample x = {"allvalues", make_buffer(768), 768};
    for (size_t i = 0; i < 768; ++i) x.data[i] = (unsigned char)(i % 256);
    return x;
}

/* ---------- tests ---------- */

/* compress -> test -> decompress -> compare, for one sample */
static void round_trip_via_cli(const Sample *s) {
    char in[128], packed[128], out[128], args[512];

    snprintf(in, sizeof in, "build/e2e_%s.in", s->name);
    snprintf(packed, sizeof packed, "build/e2e_%s.cmpz", s->name);
    snprintf(out, sizeof out, "build/e2e_%s.out", s->name);
    remove_if_exists(packed);
    remove_if_exists(out);
    CHECK(write_bytes(in, s->data, s->size) == 0);

    snprintf(args, sizeof args, "compress %s %s", in, packed);
    CHECK(run(args) == 0);
    CHECK(exists(packed));

    unsigned char *p = NULL;
    size_t pn = 0;
    CHECK(read_bytes(packed, &p, &pn) == 0);
    CHECK(pn <= s->size + 20);                      /* never grows by more than the header */
    free(p);

    snprintf(args, sizeof args, "test %s", packed);
    CHECK(run(args) == 0);
    CHECK(log_contains("OK"));

    snprintf(args, sizeof args, "decompress %s %s", packed, out);
    CHECK(run(args) == 0);
    CHECK(same_contents(in, out));                  /* byte for byte identical */
}

static void test_round_trips(void) {
    Sample list[7];
    size_t count = 0;
    list[count++] = make_text();
    list[count++] = make_skewed(401, "skewed_small");
    list[count++] = make_skewed(300000, "skewed_big");
    list[count++] = make_random(5000);
    list[count++] = make_all_values();
    Sample empty = {"empty", make_buffer(0), 0};
    list[count++] = empty;
    Sample one = {"one", make_buffer(1), 1};
    one.data[0] = 'a';
    list[count++] = one;

    for (size_t i = 0; i < count; ++i) {
        round_trip_via_cli(&list[i]);
        free(list[i].data);
    }
}

static void test_overwrite_rules(void) {
    const char *msg = "hello hello hello hello";
    CHECK(write_bytes("build/e2e_ow.in", (const unsigned char *)msg, strlen(msg)) == 0);
    remove_if_exists("build/e2e_ow.cmpz");

    CHECK(run("compress build/e2e_ow.in build/e2e_ow.cmpz") == 0);
    unsigned char *before = NULL;
    size_t nb = 0;
    CHECK(read_bytes("build/e2e_ow.cmpz", &before, &nb) == 0);

    CHECK(run("compress build/e2e_ow.in build/e2e_ow.cmpz") == 2);   /* exists: refused */
    CHECK(log_contains("already exists"));
    CHECK(write_bytes("build/e2e_ow.copy", before, nb) == 0);
    CHECK(same_contents("build/e2e_ow.cmpz", "build/e2e_ow.copy"));  /* and left untouched */

    CHECK(run("compress -f build/e2e_ow.in build/e2e_ow.cmpz") == 0);  /* -f allows it */
    CHECK(run("compress --force build/e2e_ow.in build/e2e_ow.cmpz") == 0);
    CHECK(run("compress build/e2e_ow.in build/e2e_ow.in") == 1);       /* same file */
    CHECK(log_contains("same file"));
    free(before);
}

static void test_errors_and_usage(void) {
    remove_if_exists("build/e2e_none.cmpz");
    CHECK(run("compress build/e2e_missing.txt build/e2e_none.cmpz") == 2);
    CHECK(log_contains("cannot read"));
    CHECK(!exists("build/e2e_none.cmpz"));                            /* no output on failure */

    CHECK(run("decompress build/e2e_missing.cmpz build/e2e_none.out") == 2);
    CHECK(run("test build/e2e_missing.cmpz") == 2);

    CHECK(write_bytes("build/e2e_plain.txt", (const unsigned char *)"not a cmpz file at all, just text", 33) == 0);
    remove_if_exists("build/e2e_plain.out");
    CHECK(run("decompress build/e2e_plain.txt build/e2e_plain.out") == 3);
    CHECK(log_contains("not a valid CMPZ file"));
    CHECK(!exists("build/e2e_plain.out"));
    CHECK(run("test build/e2e_plain.txt") == 3);

    /* the output cannot be created: a folder that does not exist */
    CHECK(run("compress build/e2e_plain.txt build/no_such_folder/x.cmpz") == 2);
    CHECK(log_contains("cannot write"));

    CHECK(run("") == 1);                                              /* no arguments */
    CHECK(run("frobnicate a b") == 1);
    CHECK(run("compress onlyone") == 1);
    CHECK(run("compress a b c") == 1);
    CHECK(run("compress --nope a b") == 1);
    CHECK(run("test a b") == 1);

    CHECK(run("--help") == 0);
    CHECK(log_contains("Usage"));
    CHECK(run("--version") == 0);
    CHECK(log_contains("cmpz version"));
}

/* A damaged compressed file is refused with exit code 3 and no output is created. */
static void test_damaged_files(void) {
    Sample s = make_skewed(401, "dmg");
    unsigned char *packed = NULL;
    size_t pn = 0;
    char args[512];

    CHECK(write_bytes("build/e2e_dmg.in", s.data, s.size) == 0);
    remove_if_exists("build/e2e_dmg.cmpz");
    CHECK(run("compress build/e2e_dmg.in build/e2e_dmg.cmpz") == 0);
    CHECK(read_bytes("build/e2e_dmg.cmpz", &packed, &pn) == 0);
    if (packed == NULL) { free(s.data); return; }

    /* flip one bit in every byte of the file (the bit changes from byte to byte) */
    int undetected = 0;
    for (size_t i = 0; i < pn; ++i) {
        packed[i] ^= (unsigned char)(1u << (i % 8));
        CHECK(write_bytes("build/e2e_dmg.bad", packed, pn) == 0);
        if (run("test build/e2e_dmg.bad") != 3) undetected++;
        packed[i] ^= (unsigned char)(1u << (i % 8));
    }
    CHECK(undetected == 0);

    /* truncated at several places, and with extra bytes appended */
    size_t cuts[] = {0, 1, 19, 20, 100, 275, 276, pn - 1};
    for (size_t k = 0; k < sizeof cuts / sizeof cuts[0]; ++k) {
        CHECK(write_bytes("build/e2e_dmg.bad", packed, cuts[k]) == 0);
        CHECK(run("test build/e2e_dmg.bad") == 3);
    }
    unsigned char *longer = make_buffer(pn + 1);
    memcpy(longer, packed, pn);
    longer[pn] = 0;
    CHECK(write_bytes("build/e2e_dmg.bad", longer, pn + 1) == 0);
    CHECK(run("test build/e2e_dmg.bad") == 3);
    free(longer);

    /* decompress of a damaged file: exit 3, and an EXISTING output is not touched, even with -f */
    packed[pn - 1] ^= 0x40;
    CHECK(write_bytes("build/e2e_dmg.bad", packed, pn) == 0);
    CHECK(write_bytes("build/e2e_dmg.keep", (const unsigned char *)"precious", 8) == 0);
    snprintf(args, sizeof args, "decompress -f build/e2e_dmg.bad build/e2e_dmg.keep");
    CHECK(run(args) == 3);
    unsigned char *kept = NULL;
    size_t kn = 0;
    CHECK(read_bytes("build/e2e_dmg.keep", &kept, &kn) == 0);
    CHECK(kn == 8 && kept != NULL && memcmp(kept, "precious", 8) == 0);
    free(kept);

    free(packed);
    free(s.data);
}

int main(int argc, char *argv[]) {
    if (argc >= 2) {
        exe_path = argv[1];
    }
    if (!exists(exe_path) && !exists("build/cmpz.exe")) {
        printf("cannot find the program at %s (build it first)\n", exe_path);
        return 2;
    }
    test_round_trips();
    test_overwrite_rules();
    test_errors_and_usage();
    test_damaged_files();
    if (failures == 0) { printf("e2e: all tests passed\n"); return 0; }
    printf("e2e: %d check(s) FAILED\n", failures);
    return 1;
}
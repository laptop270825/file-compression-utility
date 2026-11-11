#include<stdio.h>
#include<stdlib.h>
#include<fileio.h>

int read_file(const char *path, unsigned char **data, size_t *size) {
    if ((path == NULL) || (data == NULL) || (size == NULL)) {
        return -1; // Return an error code for invalid input
    }

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return -1; // Return an error code if file cannot be opened
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1; // Return an error code if seeking fails
    }
    long n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1; // Return an error code if size or rewind fails
    }
    size_t len = (size_t)n;

    unsigned char *buffer = malloc(len > 0 ? len : 1);
    if (buffer == NULL) {
        fclose(f);
        return -1; // Return an error code if memory allocation fails
    }

    if (fread(buffer, 1, len, f) != len) {
        free(buffer);
        fclose(f);
        return -1; // Return an error code if reading fails
    }

    fclose(f);
    *data = buffer;
    *size = len;
    return 0; // Return success code
}

int write_file(const char *path, const unsigned char *data, size_t size) {
    if ((path == NULL) || ((data == NULL) && (size > 0))) {
        return -1; // Return an error code for invalid input
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return -1; // Return an error code if file cannot be opened
    }

    while(size > 0) {
        size_t written = fwrite(data, 1, size, f);
        if (written == 0) {
            fclose(f);
            return -1; // Return an error code if writing fails
        }
        data += written;
        size -= written;
    }

    if (fclose(f) != 0) {
        return -1; // Return an error code if closing fails
    }
    return 0; // Return success code
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

static uint8_t buffer[1024];

static const char *test_case_filename = "test_case.dat";
static const char *missing_filename = "256ece7c-1a77-4031-b0dd-c3cbd98ba971";

static void cleanup() {
    unlink(test_case_filename);
}

static void test_file_io()
{
    int ret;
    memset(buffer, 0xa5, sizeof(buffer));

    int fd = open(test_case_filename, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    assert(fd != -1);

    ret = write(fd, buffer, sizeof(buffer));
    assert(ret != -1);

    ret = close(fd);
    assert(ret != -1);

    atexit(cleanup);

    DIR *dir = opendir(".");
    if (!dir) {
        perror("directory open failed");
        exit(3);
    }

    bool found_case = false;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir))) {
        found_case = found_case || (strcmp(entry->d_name, test_case_filename) == 0);
    }
    assert(found_case);
    closedir(dir);

    static uint8_t read_buffer[1024];
    memset(read_buffer, 0x99, sizeof(read_buffer));
    int read_fd = open(test_case_filename, O_RDONLY);
    int bytes_read = read(read_fd, read_buffer, sizeof(read_buffer));
    assert(bytes_read == sizeof(read_buffer));

    close(read_fd);

    assert(memcmp(read_buffer, buffer, sizeof(buffer)) == 0);
}

static void test_missing_file()
{
    int fd = open(missing_filename, O_RDONLY);
    assert(errno == ENOENT);
    assert(fd == -1);
}

int main()
{
    test_file_io();
    test_missing_file();
    printf("PASS\n");
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include "../include/files.h"
#include "../include/mime.h"

static int resolve_safe_path(const char *requested_path, char *resolved_path) {
    char base_dir[PATH_MAX];
    char full_path[PATH_MAX];

    if (realpath("www", base_dir) == NULL) {
        return -3;
    }

    int n = snprintf(full_path, sizeof(full_path), "%s%s", base_dir, requested_path);
    if (n < 0 || (size_t)n >= sizeof(full_path)) {
        return -4;
    }

    if (realpath(full_path, resolved_path) == NULL) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return -1;
        }
        if (errno == EACCES) {
            return -2;
        }
        return -3;
    }

    size_t base_len = strlen(base_dir);
    if (strncmp(base_dir, resolved_path, base_len) != 0 ||
        (resolved_path[base_len] != '/' && resolved_path[base_len] != '\0')) {
        return -2;
    }

    return 0;
}

int read_static_file(const char *uri, char **buffer, off_t *file_size, const char **mime_type) {
    if (!uri || !buffer || !file_size || !mime_type) {
        return -3;
    }

    char clean_uri[PATH_MAX];
    int n = snprintf(clean_uri, sizeof(clean_uri), "%s", uri);
    if (n < 0 || (size_t)n >= sizeof(clean_uri)) {
        return -4;
    }

    char *query = strchr(clean_uri, '?');
    if (query != NULL) {
        *query = '\0';
    }

    char rel_path[PATH_MAX];
    if (strcmp(clean_uri, "/") == 0) {
        n = snprintf(rel_path, sizeof(rel_path), "/index.html");
    } else {
        n = snprintf(rel_path, sizeof(rel_path), "%s", clean_uri);
    }

    if (n < 0 || (size_t)n >= sizeof(rel_path)) {
        return -4;
    }

    char safe_filepath[PATH_MAX];
    int path_status = resolve_safe_path(rel_path, safe_filepath);
    if (path_status != 0) {
        return path_status;
    }

    int file_fd = open(safe_filepath, O_RDONLY);
    if (file_fd < 0) {
        if (errno == ENOENT) return -1;
        if (errno == EACCES) return -2;
        return -3;
    }

    struct stat st;
    if (fstat(file_fd, &st) < 0) {
        close(file_fd);
        return -3;
    }

    if (!S_ISREG(st.st_mode)) {
        close(file_fd);
        return -2;
    }

    *file_size = st.st_size;
    *mime_type = get_mime_type(safe_filepath);

    if (*file_size == 0) {
        *buffer = NULL;
        close(file_fd);
        return 0;
    }

    *buffer = malloc((size_t)*file_size);
    if (!(*buffer)) {
        close(file_fd);
        return -3;
    }

    size_t total_read = 0;
    while (total_read < (size_t)*file_size) {
        ssize_t bytes_read = read(file_fd, *buffer + total_read, (size_t)*file_size - total_read);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno == EINTR) continue;
            free(*buffer);
            *buffer = NULL;
            close(file_fd);
            return -3;
        }
        total_read += (size_t)bytes_read;
    }

    close(file_fd);
    return 0;
}

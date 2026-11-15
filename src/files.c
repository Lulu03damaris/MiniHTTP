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

static int is_safe_path(const char *requested_path, char *resolved_path) {
    char base_dir[PATH_MAX];
    char full_path[PATH_MAX];

    if (realpath("www", base_dir) == NULL) return 0;
    snprintf(full_path, sizeof(full_path), "%s%s", base_dir, requested_path);
    if (realpath(full_path, resolved_path) == NULL) return 0;
    if (strncmp(base_dir, resolved_path, strlen(base_dir)) == 0) return 1;

    return 0;
}

int read_static_file(const char *uri, char **buffer, off_t *file_size, const char **mime_type) {
    char rel_path[256];

    /* Unificar asignación de ruta evitando strcpy */
    if (strcmp(uri, "/") == 0) {
        snprintf(rel_path, sizeof(rel_path), "/index.html");
    } else {
        snprintf(rel_path, sizeof(rel_path), "%s", uri);
    }

    char safe_filepath[PATH_MAX];
    if (!is_safe_path(rel_path, safe_filepath)) {
        return -2; /* Intentos de Directory Traversal / Acceso no permitido */
    }

    int file_fd = open(safe_filepath, O_RDONLY);
    if (file_fd < 0) {
        if (errno == ENOENT) return -1; /* 404 Not Found */
        if (errno == EACCES) return -2; /* 403 Forbidden */
        return -3;                      /* 500 Internal Error */
    }

    struct stat st;
    if (fstat(file_fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(file_fd);
        return -2; /* Los directorios o archivos especiales retornan Forbidden */
    }

    *file_size = st.st_size;
    *mime_type = get_mime_type(safe_filepath);

    *buffer = malloc(*file_size);
    if (!(*buffer)) {
        close(file_fd);
        return -3;
    }

    ssize_t bytes_read = read(file_fd, *buffer, *file_size);
    close(file_fd);

    if (bytes_read != *file_size) {
        free(*buffer);
        *buffer = NULL;
        return -3;
    }

    return 0;
}

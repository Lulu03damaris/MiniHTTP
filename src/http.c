#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include "../include/http.h"
#include "../include/mime.h"
#include "../include/files.h"

#define BUFSZ 2048

typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_UNSUPPORTED
} http_method_t;

static http_method_t parse_method(const char *method_str) {
    if (strcmp(method_str, "GET") == 0) {
        return HTTP_METHOD_GET;
    }
    return HTTP_METHOD_UNSUPPORTED;
}

static void send_response(int fd, int status_code, const char *status_text, const char *content_type, const char *body, long body_len) {
    char header[BUFSZ];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: keep-alive\r\n\r\n",
        status_code, status_text, content_type, body_len);

    if (header_len > 0) {
        (void)write(fd, header, header_len);
    }
    if (body && body_len > 0) {
        (void)write(fd, body, body_len);
    }
}

void handle_http_request(int client_fd) {
    char buf[BUFSZ];
    int nread = read(client_fd, buf, sizeof(buf) - 1);

    if (nread <= 0) return;
    buf[nread] = '\0';

    char method_str[16];
    char uri[256];
    char protocol[16];

    /* 1. SEGURO: Ancho limite en sscanf (%15s, %255s) para evitar Buffer Overflow */
    if (sscanf(buf, "%15s %255s %15s", method_str, uri, protocol) < 3) {
        send_response(client_fd, 400, "Bad Request", "text/html", "<h1>400 Bad Request</h1>", 24);
        return;
    }

    http_method_t method = parse_method(method_str);

    switch (method) {
        case HTTP_METHOD_GET: {
            char rel_path[256];

            /* 2. UNIFICADO Y SIN strcpy: Uso seguro de snprintf */
            if (strcmp(uri, "/") == 0) {
                snprintf(rel_path, sizeof(rel_path), "/index.html");
            } else {
                snprintf(rel_path, sizeof(rel_path), "%s", uri);
            }

            /* Validar vulnerabilidad de Directory Traversal */
            char safe_filepath[PATH_MAX];
            if (!is_safe_path(rel_path, safe_filepath)) {
                send_response(client_fd, 403, "Forbidden", "text/html", "<h1>403 Forbidden</h1>", 22);
                return;
            }

            /* 3. MANEJO EXPLICITO DE ERRNO: Distinguir entre 404, 403 y 500 */
            int file_fd = open(safe_filepath, O_RDONLY);
            if (file_fd < 0) {
                if (errno == ENOENT) {
                    send_response(client_fd, 404, "Not Found", "text/html", "<h1>404 Not Found</h1>", 22);
                } else if (errno == EACCES) {
                    send_response(client_fd, 403, "Forbidden", "text/html", "<h1>403 Forbidden</h1>", 22);
                } else {
                    send_response(client_fd, 500, "Internal Server Error", "text/html", "<h1>500 Internal Server Error</h1>", 34);
                }
                return;
            }

            /* 4. VERIFICAR ARCHIVO REGULAR (Evita EISDIR al intentar leer directorios) */
            struct stat st;
            if (fstat(file_fd, &st) < 0 || !S_ISREG(st.st_mode)) {
                close(file_fd);
                send_response(client_fd, 403, "Forbidden", "text/html", "<h1>403 Forbidden</h1>", 22);
                return;
            }

            /* Transmisión de respuesta exitosa 200 OK */
            const char *mime = get_mime_type(safe_filepath);
            send_response(client_fd, 200, "OK", mime, NULL, st.st_size);

            while ((nread = read(file_fd, buf, sizeof(buf))) > 0) {
                (void)write(client_fd, buf, nread);
            }

            close(file_fd);
            break;
        }

        case HTTP_METHOD_UNSUPPORTED:
        default:
            send_response(client_fd, 405, "Method Not Allowed", "text/html", "<h1>405 Method Not Allowed</h1>", 31);
            break;
    }
}

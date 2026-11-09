#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include "../include/http.h"
#include "../include/mime.h"
#include "../include/files.h"

#define BUFSZ 2048

/* Definición de métodos HTTP soportados o reconocidos */
typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_UNSUPPORTED
} http_method_t;

/* Función auxiliar para mapear el string del método al enum */
static http_method_t parse_method(const char *method_str) {
    if (strcmp(method_str, "GET") == 0) {
        return HTTP_METHOD_GET;
    }
    return HTTP_METHOD_UNSUPPORTED;
}

/* Función auxiliar para enviar respuestas HTTP con sus encabezados */
static void send_response(int fd, int status_code, const char *status_text, const char *content_type, const char *body, long body_len) {
    char header[BUFSZ];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: keep-alive\r\n\r\n",
        status_code, status_text, content_type, body_len);

    write(fd, header, header_len);
    if (body && body_len > 0) {
        write(fd, body, body_len);
    }
}

void handle_http_request(int client_fd) {
    char buf[BUFSZ];
    int nread = read(client_fd, buf, sizeof(buf) - 1);

    if (nread <= 0) return;
    buf[nread] = '\0';

    char method_str[16], uri[256], protocol[16];
    
    /* 1. Parsing de la línea de solicitud */
 u[   if (sscanf(buf, "%s %s %s", method_str, uri, protocol) < 3) {
        send_response(client_fd, 400, "Bad Request", "text/html", "<h1>400 Bad Request</h1>", 24);
        return;
    }

    /* 2. Convertir string de método a Enum para usar SWITCH */
    http_method_t method = parse_method(method_str);

    switch (method) {
        case HTTP_METHOD_GET: {
            /* Determinar ruta relativa */
            char rel_path[256];
            if (strcmp(uri, "/") == 0) {
                strcpy(rel_path, "/index.html");
            } else {
                strncpy(rel_path, uri, sizeof(rel_path) - 1);
                rel_path[sizeof(rel_path) - 1] = '\0';
            }

            /* Validar seguridad (Evitar Directory Traversal con realpath) */
            char safe_filepath[PATH_MAX];
            if (!is_safe_path(rel_path, safe_filepath)) {
                send_response(client_fd, 403, "Forbidden", "text/html", "<h1>403 Forbidden</h1>", 22);
                return;
            }

            /* Intentar abrir el archivo estático */
            int file_fd = open(safe_filepath, O_RDONLY);
            if (file_fd < 0) {
                send_response(client_fd, 404, "Not Found", "text/html", "<h1>404 Not Found</h1>", 22);
                return;
            }

            struct stat st;
            fstat(file_fd, &st);

            /* Obtener tipo MIME y responder con 200 OK */
            const char *mime = get_mime_type(safe_filepath);
            send_response(client_fd, 200, "OK", mime, NULL, st.st_size);

            /* Transmitir el contenido del archivo */
            while ((nread = read(file_fd, buf, sizeof(buf))) > 0) {
                write(client_fd, buf, nread);
            }

            close(file_fd);
            break;
        }

        case HTTP_METHOD_UNSUPPORTED:
        default:
            /* Rechazar POST, PUT, DELETE, etc. con 405 Method Not Allowed */
            send_response(client_fd, 405, "Method Not Allowed", "text/html", "<h1>405 Method Not Allowed</h1>", 31);
            break;
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include "../include/http.h"
#include "../include/files.h"
#include "../include/server.h"

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

static void prepare_error_response(client_conn_t *conn, int status_code, const char *status_text,
                                   const char *extra_headers, const char *body_html, bool keep_alive) {
    conn->keep_alive = keep_alive;
    conn->body_buf = (char *)body_html;
    conn->body_len = strlen(body_html);
    conn->body_sent = 0;
    conn->body_is_dynamic = false;

    int n = snprintf(conn->resp_hdr, sizeof(conn->resp_hdr),
        "HTTP/1.1 %d %s\r\n"
        "%s"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n\r\n",
        status_code, status_text,
        extra_headers ? extra_headers : "",
        conn->body_len,
        conn->keep_alive ? "keep-alive" : "close");

    conn->hdr_len = (n > 0 && (size_t)n < sizeof(conn->resp_hdr)) ? (size_t)n : 0;
    conn->hdr_sent = 0;
    conn->writing = true;
}

static bool check_keep_alive(const char *headers_start, const char *headers_end, const char *protocol) {
    bool is_http11 = (strcmp(protocol, "HTTP/1.1") == 0);
    const char *p = headers_start;

    while (p < headers_end) {
        const char *line_end = strstr(p, "\r\n");
        if (!line_end || line_end > headers_end) break;

        if (strncasecmp(p, "Connection:", 11) == 0) {
            const char *val = p + 11;
            while (val < line_end && (*val == ' ' || *val == '\t')) val++;
            if (strncasecmp(val, "close", 5) == 0) return false;
            if (strncasecmp(val, "keep-alive", 10) == 0) return true;
        }
        p = line_end + 2;
    }
    return is_http11;
}

int handle_http_request(client_conn_t *conn) {
    conn->req_buf[conn->req_len] = '\0';

    char *headers_end = strstr(conn->req_buf, "\r\n\r\n");
    if (!headers_end) {
        if (conn->req_len >= REQ_BUF_SIZE - 1) {
            prepare_error_response(conn, 400, "Bad Request", NULL, "<h1>400 Request Too Large</h1>", false);
            return 1;
        }
        return 0;
    }

    size_t total_req_bytes = (size_t)((headers_end + 4) - conn->req_buf);

    char *first_line_end = strstr(conn->req_buf, "\r\n");
    char *first_space = strchr(conn->req_buf, ' ');
    if (!first_space || first_space > first_line_end) {
        prepare_error_response(conn, 400, "Bad Request", NULL, "<h1>400 Bad Request</h1>", false);
        return 1;
    }

    char *second_space = strchr(first_space + 1, ' ');
    if (!second_space || second_space > first_line_end || (second_space - (first_space + 1)) > 255) {
        prepare_error_response(conn, 400, "Bad Request", NULL, "<h1>400 URI Too Long</h1>", false);
        return 1;
    }

    char method_str[16], uri[256], protocol[16];
    if (sscanf(conn->req_buf, "%15s %255s %15s", method_str, uri, protocol) < 3) {
        prepare_error_response(conn, 400, "Bad Request", NULL, "<h1>400 Bad Request</h1>", false);
        return 1;
    }

    if (strcmp(protocol, "HTTP/1.1") != 0 && strcmp(protocol, "HTTP/1.0") != 0) {
        prepare_error_response(conn, 400, "Bad Request", NULL, "<h1>400 Bad Request</h1>", false);
        return 1;
    }

    bool keep_alive = check_keep_alive(first_line_end + 2, headers_end, protocol);

    size_t remaining = conn->req_len - total_req_bytes;
    if (remaining > 0) {
        memmove(conn->req_buf, conn->req_buf + total_req_bytes, remaining);
    }
    conn->req_len = remaining;

    http_method_t method = parse_method(method_str);

    switch (method) {
        case HTTP_METHOD_GET: {
            char *file_buffer = NULL;
            off_t file_size = 0;
            const char *mime_type = NULL;

            int result = read_static_file(uri, &file_buffer, &file_size, &mime_type);

            if (result == -4) {
                prepare_error_response(conn, 400, "Bad Request", NULL, "<h1>400 Bad Request</h1>", false);
            } else if (result == -1) {
                prepare_error_response(conn, 404, "Not Found", NULL, "<h1>404 Not Found</h1>", keep_alive);
            } else if (result == -2) {
                prepare_error_response(conn, 403, "Forbidden", NULL, "<h1>403 Forbidden</h1>", keep_alive);
            } else if (result == -3) {
                prepare_error_response(conn, 500, "Internal Server Error", NULL, "<h1>500 Internal Server Error</h1>", false);
            } else if (result == 0) {
                conn->keep_alive = keep_alive;
                conn->body_buf = file_buffer;
                conn->body_len = (size_t)file_size;
                conn->body_sent = 0;
                conn->body_is_dynamic = true;

                int n = snprintf(conn->resp_hdr, sizeof(conn->resp_hdr),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: %s\r\n\r\n",
                    mime_type, conn->body_len,
                    conn->keep_alive ? "keep-alive" : "close");

                if (n < 0 || (size_t)n >= sizeof(conn->resp_hdr)) {
                    free(file_buffer);
                    prepare_error_response(conn, 500, "Internal Server Error", NULL, "<h1>500 Internal Server Error</h1>", false);
                    return 1;
                }

                conn->hdr_len = (size_t)n;
                conn->hdr_sent = 0;
                conn->writing = true;
            }
            return 1;
        }

        case HTTP_METHOD_UNSUPPORTED:
        default:
            prepare_error_response(conn, 405, "Method Not Allowed", "Allow: GET\r\n", "<h1>405 Method Not Allowed</h1>", keep_alive);
            return 1;
    }
}

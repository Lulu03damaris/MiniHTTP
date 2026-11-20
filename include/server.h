#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#define REQ_BUF_SIZE 4096
#define HDR_BUF_SIZE 1024

typedef struct client_conn {
    int fd;

    char req_buf[REQ_BUF_SIZE];
    size_t req_len;

    char resp_hdr[HDR_BUF_SIZE];
    size_t hdr_len;
    size_t hdr_sent;

    char *body_buf;
    size_t body_len;
    size_t body_sent;
    bool body_is_dynamic;

    bool writing;
    bool keep_alive;
} client_conn_t;

void run_server(const char *port);

#endif

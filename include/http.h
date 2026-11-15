#ifndef HTTP_H
#define HTTP_H

/* Declaración adelantada: evita dependencias circulares con server.h */
typedef struct client_conn client_conn_t;

int handle_http_request(client_conn_t *conn);

#endif

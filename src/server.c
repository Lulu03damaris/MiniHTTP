#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../include/server.h"
#include "../include/http.h"

#define MAX_EVENTS 128
#define QLEN 64

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void close_connection(int epoll_fd, client_conn_t *conn) {
    if (!conn) return;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
    close(conn->fd);
    if (conn->body_is_dynamic && conn->body_buf) {
        free(conn->body_buf);
    }
    free(conn);
}

static void reset_conn_response(client_conn_t *conn) {
    if (conn->body_is_dynamic && conn->body_buf) {
        free(conn->body_buf);
    }
    conn->body_buf = NULL;
    conn->body_len = 0;
    conn->body_sent = 0;
    conn->body_is_dynamic = false;
    conn->hdr_len = 0;
    conn->hdr_sent = 0;
    conn->writing = false;
}

static int create_listen_socket(const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(NULL, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "Error en getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) continue;

        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd < 0 || listen(sockfd, QLEN) < 0) {
        if (sockfd >= 0) close(sockfd);
        return -1;
    }

    if (set_nonblocking(sockfd) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

static int flush_response(client_conn_t *conn) {
    while (conn->hdr_sent < conn->hdr_len) {
        ssize_t n = write(conn->fd, conn->resp_hdr + conn->hdr_sent, conn->hdr_len - conn->hdr_sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        if (n == 0) return -1;
        conn->hdr_sent += (size_t)n;
    }

    while (conn->body_sent < conn->body_len) {
        ssize_t n = write(conn->fd, conn->body_buf + conn->body_sent, conn->body_len - conn->body_sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        if (n == 0) return -1;
        conn->body_sent += (size_t)n;
    }

    return 1;
}

void run_server(const char *port) {
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = create_listen_socket(port);
    if (listen_fd < 0) {
        fprintf(stderr, "No se pudo iniciar el socket de escucha en puerto %s\n", port);
        exit(EXIT_FAILURE);
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    client_conn_t listen_conn;
    memset(&listen_conn, 0, sizeof(listen_conn));
    listen_conn.fd = listen_fd;

    struct epoll_event ev, events[MAX_EVENTS];
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.ptr = &listen_conn;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl listen_fd");
        close(listen_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor MiniHTTPd activo en puerto %s (Non-blocking + Keep-Alive)...\n", port);

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            client_conn_t *conn = (client_conn_t *)events[i].data.ptr;
            uint32_t revents = events[i].events;

            if (conn->fd == listen_fd) {
                while (1) {
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int client_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        if (errno == EINTR) continue;
                        perror("accept");
                        break;
                    }

                    if (set_nonblocking(client_fd) < 0) {
                        close(client_fd);
                        continue;
                    }

                    client_conn_t *new_conn = calloc(1, sizeof(client_conn_t));
                    if (!new_conn) {
                        close(client_fd);
                        continue;
                    }
                    new_conn->fd = client_fd;

                    struct epoll_event cli_ev;
                    memset(&cli_ev, 0, sizeof(cli_ev));
                    cli_ev.events = EPOLLIN | EPOLLRDHUP;
                    cli_ev.data.ptr = new_conn;

                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &cli_ev) < 0) {
                        close(client_fd);
                        free(new_conn);
                    }
                }
                continue;
            }

            if (revents & (EPOLLERR | EPOLLHUP)) {
                close_connection(epoll_fd, conn);
                continue;
            }

            if ((revents & EPOLLIN) && !conn->writing) {
                bool peer_closed = false;

                while (conn->req_len < REQ_BUF_SIZE - 1) {
                    ssize_t n = read(conn->fd, conn->req_buf + conn->req_len,
                                     (REQ_BUF_SIZE - 1) - conn->req_len);
                    if (n < 0) {
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        peer_closed = true;
                        break;
                    }
                    if (n == 0) {
                        peer_closed = true;
                        break;
                    }
                    conn->req_len += (size_t)n;
                }

                if (peer_closed && conn->req_len == 0) {
                    close_connection(epoll_fd, conn);
                    continue;
                }

                int status = handle_http_request(conn);
                if (status < 0) {
                    close_connection(epoll_fd, conn);
                    continue;
                }
                if (status == 1) {
                    struct epoll_event mod_ev;
                    memset(&mod_ev, 0, sizeof(mod_ev));
                    mod_ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
                    mod_ev.data.ptr = conn;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &mod_ev) < 0) {
                        close_connection(epoll_fd, conn);
                        continue;
                    }
                } else if (peer_closed || (revents & EPOLLRDHUP)) {
                    close_connection(epoll_fd, conn);
                    continue;
                }
            }

            if ((revents & EPOLLOUT) && conn->writing) {
                int flush_res = flush_response(conn);
                if (flush_res < 0) {
                    close_connection(epoll_fd, conn);
                    continue;
                }

                if (flush_res == 1) {
                    bool keep = conn->keep_alive && !(revents & EPOLLRDHUP);
                    reset_conn_response(conn);

                    if (!keep) {
                        close_connection(epoll_fd, conn);
                        continue;
                    }

                    struct epoll_event mod_ev;
                    memset(&mod_ev, 0, sizeof(mod_ev));
                    mod_ev.events = EPOLLIN | EPOLLRDHUP;
                    mod_ev.data.ptr = conn;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &mod_ev) < 0) {
                        close_connection(epoll_fd, conn);
                        continue;
                    }
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
}

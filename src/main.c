#include <stdio.h>
#include <stdlib.h>
#include "../include/server.h"

int main(int argc, char *argv[]) {
    const char *port = "8080";

    if (argc > 2) {
        fprintf(stderr, "Uso: %s [puerto]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        char *endptr = NULL;
        long port_num = strtol(argv[1], &endptr, 10);
        if (*argv[1] == '\0' || *endptr != '\0' || port_num < 1 || port_num > 65535) {
            fprintf(stderr, "Error: Puerto invalido '%s'. Debe estar entre 1 y 65535.\n", argv[1]);
            return EXIT_FAILURE;
        }
        port = argv[1];
    }

    run_server(port);
    return EXIT_SUCCESS;
}

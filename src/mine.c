#include <string.h>
#include <strings.h>
#include "../include/mime.h"

typedef struct {
    const char *ext;
    const char *mime;
} MimeEntry;

/* Tabla de tipos MIME requeridos por el proyecto */
static const MimeEntry mime_table[] = {
    {".html", "text/html"},
    {".htm",  "text/html"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {NULL,    NULL}
};

const char *get_mime_type(const char *filepath) {
    if (!filepath) {
        return "application/octet-stream";
    }

    /* 1. Buscar el último '/' para aislar el nombre del archivo */
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;

    /* 2. Buscar el último '.' exclusivamente dentro del nombre del archivo */
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return "application/octet-stream";
    }

    /* 3. Comparar la extensión sin importar mayúsculas o minúsculas */
    for (int i = 0; mime_table[i].ext != NULL; i++) {
        if (strcasecmp(dot, mime_table[i].ext) == 0) {
            return mime_table[i].mime;
        }
    }

    /* Valor por defecto para extensiones desconocidas */
    return "application/octet-stream";
}

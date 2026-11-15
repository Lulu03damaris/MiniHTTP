#ifndef FILES_H
#define FILES_H

#include <sys/types.h>

/*
 * Retornos de read_static_file:
 *   0: Éxito (archivo leído correctamente)
 *  -1: No encontrado (ENOENT -> 404)
 *  -2: Prohibido / No es archivo regular / Traversal (EACCES/S_ISREG -> 403)
 *  -3: Error interno del servidor (500)
 */
int read_static_file(const char *uri, char **buffer, off_t *file_size, const char **mime_type);

#endif

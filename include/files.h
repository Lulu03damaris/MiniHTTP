#ifndef FILES_H
#define FILES_H

#include <sys/types.h>

int read_static_file(const char *uri, char **buffer, off_t *file_size, const char **mime_type);

#endif

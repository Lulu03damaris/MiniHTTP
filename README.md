minihttpd/
├── include/          # Encabezados con include guards y forward declarations
│   ├── files.h
│   ├── http.h
│   ├── mime.h
│   └── server.h
├── src/              # Implementación modular en C
│   ├── main.c        # Validación de argumentos y arranque
│   ├── server.c      # Motor epoll, sockets no bloqueantes y gestión de conexiones
│   ├── http.c        # Parseo HTTP/1.1, despacho de métodos y Keep-Alive
│   ├── files.c       # Acceso seguro al sistema de archivos (realpath + fstat)
│   └── mime.c        # Tabla de resolución de Content-Type
├── www/              # Directorio raíz público (HTML, CSS, imágenes)
├── Makefile          # Automatización de compilación (all, clean, asan)
└── README.md

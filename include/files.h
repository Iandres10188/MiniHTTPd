#ifndef FILES_H
#define FILES_H

#include <stddef.h>

/* Estados de resolución de recursos estáticos (Mapeo a HTTP) */
typedef enum {
    FILE_OK = 0,        /* Encontrado y legible -> 200 OK */
    FILE_NOT_FOUND,     /* No existe -> 404 Not Found */
    FILE_FORBIDDEN,     /* Fuera de la raíz o sin permisos -> 403 Forbidden */
    FILE_ERROR          /* Error interno del sistema -> 500 Internal Server Error */
} file_result_t;

/**
 * Resuelve y abre 'uri' de forma segura dentro de la raíz web usando realpath().
 * Evita vulnerabilidades de Directory Traversal (ej. ../../etc/passwd).
 * * NOTA: Si retorna FILE_OK, el llamador es responsable de cerrar 'out_fd'.
 */
file_result_t file_open_safe(const char *uri, int *out_fd, size_t *out_size,
                             char *resolved_path, size_t resolved_cap);

#endif /* FILES_H */

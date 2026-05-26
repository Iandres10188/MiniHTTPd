#ifndef FILES_H
#define FILES_H

#include <stddef.h>

/* Resultado de resolver/abrir un recurso estatico */
typedef enum {
    FILE_OK = 0,      /* archivo encontrado y legible       */
    FILE_NOT_FOUND,   /* no existe -> 404                   */
    FILE_FORBIDDEN,   /* fuera de la raiz o sin permiso ->403*/
    FILE_ERROR        /* error interno -> 500               */
} file_result_t;

/*
 * Resuelve de forma segura 'uri' dentro de WEB_ROOT usando realpath()
 * para evitar Directory Traversal (p.ej. /../../etc/passwd).
 *
 * En caso de exito (FILE_OK):
 *   - 'out_fd'   queda con el descriptor del archivo abierto (solo lectura)
 *   - 'out_size' queda con el tamano en bytes
 * El llamador es responsable de cerrar 'out_fd'.
 */
file_result_t file_open_safe(const char *uri, int *out_fd, size_t *out_size,
                             char *resolved_path, size_t resolved_cap);

#endif /* FILES_H */

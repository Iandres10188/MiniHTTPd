#define _GNU_SOURCE
#include "files.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/**
 * Mitiga ataques de Directory Traversal validando que el recurso solicitado
 * con realpath() se encuentre estrictamente dentro de la raíz 'WEB_ROOT'.
 */
file_result_t file_open_safe(const char *uri, int *out_fd, size_t *out_size,
                             char *resolved_path, size_t resolved_cap)
{
    char candidate[PATH_MAX];
    char root_real[PATH_MAX];
    char file_real[PATH_MAX];

    if (!realpath(WEB_ROOT, root_real))
        return FILE_ERROR;

    /* Enrutamiento por defecto */
    const char *path = uri;
    if (strcmp(uri, "/") == 0)
        path = "/index.html";

    int written = snprintf(candidate, sizeof(candidate), "%s%s", root_real, path);
    if (written < 0 || (size_t)written >= sizeof(candidate))
        return FILE_FORBIDDEN; 

    if (!realpath(candidate, file_real))
        return FILE_NOT_FOUND;

    /* Validación de contención (防 Directory Traversal) */
    size_t root_len = strlen(root_real);
    if (strncmp(file_real, root_real, root_len) != 0 ||
        (file_real[root_len] != '/' && file_real[root_len] != '\0')) {
        return FILE_FORBIDDEN;
    }

    /* Evita abrir directorios, sockets o dispositivos */
    struct stat st;
    if (stat(file_real, &st) != 0)
        return FILE_NOT_FOUND;
    if (!S_ISREG(st.st_mode))
        return FILE_FORBIDDEN;

    int fd = open(file_real, O_RDONLY);
    if (fd < 0)
        return FILE_FORBIDDEN;

    *out_fd   = fd;
    *out_size = (size_t)st.st_size;

    if (resolved_path && resolved_cap > 0) {
        snprintf(resolved_path, resolved_cap, "%s", file_real);
    }
    return FILE_OK;
}
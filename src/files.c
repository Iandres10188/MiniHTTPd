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

/*
 * Construye una ruta candidata "WEB_ROOT + uri", la canonicaliza con
 * realpath() y verifica que el resultado siga estando DENTRO de la raiz
 * canonica. Esto neutraliza ataques de Directory Traversal como
 * "GET /../../etc/passwd": aunque el cliente meta "..", realpath()
 * colapsa la ruta y la comparacion de prefijo la rechaza.
 */
file_result_t file_open_safe(const char *uri, int *out_fd, size_t *out_size,
                             char *resolved_path, size_t resolved_cap)
{
    char candidate[PATH_MAX];
    char root_real[PATH_MAX];
    char file_real[PATH_MAX];

    /* 1) Raiz canonica (WEB_ROOT debe existir). */
    if (!realpath(WEB_ROOT, root_real))
        return FILE_ERROR;

    /* 2) Si la URI es "/", servimos index.html por defecto. */
    const char *path = uri;
    if (strcmp(uri, "/") == 0)
        path = "/index.html";

    /* 3) Componemos la ruta candidata sin desbordar (snprintf, no sprintf). */
    int written = snprintf(candidate, sizeof(candidate), "%s%s", root_real, path);
    if (written < 0 || (size_t)written >= sizeof(candidate))
        return FILE_FORBIDDEN; /* ruta demasiado larga */

    /* 4) Canonicalizamos la ruta solicitada. Si no existe -> 404. */
    if (!realpath(candidate, file_real))
        return FILE_NOT_FOUND;

    /* 5) Verificacion de contencion: file_real debe empezar con root_real
     *    seguido de '/' (o ser exactamente la raiz). */
    size_t root_len = strlen(root_real);
    if (strncmp(file_real, root_real, root_len) != 0 ||
        (file_real[root_len] != '/' && file_real[root_len] != '\0')) {
        return FILE_FORBIDDEN;
    }

    /* 6) Debe ser un archivo regular (no un directorio, socket, etc.). */
    struct stat st;
    if (stat(file_real, &st) != 0)
        return FILE_NOT_FOUND;
    if (!S_ISREG(st.st_mode))
        return FILE_FORBIDDEN;

    /* 7) Abrimos en solo lectura. */
    int fd = open(file_real, O_RDONLY);
    if (fd < 0)
        return FILE_FORBIDDEN;

    *out_fd   = fd;
    *out_size = (size_t)st.st_size;

    /* Devolvemos la ruta resuelta (util para logs / MIME). */
    if (resolved_path && resolved_cap > 0) {
        snprintf(resolved_path, resolved_cap, "%s", file_real);
    }
    return FILE_OK;
}

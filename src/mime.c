#include "mime.h"
#include <string.h>
#include <strings.h>  /* strcasecmp */

/* Tabla extension -> tipo MIME */
static const struct {
    const char *ext;
    const char *type;
} mime_table[] = {
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".json", "application/json"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".svg",  "image/svg+xml"},
    {".ico",  "image/x-icon"},
    {".txt",  "text/plain; charset=utf-8"},
};

const char *mime_lookup(const char *path)
{
    /* Buscamos el ultimo '.' del nombre para obtener la extension. */
    const char *dot = strrchr(path, '.');
    if (dot) {
        size_t n = sizeof(mime_table) / sizeof(mime_table[0]);
        for (size_t i = 0; i < n; i++) {
            if (strcasecmp(dot, mime_table[i].ext) == 0)
                return mime_table[i].type;
        }
    }
    /* Tipo generico para binarios desconocidos. */
    return "application/octet-stream";
}

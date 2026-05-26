#ifndef MIME_H
#define MIME_H

/**
 * Retorna el tipo MIME según la extensión de 'path'.
 * Si no se reconoce, por defecto retorna "application/octet-stream".
 */
const char *mime_lookup(const char *path);

#endif /* MIME_H */
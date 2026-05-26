#ifndef MIME_H
#define MIME_H

/*
 * Devuelve el tipo MIME asociado a la extension del archivo 'path'.
 * Si la extension no se reconoce, devuelve "application/octet-stream".
 */
const char *mime_lookup(const char *path);

#endif /* MIME_H */

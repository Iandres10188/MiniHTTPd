#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* Resultado del parsing de la linea/encabezados de la solicitud */
typedef enum {
    PARSE_OK = 0,        /* solicitud completa y valida          */
    PARSE_INCOMPLETE,    /* faltan bytes (aun no llega \r\n\r\n)  */
    PARSE_BAD_REQUEST,   /* malformada -> 400                     */
    PARSE_NOT_IMPLEMENTED/* metodo distinto de GET -> 405         */
} parse_result_t;

/* Datos extraidos de una solicitud HTTP */
typedef struct {
    char method[16];     /* GET, POST, ...                        */
    char uri[2048];      /* ruta solicitada (sin query string)    */
    char version[16];    /* HTTP/1.0 o HTTP/1.1                    */
    int  keep_alive;     /* 1 si la conexion debe mantenerse      */
} http_request_t;

/*
 * Analiza el buffer crudo recibido.
 * Devuelve un parse_result_t y, si es PARSE_OK, llena 'req'.
 */
parse_result_t http_parse(const char *buf, size_t len, http_request_t *req);

/*
 * Atiende una solicitud ya parseada: localiza el archivo, arma
 * la respuesta y la envia por 'fd'. Devuelve 1 si la conexion
 * debe mantenerse abierta (keep-alive), 0 si debe cerrarse.
 */
int http_handle(int fd, const http_request_t *req);

/*
 * Envia una respuesta de error con cuerpo HTML minimo.
 * Devuelve 1/0 igual que http_handle segun keep_alive.
 */
int http_send_error(int fd, int status, int keep_alive);

#endif /* HTTP_H */

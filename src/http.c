#define _GNU_SOURCE
#include "http.h"
#include "files.h"
#include "mime.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Utilidades de envio                                                */
/* ------------------------------------------------------------------ */

/*
 * Envia 'len' bytes de forma confiable sobre un socket no bloqueante.
 * Reintenta ante EAGAIN/EINTR. Devuelve 0 en exito, -1 si el peer cerro.
 */
static int send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, data + sent, len - sent);
        if (n > 0) {
            sent += (size_t)n;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            continue; /* socket lleno momentaneamente: reintentar */
        } else {
            return -1; /* conexion rota */
        }
    }
    return 0;
}

/* Texto asociado a cada codigo de estado. */
static const char *status_text(int status)
{
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

/* ------------------------------------------------------------------ */
/* Respuestas de error                                                */
/* ------------------------------------------------------------------ */

int http_send_error(int fd, int status, int keep_alive)
{
    char body[512];
    char header[1024];
    const char *txt = status_text(status);

    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
        "<title>%d %s</title></head>"
        "<body><h1>%d %s</h1><hr><p>MiniHTTPd</p></body></html>\n",
        status, txt, status, txt);

    /* El 405 exige el encabezado Allow segun RFC 7231. */
    const char *allow = (status == 405) ? "Allow: GET\r\n" : "";

    int hdr_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Server: MiniHTTPd/1.0\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "%s"
        "Connection: %s\r\n"
        "\r\n",
        status, txt, body_len, allow,
        keep_alive ? "keep-alive" : "close");

    if (send_all(fd, header, (size_t)hdr_len) < 0) return 0;
    if (send_all(fd, body, (size_t)body_len) < 0)  return 0;
    return keep_alive;
}

/* ------------------------------------------------------------------ */
/* Parsing                                                            */
/* ------------------------------------------------------------------ */

parse_result_t http_parse(const char *buf, size_t len, http_request_t *req)
{
    /* La solicitud termina (cabeceras) en la secuencia "\r\n\r\n". */
    const char *end = memmem(buf, len, "\r\n\r\n", 4);
    if (!end)
        return PARSE_INCOMPLETE; /* todavia no llegaron todas las cabeceras */

    /* --- Linea de solicitud: METHOD SP URI SP VERSION --- */
    const char *line_end = memmem(buf, len, "\r\n", 2);
    size_t line_len = (size_t)(line_end - buf);
    if (line_len == 0 || line_len >= 4096)
        return PARSE_BAD_REQUEST;

    char line[4096];
    memcpy(line, buf, line_len);
    line[line_len] = '\0';

    char method[16], uri[MAX_URI], version[16];
    /* sscanf con anchos de campo acotados: evita desbordamientos. */
    if (sscanf(line, "%15s %2047s %15s", method, uri, version) != 3)
        return PARSE_BAD_REQUEST;

    /* Solo aceptamos HTTP/1.0 y HTTP/1.1. */
    if (strcmp(version, "HTTP/1.1") != 0 && strcmp(version, "HTTP/1.0") != 0)
        return PARSE_BAD_REQUEST;

    /* Solo el metodo GET esta implementado -> el resto es 405. */
    if (strcmp(method, "GET") != 0)
        return PARSE_NOT_IMPLEMENTED;

    /* Descartamos el query string (?a=b) para servir el archivo. */
    char *q = strchr(uri, '?');
    if (q) *q = '\0';

    /* El URI debe ser absoluto (empezar con '/'). */
    if (uri[0] != '/')
        return PARSE_BAD_REQUEST;

    /* keep-alive por defecto en 1.1; en 1.0 solo si lo piden. */
    req->keep_alive = (strcmp(version, "HTTP/1.1") == 0) ? 1 : 0;

    /* --- Analisis de encabezados basicos (Connection, Host, User-Agent) --- */
    const char *h = line_end + 2;
    while (h < end) {
        const char *nl = memmem(h, (size_t)(end - h), "\r\n", 2);

        if (strncasecmp(h, "Connection:", 11) == 0) {
            const char *v = h + 11;
            while (*v == ' ') v++;
            if (strncasecmp(v, "close", 5) == 0)
                req->keep_alive = 0;
            else if (strncasecmp(v, "keep-alive", 10) == 0)
                req->keep_alive = 1;
        }
        /* Host: y User-Agent: se reconocen; aqui no alteran la logica
         * pero podrian registrarse o usarse para hosts virtuales. */

        if (!nl) break;
        h = nl + 2;
    }

    /* Copias acotadas a la estructura de salida (no strcpy). */
    snprintf(req->method,  sizeof(req->method),  "%s", method);
    snprintf(req->uri,     sizeof(req->uri),     "%s", uri);
    snprintf(req->version, sizeof(req->version), "%s", version);

    return PARSE_OK;
}

/* ------------------------------------------------------------------ */
/* Atencion de la solicitud                                           */
/* ------------------------------------------------------------------ */

int http_handle(int fd, const http_request_t *req)
{
    int    file_fd = -1;
    size_t file_size = 0;
    char   resolved[1024];

    file_result_t r = file_open_safe(req->uri, &file_fd, &file_size,
                                      resolved, sizeof(resolved));

    switch (r) {
        case FILE_NOT_FOUND: return http_send_error(fd, 404, req->keep_alive);
        case FILE_FORBIDDEN: return http_send_error(fd, 403, req->keep_alive);
        case FILE_ERROR:     return http_send_error(fd, 500, req->keep_alive);
        case FILE_OK:        break;
    }

    /* Cabecera 200 OK con Content-Type y Content-Length correctos. */
    char header[1024];
    int hdr_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Server: MiniHTTPd/1.0\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        mime_lookup(resolved), file_size,
        req->keep_alive ? "keep-alive" : "close");

    if (send_all(fd, header, (size_t)hdr_len) < 0) {
        close(file_fd);
        return 0;
    }

    /* Cuerpo: leemos del archivo en bloques y reenviamos. */
    char chunk[READ_CHUNK];
    ssize_t n;
    while ((n = read(file_fd, chunk, sizeof(chunk))) > 0) {
        if (send_all(fd, chunk, (size_t)n) < 0) {
            close(file_fd);
            return 0;
        }
    }
    close(file_fd);

    if (n < 0) return 0; /* error de lectura a mitad de respuesta */
    return req->keep_alive;
}

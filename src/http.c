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

/* ================================================================== */
/* UTILIDADES DE ENVÍO                                                */
/* ================================================================== */

/**
 * Envía datos de forma confiable sobre un socket no bloqueante.
 * Retorna: 0 en éxito, -1 si la conexión se rompe.
 */
static int send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, data + sent, len - sent);
        if (n > 0) {
            sent += (size_t)n;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            continue; /* Socket lleno momentáneamente o interrupción: reintentar */
        } else {
            return -1; 
        }
    }
    return 0;
}

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

/* ================================================================== */
/* RESPUESTAS DE ERROR                                                */
/* ================================================================== */

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

    /* El estado 405 exige el encabezado 'Allow' según RFC 7231 */
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

/* ================================================================== */
/* PARSING DE LA SOLICITUD                                            */
/* ================================================================== */

parse_result_t http_parse(const char *buf, size_t len, http_request_t *req)
{
    /* Una solicitud HTTP válida (cabeceras) finaliza estrictamente en "\r\n\r\n" */
    const char *end = memmem(buf, len, "\r\n\r\n", 4);
    if (!end)
        return PARSE_INCOMPLETE; 

    /* --- Análisis de la Línea de Solicitud --- */
    const char *line_end = memmem(buf, len, "\r\n", 2);
    size_t line_len = (size_t)(line_end - buf);
    if (line_len == 0 || line_len >= 4096)
        return PARSE_BAD_REQUEST;

    char line[4096];
    memcpy(line, buf, line_len);
    line[line_len] = '\0';

    char method[16], uri[MAX_URI], version[16];
    if (sscanf(line, "%15s %2047s %15s", method, uri, version) != 3)
        return PARSE_BAD_REQUEST;

    if (strcmp(version, "HTTP/1.1") != 0 && strcmp(version, "HTTP/1.0") != 0)
        return PARSE_BAD_REQUEST;

    if (strcmp(method, "GET") != 0)
        return PARSE_NOT_IMPLEMENTED;

    /* Remoción del query string para localizar el recurso en disco */
    char *q = strchr(uri, '?');
    if (q) *q = '\0';

    if (uri[0] != '/')
        return PARSE_BAD_REQUEST;

    /* Gestion persistente por defecto: activa en HTTP/1.1, pasiva en HTTP/1.0 */
    req->keep_alive = (strcmp(version, "HTTP/1.1") == 0) ? 1 : 0;

    /* --- Análisis de Encabezados (Connection) --- */
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

        if (!nl) break;
        h = nl + 2;
    }

    snprintf(req->method,  sizeof(req->method),  "%s", method);
    snprintf(req->uri,     sizeof(req->uri),     "%s", uri);
    snprintf(req->version, sizeof(req->version), "%s", version);

    return PARSE_OK;
}
/* ================================================================== */
/* ATENCIÓN DE LA SOLICITUD                                           */
/* ================================================================== */

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

    /* Transmisión del cuerpo del archivo por bloques (Chunks) */
    char chunk[READ_CHUNK];
    ssize_t n;
    while ((n = read(file_fd, chunk, sizeof(chunk))) > 0) {
        if (send_all(fd, chunk, (size_t)n) < 0) {
            close(file_fd);
            return 0;
        }
    }
    close(file_fd);

    if (n < 0) return 0; 
    return req->keep_alive;
}
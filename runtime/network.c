/*
 * network.c - low-level POSIX/Win32 socket wrapper stubs for the pith
 * runtime. These are v0.1 placeholders: thin, blocking wrappers around
 * the platform socket API. A future os.network namespace will surface
 * them to compiled pith code.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/api.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

int32_t pith_net_socket(int32_t domain, int32_t type, int32_t protocol)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)domain; (void)type; (void)protocol;
    return -1;   /* stub: Winsock wiring lands with the os.network namespace */
#else
    return (int32_t)socket((int)domain, (int)type, (int)protocol);
#endif
}

int32_t pith_net_connect(int32_t fd, const char *host, int32_t port)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)fd; (void)host; (void)port;
    return -1;
#else
    if (!host || port <= 0 || port > 65535)
        return -1;

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char service[8];
    snprintf(service, sizeof(service), "%d", (int)port);

    if (getaddrinfo(host, service, &hints, &res) != 0 || !res)
        return -1;

    int rc = connect((int)fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return rc == 0 ? 0 : -1;
#endif
}

int32_t pith_net_send(int32_t fd, const char *buf, int32_t len)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)fd; (void)buf; (void)len;
    return -1;
#else
    if (!buf || len < 0)
        return -1;
    return (int32_t)send((int)fd, buf, (size_t)len, 0);
#endif
}

int32_t pith_net_recv(int32_t fd, char *buf, int32_t len)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)fd; (void)buf; (void)len;
    return -1;
#else
    if (!buf || len < 0)
        return -1;
    return (int32_t)recv((int)fd, buf, (size_t)len, 0);
#endif
}

int32_t pith_net_close(int32_t fd)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)fd;
    return -1;
#else
    return (int32_t)close((int)fd);
#endif
}

int32_t pith_rt_net_connect(int32_t fd, PithValue *host, int32_t port)
{
    if (!host || host->length == 0)
        return -1;
    char h[256];
    size_t len = host->length < sizeof(h) - 1 ? host->length : sizeof(h) - 1;
    memcpy(h, host->data, len);
    h[len] = '\0';
    return pith_net_connect(fd, h, port);
}

int32_t pith_rt_net_send(int32_t fd, PithValue *data)
{
    if (!data || data->length == 0)
        return 0;
    return pith_net_send(fd, data->data, (int32_t)data->length);
}

PithValue *pith_rt_net_recv(int32_t fd, int32_t max_len)
{
    if (max_len <= 0)
        return pith_str_new("", 0);
    if (max_len > 1024 * 1024)
        max_len = 1024 * 1024;
    char *buf = malloc((size_t)max_len);
    if (!buf)
        return pith_str_new("", 0);
    int32_t n = pith_net_recv(fd, buf, max_len);
    if (n <= 0) {
        free(buf);
        return pith_str_new("", 0);
    }
    PithValue *res = pith_str_new(buf, (size_t)n);
    free(buf);
    return res;
}

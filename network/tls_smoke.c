/*
 * Public-page HTTPS compatibility probe for the 32-bit Windows ME port.
 * Copyright (c) 2026 IEWebkit contributors. SPDX-License-Identifier: BSD-2-Clause
 *
 * This is a dependency/runtime gate, not the browser resource loader. It never
 * accepts cookies, credentials, or arbitrary HTTP request bytes from a page.
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *log_file;

static int fail(const char *stage)
{
    unsigned long error = ERR_peek_last_error();
    if (log_file) {
        fprintf(log_file, "FAIL %s winsock=%lu openssl=%lu\n", stage,
                (unsigned long)WSAGetLastError(), error);
        fflush(log_file);
    }
    fprintf(stderr, "FAIL %s winsock=%lu openssl=%lu\n", stage,
            (unsigned long)WSAGetLastError(), error);
    return 1;
}

static int valid_dns_name(const char *name)
{
    size_t i, n = strlen(name);
    if (n == 0 || n > 253 || name[0] == '.' || name[n - 1] == '.')
        return 0;
    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '.'))
            return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    const char *host = argc > 1 ? argv[1] : "www.zuzunza.com";
    const char *ca_file = argc > 2 ? argv[2] : "C:\\ZUKUQA\\cacert.pem";
    const char *report = argc > 3 ? argv[3] : "C:\\ZUKUQA\\TLS.LOG";
    WSADATA wsa;
    struct hostent *answer = NULL;
    struct sockaddr_in addr;
    SOCKET fd = INVALID_SOCKET;
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    char request[512], first[512];
    int rc = 1, n, status = 0;
    size_t used = 0;
    u_long nonblocking = 1;
    fd_set writable, exceptional;
    struct timeval timeout;
    int socket_error = 0, socket_error_len = sizeof(socket_error);
    int io_timeout_ms = 15000;

    log_file = fopen(report, "wb");
    if (!log_file)
        return fail("open_report");
    if (!valid_dns_name(host))
        goto invalid_host;
    if (WSAStartup(MAKEWORD(2, 0), &wsa)) {
        fail("winsock_startup");
        goto cleanup;
    }
    if (LOBYTE(wsa.wVersion) != 2) {
        fail("winsock_version");
        goto cleanup_wsa;
    }
    answer = gethostbyname(host); /* Winsock2 on ME has no getaddrinfo. */
    if (!answer || answer->h_addrtype != AF_INET || !answer->h_addr_list[0]) {
        fail("dns");
        goto cleanup_wsa;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(443);
    memcpy(&addr.sin_addr, answer->h_addr_list[0], sizeof(addr.sin_addr));
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET || ioctlsocket(fd, FIONBIO, &nonblocking)) {
        fail("socket");
        goto cleanup_socket;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR &&
        WSAGetLastError() != WSAEWOULDBLOCK) {
        fail("connect");
        goto cleanup_socket;
    }
    FD_ZERO(&writable);
    FD_ZERO(&exceptional);
    FD_SET(fd, &writable);
    FD_SET(fd, &exceptional);
    timeout.tv_sec = 15;
    timeout.tv_usec = 0;
    if (select(0, NULL, &writable, &exceptional, &timeout) != 1 ||
        getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&socket_error,
                   &socket_error_len) == SOCKET_ERROR || socket_error) {
        fail("connect_timeout_or_error");
        goto cleanup_socket;
    }
    nonblocking = 0;
    if (ioctlsocket(fd, FIONBIO, &nonblocking)) {
        fail("socket_blocking");
        goto cleanup_socket;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&io_timeout_ms,
                   sizeof(io_timeout_ms)) == SOCKET_ERROR ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&io_timeout_ms,
                   sizeof(io_timeout_ms)) == SOCKET_ERROR) {
        fail("socket_timeout");
        goto cleanup_socket;
    }

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx || !SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
        fail("tls_context");
        goto cleanup_ctx;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    if (SSL_CTX_load_verify_locations(ctx, ca_file, NULL) != 1) {
        fail("ca_bundle");
        goto cleanup_ctx;
    }
    ssl = SSL_new(ctx);
    if (!ssl || SSL_set_fd(ssl, (int)fd) != 1 ||
        SSL_set_tlsext_host_name(ssl, host) != 1 ||
        SSL_set1_host(ssl, host) != 1)
        goto cleanup_ssl;
    if (SSL_connect(ssl) != 1 || SSL_get_verify_result(ssl) != X509_V_OK)
        goto cleanup_ssl;

    n = snprintf(request, sizeof(request),
                 "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: IEWebkit-TLS-Probe/1\r\nAccept: text/html\r\nConnection: close\r\n\r\n",
                 host);
    if (n < 0 || n >= (int)sizeof(request) || SSL_write(ssl, request, n) != n)
        goto cleanup_ssl;
    /* Only collect the status line; page bytes remain out of this probe. */
    while (used + 1 < sizeof(first)) {
        n = SSL_read(ssl, first + used, 1);
        if (n != 1)
            goto cleanup_ssl;
        if (first[used++] == '\n')
            break;
    }
    first[used] = 0;
    if (used < 10 || first[used - 1] != '\n' ||
        (strncmp(first, "HTTP/1.1 ", 9) && strncmp(first, "HTTP/1.0 ", 9)) ||
        sscanf(first + 9, "%d", &status) != 1 || status < 100 || status > 599)
        goto cleanup_ssl;
    fprintf(log_file, "PASS tls=%s verify=%ld http=%d host=%s\n",
            SSL_get_version(ssl), SSL_get_verify_result(ssl), status, host);
    fflush(log_file);
    printf("PASS TLS %s HTTP %d\n", SSL_get_version(ssl), status);
    rc = 0;
    goto cleanup_ssl_done;

invalid_host:
    fail("invalid_host");
    goto cleanup;
cleanup_ssl:
    fail("tls_or_http");
cleanup_ssl_done:
    if (ssl)
        SSL_free(ssl);
cleanup_ctx:
    if (ctx)
        SSL_CTX_free(ctx);
cleanup_socket:
    if (fd != INVALID_SOCKET)
        closesocket(fd);
cleanup_wsa:
    WSACleanup();
cleanup:
    fclose(log_file);
    return rc;
}

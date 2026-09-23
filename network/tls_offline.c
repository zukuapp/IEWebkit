/*
 * Offline OpenSSL handshake gate for a Windows ME guest without a bound NIC.
 * Copyright (c) 2026 IEWebkit contributors. SPDX-License-Identifier: BSD-2-Clause
 */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>
#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

static FILE *report;
enum expectation { ACCEPT, HOST_MISMATCH, UNTRUSTED_CA };

static unsigned long platform_error(void)
{
#ifdef _WIN32
    return GetLastError();
#else
    return 0;
#endif
}

static int handshake(const char *ca_path, const char *cert_path,
                     const char *key_path, const char *verify_host,
                     enum expectation expected)
{
    SSL_CTX *client_ctx = NULL, *server_ctx = NULL;
    SSL *client = NULL, *server = NULL;
    BIO *client_bio = NULL, *server_bio = NULL;
    int client_done = 0, server_done = 0, step, result = 0;
    long verify_result = -1;
    const char *stage = "client_context";

    client_ctx = SSL_CTX_new(TLS_client_method());
    if (!client_ctx)
        goto cleanup;
    stage = "server_context";
    server_ctx = SSL_CTX_new(TLS_server_method());
    if (!server_ctx)
        goto cleanup;
    stage = "min_protocol";
    if (!SSL_CTX_set_min_proto_version(client_ctx, TLS1_2_VERSION) ||
        !SSL_CTX_set_min_proto_version(server_ctx, TLS1_2_VERSION))
        goto cleanup;
    SSL_CTX_set_verify(client_ctx, SSL_VERIFY_PEER, NULL);
    stage = "ca_file";
    if (SSL_CTX_load_verify_locations(client_ctx, ca_path, NULL) != 1)
        goto cleanup;
    stage = "server_certificate";
    if (SSL_CTX_use_certificate_file(server_ctx, cert_path, SSL_FILETYPE_PEM) != 1)
        goto cleanup;
    stage = "server_key";
    if (SSL_CTX_use_PrivateKey_file(server_ctx, key_path, SSL_FILETYPE_PEM) != 1)
        goto cleanup;
    stage = "server_key_match";
    if (SSL_CTX_check_private_key(server_ctx) != 1)
        goto cleanup;
    stage = "client_ssl";
    client = SSL_new(client_ctx);
    if (!client)
        goto cleanup;
    stage = "server_ssl";
    server = SSL_new(server_ctx);
    if (!server)
        goto cleanup;
    stage = "sni";
    if (SSL_set_tlsext_host_name(client, "iewebkit.invalid") != 1)
        goto cleanup;
    stage = "hostname";
    if (SSL_set1_host(client, verify_host) != 1)
        goto cleanup;
    stage = "bio_pair";
    if (BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) != 1)
        goto cleanup;
    SSL_set_bio(client, client_bio, client_bio);
    client_bio = NULL;
    SSL_set_bio(server, server_bio, server_bio);
    server_bio = NULL;
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);

    for (step = 0; step < 64; ++step) {
        int value, error;
        if (!client_done) {
            value = SSL_do_handshake(client);
            if (value == 1)
                client_done = 1;
            else {
                error = SSL_get_error(client, value);
                if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE)
                    break;
            }
        }
        if (!server_done) {
            value = SSL_do_handshake(server);
            if (value == 1)
                server_done = 1;
            else {
                error = SSL_get_error(server, value);
                if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE)
                    break;
            }
        }
        if (client_done && server_done)
            break;
    }
    verify_result = SSL_get_verify_result(client);
    if (expected == HOST_MISMATCH)
        result = !client_done && verify_result == X509_V_ERR_HOSTNAME_MISMATCH;
    else if (expected == UNTRUSTED_CA)
        result = !client_done &&
                 (verify_result == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY ||
                  verify_result == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE);
    else
        result = client_done && server_done && verify_result == X509_V_OK &&
                 SSL_version(client) >= TLS1_2_VERSION;
    fprintf(report, "%s host=%s steps=%d client=%d server=%d verify=%ld tls=%s openssl=%lu\n",
            result ? "PASS" : "FAIL", verify_host, step + 1,
            client_done, server_done, verify_result,
            SSL_get_version(client), ERR_peek_last_error());
    fflush(report);

cleanup:
    if (!result && verify_result == -1)
        fprintf(report, "FAIL setup stage=%s host=%s winerr=%lu errno=%d openssl=%lu\n",
                stage, verify_host, platform_error(), errno, ERR_peek_last_error());
    if (client_bio)
        BIO_free(client_bio);
    if (server_bio)
        BIO_free(server_bio);
    if (client)
        SSL_free(client);
    if (server)
        SSL_free(server);
    if (client_ctx)
        SSL_CTX_free(client_ctx);
    if (server_ctx)
        SSL_CTX_free(server_ctx);
    return result;
}

int main(int argc, char **argv)
{
    const char *ca = argc > 1 ? argv[1] : "D:\\CA.PEM";
    const char *cert = argc > 2 ? argv[2] : "D:\\SERVER.PEM";
    const char *key = argc > 3 ? argv[3] : "D:\\SERVER.KEY";
    const char *other_ca = argc > 4 ? argv[4] : "D:\\OTHERCA.PEM";
    const char *log_path = argc > 5 ? argv[5] : "C:\\ZUKUQA\\TLSOFF.LOG";
    int valid, invalid, untrusted;

    report = fopen(log_path, "wb");
    if (!report)
        return 2;
    valid = handshake(ca, cert, key, "iewebkit.invalid", ACCEPT);
    invalid = handshake(ca, cert, key, "wrong.iewebkit.invalid", HOST_MISMATCH);
    untrusted = handshake(other_ca, cert, key, "iewebkit.invalid", UNTRUSTED_CA);
    fprintf(report, "RESULT valid=%d invalid_rejected=%d untrusted_rejected=%d\n",
            valid, invalid, untrusted);
    fclose(report);
    return valid && invalid && untrusted ? 0 : 1;
}

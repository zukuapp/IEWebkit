/*
 * Offline OpenSSL handshake gate for a Windows ME guest without a bound NIC.
 * Copyright (c) 2026 IEWebkit contributors. SPDX-License-Identifier: BSD-2-Clause
 */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>
#include <stdio.h>

static FILE *report;

static int handshake(const char *ca_path, const char *cert_path,
                     const char *key_path, const char *verify_host,
                     int expect_host_mismatch)
{
    SSL_CTX *client_ctx = NULL, *server_ctx = NULL;
    SSL *client = NULL, *server = NULL;
    BIO *client_bio = NULL, *server_bio = NULL;
    int client_done = 0, server_done = 0, step, result = 0;
    long verify_result = -1;

    client_ctx = SSL_CTX_new(TLS_client_method());
    server_ctx = SSL_CTX_new(TLS_server_method());
    if (!client_ctx || !server_ctx ||
        !SSL_CTX_set_min_proto_version(client_ctx, TLS1_2_VERSION) ||
        !SSL_CTX_set_min_proto_version(server_ctx, TLS1_2_VERSION))
        goto cleanup;
    SSL_CTX_set_verify(client_ctx, SSL_VERIFY_PEER, NULL);
    if (SSL_CTX_load_verify_locations(client_ctx, ca_path, NULL) != 1 ||
        SSL_CTX_use_certificate_file(server_ctx, cert_path, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_use_PrivateKey_file(server_ctx, key_path, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(server_ctx) != 1)
        goto cleanup;
    client = SSL_new(client_ctx);
    server = SSL_new(server_ctx);
    if (!client || !server ||
        SSL_set_tlsext_host_name(client, "iewebkit.invalid") != 1 ||
        SSL_set1_host(client, verify_host) != 1 ||
        BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) != 1)
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
    if (expect_host_mismatch)
        result = !client_done && verify_result == X509_V_ERR_HOSTNAME_MISMATCH;
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
        fprintf(report, "FAIL setup host=%s openssl=%lu\n",
                verify_host, ERR_peek_last_error());
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
    const char *log_path = argc > 4 ? argv[4] : "C:\\ZUKUQA\\TLSOFF.LOG";
    int valid, invalid;

    report = fopen(log_path, "wb");
    if (!report)
        return 2;
    valid = handshake(ca, cert, key, "iewebkit.invalid", 0);
    invalid = handshake(ca, cert, key, "wrong.iewebkit.invalid", 1);
    fprintf(report, "RESULT valid=%d invalid_rejected=%d\n", valid, invalid);
    fclose(report);
    return valid && invalid ? 0 : 1;
}

/* Bounded phase trace for locating ME runtime failures inside OpenSSL init. */
/* Copyright (c) 2026 IEWebkit contributors. SPDX-License-Identifier: BSD-2-Clause */
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>

static FILE *report;
static CRYPTO_ONCE once = CRYPTO_ONCE_STATIC_INIT;
static int once_called;
static void init_once(void) { once_called = 1; }

static void phase(const char *name)
{
    /* ERR_peek_last_error itself initializes OpenSSL; keep the trace independent. */
    fprintf(report, "PHASE %s\n", name);
    fflush(report);
}

int main(void)
{
    CRYPTO_RWLOCK *lock;
    SSL_CTX *ctx;
    report = fopen("C:\\ZUKUQA\\TLSINIT.LOG", "wb");
    if (!report)
        return 2;
    phase("before_run_once");
    if (!CRYPTO_THREAD_run_once(&once, init_once) || !once_called)
        return 10;
    phase("after_run_once_before_lock");
    lock = CRYPTO_THREAD_lock_new();
    if (!lock)
        return 11;
    phase("after_lock_before_crypto_init");
    CRYPTO_THREAD_lock_free(lock);
    if (!OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, NULL))
        return 12;
    phase("after_crypto_init_before_add_all");
    if (!OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG |
                             OPENSSL_INIT_ADD_ALL_CIPHERS |
                             OPENSSL_INIT_ADD_ALL_DIGESTS, NULL))
        return 15;
    phase("after_add_all_before_ssl_init");
    if (!OPENSSL_init_ssl(OPENSSL_INIT_NO_LOAD_CONFIG, NULL))
        return 13;
    phase("after_ssl_init_before_ctx");
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx)
        return 14;
    phase("after_ctx");
    SSL_CTX_free(ctx);
    fclose(report);
    return 0;
}

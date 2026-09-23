/* Narrow Windows ME OpenSSL library-context constructor probe. */
/* Copyright (c) 2026 IEWebkit contributors. SPDX-License-Identifier: BSD-2-Clause */
#include <openssl/crypto.h>
#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
# include <windows.h>
#endif

#ifndef IEWK_LIBCTX_LOG_PATH
# define IEWK_LIBCTX_LOG_PATH "C:\\ZUKUQA\\TLSCTX.LOG"
#endif

int main(void)
{
    FILE *report = fopen(IEWK_LIBCTX_LOG_PATH, "wb");
    OSSL_LIB_CTX *ctx;

    if (report == NULL)
        return 2;
    fputs("before_libctx_new\n", report);
    fflush(report);
    ctx = OSSL_LIB_CTX_new();
    if (ctx == NULL) {
#ifdef _WIN32
        fprintf(report, "libctx_new_failed winerr=%lu errno=%d\n",
                (unsigned long)GetLastError(), errno);
#else
        fprintf(report, "libctx_new_failed errno=%d\n", errno);
#endif
        fclose(report);
        return 1;
    }
    fputs("after_libctx_new\n", report);
    fflush(report);
    OSSL_LIB_CTX_free(ctx);
    fputs("after_libctx_free\n", report);
    fclose(report);
    return 0;
}

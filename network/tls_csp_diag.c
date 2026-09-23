/*
 * Windows ME CryptoAPI provider diagnostic for the offline TLS gate.
 * Copyright (c) 2026 IEWebkit contributors. SPDX-License-Identifier: BSD-2-Clause
 *
 * This program never creates a key container and never prints random bytes.
 */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static FILE *report;

static void probe(const char *label, DWORD type, const char *name, DWORD flags)
{
    HCRYPTPROV provider = 0;
    BYTE sample[32];
    DWORD acquire_error = 0, random_error = 0;
    BOOL acquired, random_ok = FALSE;

    SetLastError(0);
    acquired = CryptAcquireContextA(&provider, NULL, name, type, flags);
    if (!acquired)
        acquire_error = GetLastError();
    else {
        SetLastError(0);
        random_ok = CryptGenRandom(provider, sizeof(sample), sample);
        if (!random_ok)
            random_error = GetLastError();
        SecureZeroMemory(sample, sizeof(sample));
        CryptReleaseContext(provider, 0);
    }
    fprintf(report,
            "probe=%s type=%lu flags=%lu acquire=%d acquire_error=%lu "
            "random=%d random_error=%lu\n",
            label, (unsigned long)type, (unsigned long)flags,
            acquired != 0, (unsigned long)acquire_error,
            random_ok != 0, (unsigned long)random_error);
    fflush(report);
}

static void enumerate_providers(void)
{
    DWORD index;
    for (index = 0; index < 32; ++index) {
        char name[256];
        DWORD size = sizeof(name), type = 0, error;
        SetLastError(0);
        if (!CryptEnumProvidersA(index, NULL, 0, &type, name, &size)) {
            error = GetLastError();
            fprintf(report, "enum_end index=%lu error=%lu\n",
                    (unsigned long)index, (unsigned long)error);
            break;
        }
        name[sizeof(name) - 1] = '\0';
        fprintf(report, "provider index=%lu type=%lu name=%s\n",
                (unsigned long)index, (unsigned long)type, name);
        probe(name, type, name, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    }
}

static void default_provider(DWORD flags, const char *label)
{
    char name[256];
    DWORD size = sizeof(name), error = 0;
    BOOL ok;
    SetLastError(0);
    ok = CryptGetDefaultProviderA(PROV_RSA_FULL, NULL, flags, name, &size);
    if (!ok)
        error = GetLastError();
    else
        name[sizeof(name) - 1] = '\0';
    fprintf(report, "default=%s ok=%d error=%lu name=%s\n",
            label, ok != 0, (unsigned long)error, ok ? name : "-");
}

int main(void)
{
    report = fopen("C:\\ZUKUQA\\CSP18.LOG", "wb");
    if (report == NULL)
        return 2;
    fprintf(report, "CSP18 Windows ME diagnostic; no key containers created\n");
    probe("default_silent", PROV_RSA_FULL, NULL,
          CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    probe("default", PROV_RSA_FULL, NULL, CRYPT_VERIFYCONTEXT);
    default_provider(CRYPT_USER_DEFAULT, "user");
    default_provider(CRYPT_MACHINE_DEFAULT, "machine");
    enumerate_providers();
    fclose(report);
    return 0;
}

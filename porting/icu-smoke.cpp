#include <unicode/utypes.h>
#include <unicode/uclean.h>
#include <unicode/uversion.h>
#include <unicode/ustring.h>
#include <unicode/unorm2.h>
#include <unicode/ucol.h>
#include <unicode/ucal.h>
#include <cstdio>
int main() {
    UErrorCode error = U_ZERO_ERROR;
    u_init(&error);
    if (U_FAILURE(error)) return 10;
    UChar composed[8] = {};
    const UChar decomposed[] = {0x1112,0x1161,0x11AB,0x1100,0x1173,0x11AF};
    auto normalizer = unorm2_getNFCInstance(&error);
    int length = unorm2_normalize(normalizer, decomposed, 6, composed, 8, &error);
    if (U_FAILURE(error) || length != 2 || composed[0] != 0xD55C || composed[1] != 0xAE00) return 11;
    UCollator *collator = ucol_open("ko_KR", &error);
    if (U_FAILURE(error) || !collator) return 12;
    UChar timezone[128] = {};
    if (ucal_getDefaultTimeZone(timezone, 128, &error) <= 0 || U_FAILURE(error)) return 13;
    UVersionInfo version; char formatted[U_MAX_VERSION_STRING_LENGTH];
    u_getVersion(version); u_versionToString(version, formatted);
    std::printf("ICU %s: Korean NFC/collator/timezone smoke passed\n", formatted);
    ucol_close(collator); u_cleanup();
    return 0;
}

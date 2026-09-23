#include <unicode/utypes.h>
#include <unicode/uclean.h>
#include <unicode/uversion.h>
#include <unicode/ustring.h>
#include <unicode/unorm2.h>
#include <unicode/ucol.h>
#include <unicode/ucal.h>
#include <unicode/uloc.h>
#include <unicode/ucnv.h>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include "probe-telemetry.h"
#define PHASE(value) probe_log("phase", value)
#else
#define PHASE(value) ((void)0)
#endif
static int run_probe() {
    PHASE(1);
    UErrorCode error = U_ZERO_ERROR;
    u_init(&error);
    if (U_FAILURE(error)) return 10;
    PHASE(2);
    UChar composed[8] = {};
    const UChar decomposed[] = {0x1112,0x1161,0x11AB,0x1100,0x1173,0x11AF};
    auto normalizer = unorm2_getNFCInstance(&error);
    int length = unorm2_normalize(normalizer, decomposed, 6, composed, 8, &error);
    if (U_FAILURE(error) || length != 2 || composed[0] != 0xD55C || composed[1] != 0xAE00) return 11;
    UConverter* korean = ucnv_open("windows-949", &error);
    UChar decoded[8] = {};
    const char encoded[] = {char(0xC7), char(0xD1), char(0xB1), char(0xDB)};
    if (U_FAILURE(error) || !korean) return 19;
    length = ucnv_toUChars(korean, decoded, 8, encoded, sizeof(encoded), &error);
    ucnv_close(korean);
    if (U_FAILURE(error) || length != 2 || decoded[0] != 0xD55C || decoded[1] != 0xAE00) return 20;
    PHASE(3);
    UCollator *collator = ucol_open("ko_KR", &error);
    if (U_FAILURE(error) || !collator) return 12;
    const UChar ga[] = {0xAC00}, na[] = {0xB098};
    if (ucol_strcoll(collator, ga, 1, na, 1) != UCOL_LESS) return 14;
    ucol_setAttribute(collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &error);
    if (U_FAILURE(error) || ucol_strcoll(collator, composed, 2, decomposed, 6) != UCOL_EQUAL) return 15;
    char mapped[ULOC_FULLNAME_CAPACITY] = {};
    uloc_getLocaleForLCID(0x0412, mapped, sizeof(mapped), &error);
    if (U_FAILURE(error) || std::strcmp(mapped, "ko_KR") || uloc_getLCID("ko_KR") != 0x0412) return 16;
    std::printf("default locale=%s; Korean LCID roundtrip and collation passed\n", uloc_getDefault());
    PHASE(4);
    UChar timezone[128] = {};
    if (ucal_getDefaultTimeZone(timezone, 128, &error) <= 0 || U_FAILURE(error)) return 13;
    char timezoneUTF8[256] = {};
    u_strToUTF8(timezoneUTF8, sizeof(timezoneUTF8), nullptr, timezone, -1, &error);
    if (U_FAILURE(error)) return 17;
    std::printf("default timezone=%s\n", timezoneUTF8);
    const UChar newYork[] = u"America/New_York";
    UCalendar* calendar = ucal_open(newYork, -1, "en_US", UCAL_GREGORIAN, &error);
    if (U_FAILURE(error) || !calendar) return 21;
    ucal_setDateTime(calendar, 2026, UCAL_JANUARY, 1, 12, 0, 0, &error);
    if (ucal_get(calendar, UCAL_ZONE_OFFSET, &error) != -18000000
        || ucal_get(calendar, UCAL_DST_OFFSET, &error) != 0 || U_FAILURE(error)) return 22;
    ucal_setDateTime(calendar, 2026, UCAL_JULY, 1, 12, 0, 0, &error);
    if (ucal_get(calendar, UCAL_ZONE_OFFSET, &error) != -18000000
        || ucal_get(calendar, UCAL_DST_OFFSET, &error) != 3600000 || U_FAILURE(error)) return 23;
    ucal_close(calendar);
#ifdef IEWK_EXPECT_KOREAN_GUEST
    if (std::strcmp(uloc_getDefault(), "ko_KR") || std::strcmp(timezoneUTF8, "Asia/Seoul")) return 18;
#endif
    PHASE(5);
    UVersionInfo version; char formatted[U_MAX_VERSION_STRING_LENGTH];
    u_getVersion(version); u_versionToString(version, formatted);
    std::printf("ICU %s: Korean NFC/collator/timezone smoke passed\n", formatted);
    ucol_close(collator); u_cleanup();
    return 0;
}
int main() {
#ifdef _WIN32
    probe_open("C:\\ICUDIAG.LOG");
    return probe_finish(run_probe());
#else
    return run_probe();
#endif
}

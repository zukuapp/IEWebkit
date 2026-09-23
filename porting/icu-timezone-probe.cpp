/* The real patched ICU timezone detector, with only Win32 registry calls mocked. */
#include "unicode/utypes.h"
#include "unicode/ures.h"
#include "unicode/unistr.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "uresimp.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using LONG = int32_t;
using DWORD = uint32_t;
using LPBYTE = unsigned char*;
using HKEY = const char*;
struct SYSTEMTIME { uint16_t wYear{}, wMonth{}, wDayOfWeek{}, wDay{}, wHour{}, wMinute{}, wSecond{}, wMilliseconds{}; };
struct TIME_ZONE_INFORMATION { LONG Bias{}; SYSTEMTIME StandardDate{}; LONG StandardBias{}; SYSTEMTIME DaylightDate{}; LONG DaylightBias{}; };
struct REG_TZI_FORMAT { LONG Bias{}, StandardBias{}, DaylightBias{}; SYSTEMTIME StandardDate{}, DaylightDate{}; };
constexpr DWORD TIME_ZONE_ID_INVALID = 0xffffffffu, REG_SZ = 1, REG_BINARY = 3;
constexpr DWORD KEY_QUERY_VALUE = 1, LOCALE_SISO3166CTRYNAME = 0x5a;
constexpr LONG ERROR_SUCCESS = 0;
static HKEY HKEY_LOCAL_MACHINE = "root";
static TIME_ZONE_INFORMATION current;
static REG_TZI_FORMAT rules;
static const char* zone = "Korea";
static const char* name = "\xc7\xd1-korean-name";
static bool failAPI, failRegistry, malformedName, noRegion;
static unsigned handles;
static DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION* output) {
    *output = current;
    return failAPI ? TIME_ZONE_ID_INVALID : 0;
}
static LONG RegOpenKeyExA(HKEY, const char* path, DWORD, DWORD, HKEY* output) {
    if (failRegistry) return 2;
    if (!std::strcmp(path, "SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation")) {
        *output = "current";
    } else {
        const char* final = std::strrchr(path, '\\');
        if (!final || (std::strcmp(final + 1, zone) && std::strcmp(final + 1, "Tokyo"))) return 2;
        *output = !std::strcmp(final + 1, zone) ? "selected" : "collision";
    }
    ++handles;
    return ERROR_SUCCESS;
}
static LONG RegQueryValueExA(HKEY key, const char* value, void*, DWORD* type, LPBYTE output, DWORD* bytes) {
    if (!std::strcmp(value, "TZI")) {
        if (*bytes < sizeof(rules)) return 234;
        std::memcpy(output, &rules, sizeof(rules));
        *bytes = sizeof(rules); *type = REG_BINARY;
        return ERROR_SUCCESS;
    }
    const char* text = !std::strcmp(key, "collision") ? "different-zone-same-offset" : name;
    size_t needed = std::strlen(text) + 1;
    if (*bytes < needed) return 234;
    std::memcpy(output, text, needed);
    *bytes = needed; *type = REG_SZ;
    if (malformedName) output[needed - 1] = 'X';
    return ERROR_SUCCESS;
}
static LONG RegCloseKey(HKEY) { --handles; return ERROR_SUCCESS; }
static DWORD GetUserDefaultLCID() { return 0x0412; }
static int GetLocaleInfoA(DWORD, DWORD, char* output, int size) {
    if (noRegion || size < 3) return 0;
    std::memcpy(output, "KR", 3);
    return 3;
}
#define U_IEWEBKIT_WIN9X 1
U_NAMESPACE_BEGIN
#include "icu-timezone-under-test.inc"
U_NAMESPACE_END

static unsigned failures;
static void expect(const char* label, const char* wanted) {
    const char* found = icu::uprv_detectWindowsTimeZone();
    if ((wanted && (!found || std::strcmp(wanted, found))) || (!wanted && found) || handles) {
        std::printf("FAIL %s: got %s expected %s handles=%u\n", label,
            found ? found : "NULL", wanted ? wanted : "NULL", handles);
        ++failures;
    }
    uprv_free(const_cast<char*>(found));
}
int main() {
    current.Bias = rules.Bias = -540;
    expect("Korean ANSI name disambiguates Tokyo", "Asia/Seoul");
    noRegion = true;
    expect("no region uses CLDR world mapping", "Asia/Seoul");
    noRegion = false;
    malformedName = true;
    expect("unterminated registry string", nullptr);
    malformedName = false;
    failAPI = true;
    expect("OS query failure", nullptr);
    failAPI = false; failRegistry = true;
    expect("registry unavailable", nullptr);
    failRegistry = false;
    zone = "Pacific"; name = "Pacific encoded standard name";
    current.Bias = rules.Bias = 480;
    rules.StandardDate.wMonth = current.StandardDate.wMonth = 11;
    rules.DaylightDate.wMonth = current.DaylightDate.wMonth = 3;
    expect("configured seasonal rules retain zone", "America/Los_Angeles");
    current.StandardDate.wMonth = current.DaylightDate.wMonth = 0;
    expect("disabled seasonal rules become fixed zone", "GMT-08:00");
    current.Bias = rules.Bias = -330;
    expect("fixed zone preserves fractional minutes", "GMT+05:30");
    current.Bias = rules.Bias = INT32_MIN;
    expect("invalid offset cannot overflow sign conversion", nullptr);
    if (failures) return 1;
    std::puts("ICU Win9x timezone: 9 real-detector registry/API boundary cases passed");
    return 0;
}

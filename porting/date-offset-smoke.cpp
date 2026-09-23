/* Include the actual pinned DateMath function; only host APIs are substituted. */
#include <cstdint>
#include <cstdio>
#ifdef IEWK_REAL_WIN32
#include <windows.h>
#include "probe-telemetry.h"
#else
using DWORD = uint32_t;
using LONG = int32_t;
using BOOL = int;
struct SYSTEMTIME { uint16_t wYear {}, wMonth {}; };
struct TIME_ZONE_INFORMATION {
    LONG Bias {};
    SYSTEMTIME StandardDate {};
    LONG StandardBias {};
    SYSTEMTIME DaylightDate {};
    LONG DaylightBias {};
};
constexpr DWORD TIME_ZONE_ID_UNKNOWN = 0, TIME_ZONE_ID_STANDARD = 1;
constexpr DWORD TIME_ZONE_ID_DAYLIGHT = 2, TIME_ZONE_ID_INVALID = 0xffffffffu;
static TIME_ZONE_INFORMATION supplied;
static DWORD zoneState;
static BOOL yearSuccess;
static unsigned legacyCalls, yearCalls, observedYear;
static DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION* output) {
    ++legacyCalls;
    *output = supplied;
    return zoneState;
}
static BOOL GetTimeZoneInformationForYear(uint16_t year, void*, TIME_ZONE_INFORMATION* output) {
    ++yearCalls;
    observedYear = year;
    *output = supplied;
    return yearSuccess;
}
// Exercise the UTC/local year boundary without changing any machine clock.
static void GetSystemTime(SYSTEMTIME* output) { output->wYear = 2025; }
static void GetLocalTime(SYSTEMTIME* output) { output->wYear = 2026; }
#endif

#define OS(feature) 1
#include "date-offset-under-test.inc"

#ifndef IEWK_REAL_WIN32
static unsigned failures;
static void expect(const char* name, int32_t wanted) {
    legacyCalls = yearCalls = observedYear = 0;
    int32_t got = calculateUTCOffset();
    if (got != wanted) {
        std::printf("FAIL %s: got %ld wanted %ld\n", name, (long)got, (long)wanted);
        ++failures;
    }
#if defined(_WIN32_WINDOWS) && _WIN32_WINDOWS <= 0x0490
    if (legacyCalls != 1 || yearCalls != 0) ++failures;
#else
    if (legacyCalls != 0 || yearCalls != 1 || observedYear != 2026) ++failures;
#endif
}
#endif
int main() {
#ifdef IEWK_REAL_WIN32
    probe_open("C:\\DATEDIAG.LOG");
    TIME_ZONE_INFORMATION info {};
    DWORD status = GetTimeZoneInformation(&info);
    probe_log("timezone.status", status);
    probe_log("timezone.error", GetLastError());
    if (status == TIME_ZONE_ID_INVALID) return probe_finish(1);
    probe_log("bias.minutes", (DWORD)info.Bias);
    probe_log("standard.bias.minutes", (DWORD)info.StandardBias);
    int32_t actual = calculateUTCOffset();
    probe_log("standard.offset.ms", (DWORD)actual);
    std::printf("standard offset=%ldms\n", (long)actual);
    // Runtime execution evidence, not an independent timezone oracle.
    return probe_finish(0);
#else
    supplied.Bias = -540;
    supplied.StandardBias = 75; // ignored when no transition dates exist
    supplied.DaylightBias = -60;
    zoneState = TIME_ZONE_ID_UNKNOWN; yearSuccess = 1;
    expect("Seoul no DST ignores transition biases", 32400000);
    supplied.StandardDate.wMonth = 11; supplied.DaylightDate.wMonth = 3;
    supplied.Bias = 480; supplied.StandardBias = 0;
    zoneState = TIME_ZONE_ID_STANDARD;
    expect("US standard base", -28800000);
    zoneState = TIME_ZONE_ID_DAYLIGHT;
    expect("US daylight still returns standard base", -28800000);
    supplied.Bias = -600; supplied.StandardBias = 30; supplied.DaylightBias = -30;
    expect("nonzero standard adjustment", 34200000);
    supplied.StandardDate.wMonth = 0;
    expect("incomplete transitions ignore standard adjustment", 36000000);
    supplied = {}; supplied.Bias = -345; zoneState = TIME_ZONE_ID_UNKNOWN;
    expect("quarter-hour zone", 20700000);
    supplied.Bias = 210;
    expect("negative half-hour zone", -12600000);
    // A failing API may leave output unspecified; never consume its payload.
    supplied.Bias = 480; zoneState = TIME_ZONE_ID_INVALID; yearSuccess = 0;
    expect("API failure returns existing fallback", 0);
    if (failures) return 1;
    std::puts("date offset: standard/DST states, transition biases, fractional zones, failure and local-year API selection passed");
    return 0;
#endif
}

# Windows standard timezone offset port

The pinned WebKit 2.54.0 `DateMath.cpp` failed to compile for ME because it
unconditionally called `GetTimeZoneInformationForYear`. That function requires
Vista SP1 or later. It returns a Boolean success value and requires a **local**
year. The original code instead treated its result as `TIME_ZONE_ID_*` and
supplied the UTC year. See Microsoft's
[API contract](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-gettimezoneinformationforyear).

## Semantics retained

`calculateUTCOffset()` supplies the standard offset; its caller separately adds
`calculateDSTOffset()`. Applying today's daylight bias here would double-count
the seasonal adjustment. Windows expresses bias as `UTC = local + bias`, so
WebKit's offset is the negative bias converted from minutes to milliseconds.
`StandardBias` applies only when transition dates exist; `DaylightBias` belongs
to the separate seasonal calculation. These fields follow Microsoft's
[TIME_ZONE_INFORMATION contract](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/ns-timezoneapi-time_zone_information).

The hash-pinned patch uses `GetTimeZoneInformation` only for explicit Win9x
declarations, rejecting `TIME_ZONE_ID_INVALID`. Other Windows builds retain
the per-year API, use `GetLocalTime` for its year and check Boolean failure.
Both retain the existing zero-offset fallback when the API itself fails.
The Win9x call exposes current configured rules, not historical timezone
changes; Microsoft's [current-settings API](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-gettimezoneinformation)
documents this limitation. This patch does not implement a historical timezone
database or complete ME daylight-saving conversion.

## Reproduce the focused checks

```
python3 porting/apply-webkit-patches.py --source /external/webkitgtk-2.54.0
python3 porting/build-date-offset-probes.py \
    --source /external/webkitgtk-2.54.0 --output /external/date-offset-probes
ninja -C /external/build-me-jsc -j1 Source/WTF/wtf/CMakeFiles/WTF.dir/DateMath.cpp.obj
```

The probe builder requires the exact patched source hash and extracts the real
`calculateUTCOffset()` body. Host mocks replace only the Windows API boundary.
Eight cases run for each platform branch: Seoul without DST, US standard and
daylight states, nonzero standard adjustment, incomplete transition dates,
quarter-hour and negative half-hour zones, and API failure. Every case also
checks API selection; modern cases exercise different UTC/local calendar years.

On 2026-09-23, the original body failed no-DST, incomplete-transition and
API-failure assertions. Both revised host probes passed. The actual full
`DateMath.cpp` compiled to its x86 object with one build job. No broad JSC build
was started for this checkpoint.

The native `date-offset-me.exe` links and passes the pinned ME import audit;
SHA-256 is `feb3cc02d3194e60a829252cf8ffd0cfcffe3eef82039cf9e9614aa171da6eb5`.
It writes `C:\DATEDIAG.LOG` and queries current settings without changing the
clock or timezone. **It has not executed in the ME guest.** Host mocked tests
and an import audit do not establish actual OS behavior.

## Remaining gates

The compiled DateMath object still calls `FileTimeToSystemTime` and
`SystemTimeToTzSpecificLocalTime` for its separate DST calculation. The pinned
ME kernel exports both, but the allocator work already proved that an export
can be an unimplemented stub. Their actual ME behavior and transition edges
remain unverified. ICU retains four missing ME imports documented in
[ICU.md](ICU.md). WTF threading, stack bounds and file handling still need their
Win9x ports. The full build has not resumed beyond this single object, so no
new downstream compiler failure or JSC executable is claimed.

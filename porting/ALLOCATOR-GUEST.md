# Allocator header probes on Windows ME

On 2026-09-23, the revised probes executed in Windows ME 4.90 build 3000,
Korean ACP/OEMCP 949, with IE 5.50.4134.0100 installed. QEMU used a Pentium III
CPU, one vCPU, 256 MiB RAM, standard VGA at 640×480, and no NIC. These are
allocator-header tests; they do not establish JSC, WebCore, browser hosting,
graphics acceleration or full mimalloc allocation correctness.

## Failures that changed the implementation

The first pair probe failed `CreateThread` with a null thread-ID output. Every
Windows probe now supplies that output. The corrected pair passed layout,
roundtrip, failed CAS, concurrent atomic update/load and final counter checks.

The first lock candidate selected upstream's Critical Section fallback. Actual
ME returned error 120 (`ERROR_CALL_NOT_IMPLEMENTED`) from the exported
`TryEnterCriticalSection`. Its passing import audit was insufficient. The Win9x
branch now uses an atomic owner ID, owner-only recursion count, release
publication and a one-millisecond sleep while contended. Other Windows builds
retain upstream SRW locks. This backend does not promise fairness, priority
inheritance or recovery after an owner dies.

## Actual guest results

Both programs exited 0 under a separate bounded native runner. The lock probe
recorded recursive depths 3, 2 and 1, blocked ownership transfer until the final
release, and completed two contending threads with a final counter of 100,001.
It also checked a failed try-lock from another thread and teardown. All logged
API errors were zero. Native logs use `CreateFileA`/`WriteFile`, so absent console
output cannot hide failures.

| Artifact | SHA-256 |
| --- | --- |
| Pair executable | `2c2873fa03a33c6ff462d829fdee1913945ecea40f783b6a44f6d5c70cff51b5` |
| Lock executable | `4c29b4bca65262ccd2f48ca3106b89e710499fd45646f3848bcad3e1561625b5` |
| Runner result log | `cf4f6354b33495a40eead745d7e117acfe265cf399821104f2cef5aa7f83896b` |
| Native lock log | `48565de6dd840704f88c41c583ff4ffad4670e1291d908dfba56185162ccac73` |
| Lock stdout | `892a5dab3ae74cd8b15dde97873e63558e682482f3da4511c6ab51d8d6478f92` |

The external evidence bundle is
`/srv/zuku/deploy-work/20260924-allocator-r3-guest/handoff.json`; its directory
label does not override the execution date above. It records launch arguments,
media/artifact hashes, guest logs, shutdown screenshot and postflight checks.
The guest reached its safe-to-power-off screen. QEMU then stopped; its three
backing images retained their initial hashes, the writable overlay passed
`qemu-img check`, no QA VM remained active, and production health was HTTP 200.

## Build evidence and next gate

The actual `bmalloc` static target rebuilt successfully with this patch, and
the actual header compiles in C and C++23. The modern-declaration regression
build retains SRW imports. Eight porting tests passed at this checkpoint. Its
next compiler gate was `DateMath.cpp`; subsequent focused work is recorded in
[DATE-OFFSET.md](DATE-OFFSET.md). The baseline ICU had four missing ME imports; later dependency work is recorded
in [ICU.md](ICU.md). No JSC
executable or complete engine has been produced.

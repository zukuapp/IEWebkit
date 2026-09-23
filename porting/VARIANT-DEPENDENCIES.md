# Variant dependency audit

This checkpoint audits the 17 profiles declared in `../variants.json`, grouped
by their engine ABI. It does not add certifications. All full-engine
certification lists remain empty. The host adapter matrix and a dependency
probe are distinct from a linked WebKit engine.

| Engine profile | Declared targets | Engine/dependency evidence and outstanding work |
| --- | --- | --- |
| `win9x-x86` | IE5.5 and IE6 on ME | Actual ME/IE5.5 selected allocator, ICU78.3 and standard-timezone probes pass. IE6 has no equivalent engine guest run. C_LOOP/JIT-off/WASM-off JSC bootstrap compiles selected objects; no JSC executable. New truncate object passes; stack limits and WTF thread/file/CRT integration remain. |
| `nt5` | IE6, IE7 and IE8 on XP SP3 / XP x64 SP2 | Six OS/IE profiles; no NT5 JSC or WebCore build/guest certification. New pre-Vista truncate branch has host boundary tests only; missing modern synchronization and stack APIs remain. Neither ME x86 nor host DLL results establish XP x64 dependency compatibility. |
| `nt6` | IE7–9 on Vista SP2; IE8–11 on Windows7 SP1 | Seven profiles; no full engine build/guest certification. Vista file-size API branch retained. Pinned source's Windows8+ stack-limit call also needs a tested earlier-NT implementation here. Windows10 header defaults cannot be treated as Vista/Windows7 compatibility. |
| `nt6-appcontainer` | IE10 on Windows8; IE11 on Windows8.1 | Two profiles; no full engine build/guest certification. Existence of the stack API does not certify all dependency imports. Classic, Protected and Enhanced Protected Mode need separate actual process/bitness tests without weakening mode. |

The pinned dependencies remain modern WebKitGTK2.54.0 sources (with exact same
commit Windows supplements) and ICU78.3. The experimental ME interpreter profile
cannot yet run WebAssembly because upstream C_LOOP and WebAssembly options
conflict. This is an explicit bootstrap limitation, not a reduced final scope.

WebCore has no linked artifact: software paint/view integration and its font,
image, XML, network/TLS and other dependencies remain separate build/runtime
gates. This audit does not certify the independently developed IE DocObject,
renderer or TLS paths. See [README.md](README.md), [ICU.md](ICU.md) and
[FILE-TRUNCATE.md](FILE-TRUNCATE.md) for the source pins and exact measured checks.

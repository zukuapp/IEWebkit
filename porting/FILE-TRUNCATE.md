# Pre-Vista file truncation checkpoint

The actual pinned `FileHandleWin.cpp` object failed under ME declarations at
`FILE_END_OF_FILE_INFO` and `SetFileInformationByHandle`. Microsoft's
[API contract](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle)
places the in-box API at Vista and later. Declaring it does not supply it to ME
or stock XP. The hash-pinned patch adds a `_WIN32_WINNT < 0x0600` branch; Vista+
retains the original API.

The older branch rejects negative offsets, saves the current cursor, seeks to
the requested size, calls `SetEndOfFile`, and restores the cursor. A truncation
failure keeps its original error even if restoration also fails. A successful
size change followed by failed restoration reports failure; the size change is
not rolled back. As with existing seek/read/write operations, access to a
shared handle/file pointer must be serialized by its owner. This is not an
atomic transaction across duplicate handles or concurrent file operations.
Microsoft documents the cursor and file-size semantics in
[SetFilePointer](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointer)
and [SetEndOfFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setendoffile).
Extended bytes are not promised to contain initialized data.

## Reproduce

```
python3 porting/apply-webkit-patches.py --source /external/webkitgtk-2.54.0
python3 porting/build-file-truncate-probes.py \
    --source /external/webkitgtk-2.54.0 --output /external/truncate-probes
ninja -C /external/build-me-jsc -j1 \
    Source/WTF/wtf/CMakeFiles/WTF.dir/win/FileHandleWin.cpp.obj
```

The fixture extracts the exact hash-checked production method. Each pre-Vista
profile (ME declarations and NT5 declarations) passes 11 checks: shrinking,
extending, zero and 64-bit sizes, negative offset, absent handle, initial and
target seek failures, truncation failure, restoration failure, and both failures
with primary-error precedence. Two modern-profile checks retain the original
API's success/failure behavior. The original body failed the pre-Vista fixture.
These are boundary mocks, not a claimed ME/XP filesystem test.

The complete actual WTF object now compiles with one job. SHA-256:
`7693aa06635d82952fb946e830ed1032ef17780e922a34e15acedc87bbd38530`.
The native x86 probe links and passes the pinned ME DLL import audit with zero
gaps, SHA-256:
`babb94a70888b3f8bb9eb04aab8394d1ae9bf74d00549ea98491a97003f62e57`.
It has **not run in a guest**. It uses `CREATE_NEW` for `C:\IEWKTRNC.TMP`, fails
if that path already exists, deletes its own fixture on completion and writes
`C:\TRUNCDIAG.LOG`. It checks small actual file sizes and cursor preservation;
the host mock's 64-bit case does not establish FAT32 capacity.

Build/probe logs are external at
`/srv/zuku/deploy-work/20260924-file-truncate/`.

## Next actual compiler gate

A separate focused `StackBounds.cpp` compile now fails at
`GetCurrentThreadStackLimits`, a
[Windows 8+ API](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadstacklimits).
No fallback was guessed: stack guard, reserve/commit boundaries, worker/main
thread and collector invariants need dedicated tests. JSC has not linked, and
no engine or renderer pass follows from this file-operation object.

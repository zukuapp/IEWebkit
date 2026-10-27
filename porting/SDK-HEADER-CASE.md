# Windows SDK header spelling checkpoint

Pinned upstream: WebKitGTK 2.54.0, commit
`5220e80b97a253c60ed899361654142ab5021998`.

## Reproduced failure and change

The real ME-declaration `win/FileSystemWin.cpp` compile stopped in
`Source/WTF/wtf/win/PathWalker.h` at `#include <Windows.h>`. Linux's case-sensitive
MinGW SDK contains `windows.h`. The pinned patch uses that canonical spelling.
It changes neither declarations nor Windows runtime behavior; it also works on
case-insensitive Windows build hosts.

## Reproduce

Apply the hash-checked patch bundle, then compile the actual objects:

```
python3 porting/apply-webkit-patches.py --source "$IEWK_SOURCE"
ninja -C "$IEWK_BUILD" -j1 \
  Source/WTF/wtf/CMakeFiles/WTF.dir/win/FileSystemWin.cpp.obj \
  Source/WTF/wtf/CMakeFiles/WTF.dir/win/PathWalker.cpp.obj
python3 -m unittest discover -s porting/tests -v
```

`IEWK_SOURCE` and `IEWK_BUILD` refer to the external hydrated source and
configured ME JSC build described in README.md. GCC/MinGW 15.1.1 compiled both
full objects. Reapplying the bundle is idempotent. Neighboring `LanguageWin`,
`OSAllocatorWin`, `MainThreadWin`, and `MemoryFootprintWin` objects also compile
unchanged. A further bounded compile passes `CPUTimeWin`, `DbgHelperWin`,
`LoggingWin`, `MappedFileDataWin`, `SignalsWin`, `WTFCRTDebug`, `Win32Handle`,
`MemoryPressureHandlerWin` and `generic/RunLoopGeneric`.

Object SHA-256:

- FileSystemWin: `7344d71008baf0fb7a4f428634b9f8e43b1d6d3a82f19d380f1860ec5d0e525e`
- PathWalker: `6ef5b06f42972b9b275894116c73a919975a25e06998fabbca9b56354664b5a7`

## Static import check

An external import-surface probe references the ten imported Windows APIs
identified by `i686-w64-mingw32-nm -u` on those exact objects:
`CreateFileW`, `FindClose`, `FindFirstFileW`, `FindNextFileW`,
`GetDiskFreeSpaceW`, `GetFileInformationByHandle`, `GetLastError`,
`GetTempPathW`, `SHGetFolderPathW`, and `WideCharToMultiByte`.
It is linked with `-static -static-libgcc -lshell32` and ME declarations.

The linked probe's imports all exist in the pinned ME DLL baseline:

- Probe: `57b1966be092322141cf7f8bd735c46e4e79c22622254acc2b237d961fabf3d0`
- Baseline manifest: `38cdb3eaadce250b5ec96234a92a14c6a258920e856bd94fc95c5e70ce82d7c9`

The probe only checks that the objects' direct Windows API import surface can
be represented by a linked PE and resolved against the baseline. Windows ME may
export Unicode API stubs; actual Korean paths, directory enumeration, storage
location and file operations require guest tests.

CXX ?= g++
WINCXX ?= i686-w64-mingw32-g++
TARGET = -DWINVER=0x0410 -D_WIN32_WINNT=0x0400 -D_WIN32_WINDOWS=0x0410
CXXFLAGS = -std=c++11 -Wall -Wextra -Werror -O2
.PHONY: test win32 preview
test:
	mkdir -p build
	$(CXX) $(CXXFLAGS) subset.cpp test.cpp -o build/subset_test
	build/subset_test
win32:
	mkdir -p build
	$(WINCXX) $(CXXFLAGS) $(TARGET) -c subset.cpp -o build/subset-win32.o
	$(WINCXX) $(CXXFLAGS) $(TARGET) -c win32_view.cpp -o build/view-win32.o
	i686-w64-mingw32-ar rcs build/iewk-subset.a build/subset-win32.o build/view-win32.o
	$(WINCXX) $(CXXFLAGS) $(TARGET) -shared -static -static-libgcc -static-libstdc++ -Wl,--kill-at,--major-subsystem-version,4,--minor-subsystem-version,0 win32_exports.cpp build/subset-win32.o build/view-win32.o -o build/iewk-subset.dll -lgdi32 -luser32 -lkernel32
preview:
	mkdir -p build
	$(CXX) $(CXXFLAGS) subset.cpp preview.cpp -o build/preview
	build/preview example.html build/example.svg

.PHONY: sanitize
sanitize:
	mkdir -p build
	$(CXX) -std=c++11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer subset.cpp test.cpp -o build/subset_sanitize
	ASAN_OPTIONS=detect_leaks=1 build/subset_sanitize

.PHONY: lab
lab: win32
	$(WINCXX) $(CXXFLAGS) $(TARGET) -DIEWK_SUBSET_LAB -I ../include -shared -static -static-libgcc -static-libstdc++ -Wl,--kill-at,--major-subsystem-version,4,--minor-subsystem-version,0 ../host/docobject.cpp lab/bridge.cpp -o build/SUBHOST.DLL -ladvapi32 -luser32 -lole32 -loleaut32 -luuid -lurlmon
	$(WINCXX) $(CXXFLAGS) $(TARGET) -mwindows -static -static-libgcc -static-libstdc++ -Wl,--major-subsystem-version,4,--minor-subsystem-version,0 lab/install.cpp -o build/SUBSETUP.EXE -luser32 -lole32 -loleaut32 -luuid
	cp build/iewk-subset.dll build/SUBSET.DLL
	python3 -c 'from pathlib import Path; Path("build/DEMO.IWK").write_bytes(b"IEWKSUBSET/1\n"+Path("example.html").read_bytes())'

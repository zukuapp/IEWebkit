CC ?= gcc
WINCC ?= i686-w64-mingw32-gcc
WINCXX ?= i686-w64-mingw32-g++
TARGET = -DWINVER=0x0410 -D_WIN32_WINDOWS=0x0410 -D_WIN32_WINNT=0x0400
CFLAGS = -Wall -Wextra -Werror -Os -I ../include
.PHONY: test objects
test:
	mkdir -p build
	$(CC) -std=c99 $(CFLAGS) engine_contract.c ../test/engine_contract_test.c -o build/engine_contract_test
	build/engine_contract_test
objects:
	mkdir -p build
	$(WINCC) -std=c99 $(CFLAGS) $(TARGET) -c engine_contract.c -o build/engine_contract.o
	$(WINCXX) -std=c++11 $(CFLAGS) $(TARGET) -c engine_loader.cpp -o build/engine_loader.o
.PHONY: docobject
docobject: objects
	$(WINCXX) -std=c++11 $(CFLAGS) $(TARGET) -c docobject.cpp -o build/docobject.o
	$(WINCXX) -shared -static -static-libgcc -static-libstdc++ -Wl,--kill-at -o build/iewebkit-host.dll build/docobject.o build/engine_loader.o build/engine_contract.o -ladvapi32 -luser32 -lole32 -loleaut32 -luuid -lurlmon
.PHONY: lifecycle-test-binary
lifecycle-test-binary: objects
	$(WINCXX) -std=c++11 $(CFLAGS) $(TARGET) -o build/HOSTTEST.EXE ../test/docobject_lifecycle_test.cpp build/engine_loader.o build/engine_contract.o -static -static-libgcc -static-libstdc++ -ladvapi32 -luser32 -lole32 -loleaut32 -luuid -lurlmon
.PHONY: navigation-test-binary
navigation-test-binary: objects
	$(WINCXX) -std=c++11 $(CFLAGS) $(TARGET) -o build/NAVTEST.EXE ../test/navigation_handoff_test.cpp navigation_handoff.cpp build/engine_loader.o build/engine_contract.o -static -static-libgcc -static-libstdc++ -Wl,--major-subsystem-version,4,--minor-subsystem-version,0 -ladvapi32 -luser32 -lole32 -loleaut32 -luuid -lurlmon
	$(WINCXX) -std=c++11 $(CFLAGS) $(TARGET) -o build/NAVVALUE.EXE ../test/navigation_values_test.cpp navigation_handoff.cpp build/engine_loader.o build/engine_contract.o -static -static-libgcc -static-libstdc++ -Wl,--major-subsystem-version,4,--minor-subsystem-version,0 -ladvapi32 -luser32 -lole32 -loleaut32 -luuid -lurlmon
.PHONY: navigation-adapter navigation-diagnostic
navigation-adapter: objects
	$(WINCXX) -std=c++11 $(CFLAGS) $(TARGET) -shared -static -static-libgcc -static-libstdc++ -Wl,--kill-at -o build/iewebkit-navigation.dll navigation_bho.cpp navigation_handoff.cpp build/engine_loader.o build/engine_contract.o -ladvapi32 -luser32 -lole32 -loleaut32 -luuid -lurlmon
navigation-diagnostic: objects navigation-test-binary
	$(WINCXX) -std=c++11 $(CFLAGS) $(TARGET) -DIEWK_NAV_DIAGNOSTIC -shared -static -static-libgcc -static-libstdc++ -Wl,--kill-at -o build/NAVBHO.DLL navigation_bho.cpp navigation_handoff.cpp build/engine_loader.o build/engine_contract.o -ladvapi32 -luser32 -lole32 -loleaut32 -luuid -lurlmon
	$(WINCXX) -std=c++11 $(CFLAGS) $(TARGET) -DIEWK_NAV_DIAGNOSTIC -shared -static -static-libgcc -static-libstdc++ -Wl,--kill-at -o build/IEWKHOST.DLL docobject.cpp build/engine_loader.o build/engine_contract.o -ladvapi32 -luser32 -lole32 -loleaut32 -luuid -lurlmon

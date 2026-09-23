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

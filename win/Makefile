IW := i686-w64-mingw32
XW := x86_64-w64-mingw32
CFLAGS  := -Os -ffunction-sections -fdata-sections -Wall -Wextra -municode -DUNICODE -D_UNICODE
LDFLAGS := -s -static -Wl,--gc-sections -Wl,--subsystem,console:6.00
LIBS    := -lkernel32 -luser32 -lshell32 -ladvapi32
COMMON  := sleepv.c sleepv.rc sleepv.exe.manifest

all: dist/sleepv-x86.exe dist/sleepv-x64.exe

dist/sleepv-x86.exe: $(COMMON)
	@mkdir -p dist
	$(IW)-windres -O coff -i sleepv.rc -o build-x86.res
	$(IW)-gcc $(CFLAGS) $(LDFLAGS) -o $@ sleepv.c build-x86.res $(LIBS)

dist/sleepv-x64.exe: $(COMMON)
	@mkdir -p dist
	$(XW)-windres -O coff -i sleepv.rc -o build-x64.res
	$(XW)-gcc $(CFLAGS) $(LDFLAGS) -o $@ sleepv.c build-x64.res $(LIBS)

gate: dist/sleepv-x86.exe dist/sleepv-x64.exe
	@fail=0; for f in dist/sleepv-x86.exe dist/sleepv-x64.exe; do \
	  case $$f in *x86*) od=i686-w64-mingw32-objdump;; *) od=x86_64-w64-mingw32-objdump;; esac; \
	  echo "-- $$f"; $$od -p $$f | grep 'DLL Name' || true; \
	  if $$od -p $$f | grep -qiE 'api-ms-|ext-ms-|vcruntime|ucrt|msvcp|concrt'; then echo "GATE FAIL: forbidden import in $$f"; fail=1; fi; \
	done; exit $$fail

clean:
	rm -f dist/*.exe build-*.res

.PHONY: all gate clean

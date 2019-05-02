
CC = gcc
CXX = g++
SDCC = wine sdcc.exe
CFLAGS = -mz80 --opt-code-speed --max-allocs-per-node 100000 -I.
LDFLAGS = --out-fmt-ihx --code-loc 0x1000 --data-loc 0x4800 --stack-loc 0x0fe0 -L.
SJASM = sjasm
EPCOMPRESS = epcompress

PROGRAM = midiplay.com
PROGRAM2 = midplay2.com
OBJS = main.rel daveplay.rel midi_in.rel envelope.rel eplib.rel exos.rel
OBJS2 = main.rel daveply2.rel midi_in.rel envelope.rel eplib.rel exos.rel

IHXNAME = $(patsubst %.com,%.ihx,$(PROGRAM))
IHXNAME2 = $(patsubst %.com,%.ihx,$(PROGRAM2))

all: $(PROGRAM) $(PROGRAM2) $(OBJS) daveply2.rel midi_asm.com mididisp.com

midiconv: midiconv_linux64 midiconv.exe

daveconv: daveconv_linux64 daveconv.exe

ihx2ep: ihx2ep.c
	$(CC) -Wall -O2 $< -o $@ -s

ihx2ep.exe: ihx2ep.c
	x86_64-w64-mingw32-gcc -Wall -static -O2 $< -o $@ -s

ihx2ep32.exe: ihx2ep.c
	i686-w64-mingw32-gcc -Wall -static -O2 $< -o $@ -s

loader.bin: loader.s
	$(SJASM) $< $@

$(PROGRAM): $(OBJS) ihx2ep loader.bin
	$(SDCC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(IHXNAME)
	./ihx2ep $(IHXNAME) loader.bin $@
	$(EPCOMPRESS) -m3 -nocleanup -noborderfx $@ $@

$(PROGRAM2): $(OBJS2) ihx2ep loader.bin
	$(SDCC) $(CFLAGS) $(LDFLAGS) $(OBJS2) -o $(IHXNAME2)
	./ihx2ep $(IHXNAME2) loader.bin $@
	$(EPCOMPRESS) -m3 -nocleanup -noborderfx $@ $@

midi_asm.com: midiplay.s daveplay.s midi_in.s globals.s
	$(SJASM) $< $@
	$(EPCOMPRESS) -m3 -nocleanup -noborderfx $@ $@

mididisp.com: mididisp.s daveplay.s midi_in.s globals.s display.s pgmnames.s
	$(SJASM) $< $@
	$(EPCOMPRESS) -m3 -nocleanup -noborderfx $@ $@

$(OBJS): %.rel: %.c
	$(SDCC) $(CFLAGS) -c $<

daveply2.rel: daveplay.c daveplay.h envelope.h
	$(SDCC) $(CFLAGS) -DPANNED_NOTE_NEW=1 -c $< -o $@

daveplay.rel: envelope.h

midiconv_linux64: midiconv.cpp comprlib.cpp compress2.cpp compress2.hpp
	$(CXX) -m64 -Wall -O2 -fno-unsafe-math-optimizations -DPANNED_NOTE_NEW=1 $< -o $@ -s

midiconv.exe: midiconv.cpp comprlib.cpp compress2.cpp compress2.hpp
	i686-w64-mingw32-g++ -m32 -static -Wall -O2 -DPANNED_NOTE_NEW=1 $< -o $@ -s

daveconv_linux64: daveconv.cpp
	$(CXX) -m64 -Wall -O2 $< -o $@ -s

daveconv.exe: daveconv.cpp
	x86_64-w64-mingw32-g++ -m64 -static -Wall -O2 $< -o $@ -s

clean:
	-rm *.asm *.ihx *.lk *.lst *.map *.noi *.sym ihx2ep envelope.bin
	-rm *.rel loader.bin ihx2ep.exe ihx2ep32.exe

distclean: clean
	-rm $(PROGRAM) $(PROGRAM2) midi_asm.com mididisp.com
	-rm midiconv_linux64 midiconv.exe daveconv_linux64 daveconv.exe


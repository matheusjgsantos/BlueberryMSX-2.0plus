# BlueberryMSX 2.0 Plus — aarch64 Port: Full Change Record

Source material for the article. All work verified on a Raspberry Pi 3B running 64-bit
(aarch64) Raspberry Pi OS with the 800x480 DSI display. This branch also brings the RPMC
front-panel LEDs (74HC595) to 64-bit and documents the board (see §5 and `Doc/RPMC.md`).

## TL;DR

BlueberryMSX 2.0 Plus was written for 32-bit armhf. On a 64-bit (aarch64) Pi it failed in
two distinct ways:

1. **It would not build.** The newer 64-bit toolchain surfaced a dozen latent compile
   errors: missing includes, a `byte` typedef collision, a wrong pthread callback
   signature, pointer-constness errors, and C++ from the C++98 era that modern g++
   rejects by default.
2. **It built, but the screen stayed black.** A subtle ABI bug: two ROM-mapper stub
   functions were defined as `void`, while the caller used their return value as a status
   flag via `success &= romMapper...Create(...)`. The empty stubs never wrote the return
   register, so X0 still held the *first argument* (a string pointer). An even-aligned
   pointer ANDed into the success flag zeroed it — the emulator never created the MSX
   machine.

This document records every change, file by file, with the verified root cause at the
machine-code level.

---
## Release v2.0.2 (rom_tester utility)

No emulation code changes in this release.

* **New `rom_tester` standalone utility** (`rom_tester.c`) — fast ROM dumper that
  reads physical MSX cartridges from the RPMC board without launching the full
  emulator. Features:
  * Sub-slot auto-probing to detect where ROM data is mapped.
  * Page-switching for addresses beyond 0xBFFF (pages 4-7 remapped to
    0x6000-0x7FFF).
  * Repeat-read integrity check (10-sample verify per byte); mismatched bytes
    printed in red.
  * CRC-32 (IEEE) checksum of the dump.
  * Optional `--io` mode to dump all 256 I/O port values.
  * CLI flags: `-f` (output file), `-o` (offset, default 0x4000),
    `-s` (size, default 0x8000 = 32 KB), `-S` (parent slot 0 or 1).
* **New `Doc/RPMC_TESTER.md`** — complete reference for `rom_tester`:
  build, usage, slot mapping, privileges, sub-slot probing, and troubleshooting.
* **Updated `Makefile`** — added `rom_tester` and `clean_rom_tester` build targets;
  cleaned `clean` now removes the `rom_tester` binary as well.

### Verification

* `make rom_tester` builds cleanly (aarch64 ELF, statically links bcm2835).
* `./rom_tester -h` prints help and exits (exit 0).
* With a cartridge inserted in slot 0, `sudo ./rom_tester -f dump.rom` probes
  sub-slots, reports detected ROM page, writes dump to file, and prints CRC-32
  at exit.
* `--io` mode prints all 256 port values in hex.
* Same `MsxBusPi.c` GPIO/bus driver as the live emulator (compiled under
  `#define ROM_TESTER_BUILD`), so slot mapping and bus timing are consistent
  with `bluemsx-pi`.

---


## Release v2.0.1 (documentation & versioning)

No emulation code changes in this release.

* **New `Doc/ARCHITECTURE.md`** — code knowledge base: repository layout, build
  system (`Makefile` feature switches), startup sequence (`PiMain.c`), thread
  model, emulator core / real-time sync (`Emulator.c`), DRM/GBM/EGL video path
  (`PiVideo.c`), evdev keyboard bridge + shortcut system (`PiInput.c`,
  `PiShortcuts.c`), audio, GPIO LEDs, media/ROMs, and a key-file index.
* **README fixes** —
  * shortcut table corrected: **F6** = screenshot, **F7** = soft/hard reset
    (the two were swapped);
  * `config.txt` now documents the working set: `dtoverlay=vc4-kms-v3d`
    (was `vc4-fkms-v3d`), `disable_fw_kms_setup=1`, `arm_64bit=1`,
    `disable_overscan=1`, `arm_boost=1`;
  * added "Supported hardware" (Pi 3B aarch64 + DSI, Pi 4/400), Debian 13
    (trixie) note (`libasound2t64`), and updated the HDMI0 known issue (first
    connected KMS connector, DSI + HDMI both work).
* **Version tagging** — `VERSION = 2.0.1` in `Makefile`, passed as
  `-DBLUEMSX_VERSION` to the build; `bluemsx-pi` now prints a version banner
  at startup (`PiMain.c`).

---

## 1. The black-screen bug (root cause, verified)

### Symptom

Everything initialized — SDL, audio, udev, "Powering on" printed — but the screen stayed
black and no machine ever appeared. No crash, no error message: the board initialization
silently failed.

### Setup that triggered it

The default machine is **MSX2+** (`emulation.machineName=MSX2+` in `bluemsx.ini`).
`Machines/MSX2+/config.ini:29` hard-wires a Moonsound sound cartridge into a slot:

```
0 0 0 0 79 "Machines/Shared Roms/MOONSOUND.rom" ""
```

ROM type `79` is `ROM_MOONSOUND` (see `Src/Media/MediaDb.h`). So at every startup,
`Src/Board/Machine.c:1495` reaches:

```c
case ROM_MOONSOUND:
    success &= romMapperMoonsoundCreate(romName, buf, size, 640);
    buf = NULL; // Ownership transferred to emulation of moonsound
    break;
```

(`success` is initialized to `1` at Machine.c:1043 and is AND-combined with the return
value of every mapper it creates; `1` = success, `0` = failure.)

### Why the real mapper was not in the build

The Moonsound / MSX-Music / YM2413 cartridge emulators pull in a batch of OpenMSX C++
that does not compile cleanly under this toolchain, so the port removed them from the
Makefile. But the emulator core still *calls* those functions during board init, so the
build needed placeholder definitions. A stub file (`Src/Pi/stubs.c`) was created — and
the first version declared the two ROM mappers as **`void`**:

```c
void romMapperMoonsoundCreate(const char* filename, unsigned char* romData, int size, int sramSize) {}
void romMapperMsxMusicCreate(const char* filename, unsigned char* romData, int size, int slot, int subslot, int startPage) {}
```

### Why it linked but failed at runtime

The caller (`Machine.c`) compiles against the header declaration, which says `int`. The
`void` definition lives in a separate translation unit, so the compiler never sees the
conflict, and the linker does not check types. The mismatch is only visible in the ABI:

- The caller does `bl romMapperMoonsoundCreate` and then consumes X0.
- The stub body is empty, so it **never writes X0**.
- X0 on return is therefore whatever the caller put there: the *first argument*,
  `romName` — a pointer into `.rodata`, always even-aligned on aarch64.
- `success` starts at 1, so the compiler optimizes `success &= f(...)` to
  `success = f() & 1`.
- Even pointer & 1 = **0**. Deterministic, every boot.

### Reproduction (10 lines, machine code)

Minimal two-TU repro built with `gcc -w -O3` on the Pi:

```
$ ./voidver
success=0
```

Caller disassembly (the whole story in three instructions):

```
698:  adrp x0, ...        ; x0 = romName pointer (1st arg)
69c:  add  x0, x0, #0x828
6a0:  bl   romMapperMoonsoundCreate
6a4:  and  w1, w0, #0x1   ; success = X0 & 1  -> 0
```

Stub disassembly:

```
0:  ret                   ; X0 never written
```

### The fix (`Src/Pi/stubs.c`)

```c
#include <stdlib.h>
int romMapperMoonsoundCreate(const char* filename, unsigned char* romData, int size, int sramSize)
{ free(romData); return 1; }
int romMapperMsxMusicCreate(const char* filename, unsigned char* romData, int size,
                            int slot, int subslot, int startPage)
{ return 1; }
```

`return 1` matches the real mappers' "loaded successfully" convention.

The `free` asymmetry is intentional and follows the caller's ownership contract:

- **MOONSOUND** (Machine.c:1496-1497): the caller sets `buf = NULL` right after the
  call ("Ownership transferred to emulation"), so the mapper — or its stub — must free
  the buffer. The stub frees it.
- **MSXMUSIC** (Machine.c:1676): no ownership transfer; the common
  `if (buf != NULL) free(buf);` at the end of the switch frees the buffer. The stub must
  **not** free it (double free).

The remaining `ym2413*` / `moonsound*` stubs are no-op `void` functions — safe, because
none of their return values is consumed.

### Debugging trail (for the article)

1. `strace` of the boot showed a normal startup — no failing syscall to point at.
2. Added `[DBG]` fprintf instrumentation across the init path (PiMain, Board, MSX,
   Machine, Emulator, RomLoader, R800).
3. The prints showed board init starting, the ROM mappers being created, then the overall
   init reporting failure — while each individual step "looked fine".
4. The missing insight: the mapper return values were never printed — they were being
   `&=`-ed straight into a flag. Checked the stub signatures: `void`.
5. Checked the AArch64 AAPCS: a `void` function may return with X0 holding the caller's
   first argument. Built the minimal two-TU repro, confirmed `success=0` and the
   `and w1, w0, #0x1` in the disassembly.
6. Fixed the signatures, rebuilt — screen lit up on the first try.
7. Removed all `[DBG]` instrumentation. Six files that carried *only* debug prints
   (Board.c, MSX.c, Machine.c, Emulator.c, RomLoader.c, R800.c) were restored to pristine
   HEAD; PiMain.c and PiVideo.c kept their real fixes, debug removed.

---

## 2. Build fixes (aarch64 compile errors)

### `Makefile`

- Added `CXXFLAGS = -std=c++98 -Wno-error=ambiguous -fpermissive` — the OpenMSX C++ in
  the tree is from the C++98 era; modern g++ rejects it by default.
- Added to CFLAGS: `-Wno-implicit-function-declaration -Wno-incompatible-pointer-types
  -Wno-int-conversion` — the original C has unprototyped calls and implicit conversions
  that are hard errors under the 64-bit toolchain. Real bugs were fixed in source (below);
  the rest was suppressed to keep the diff honest.
- Removed `LIBS += -lwiringPi` and added `-lbcm2835` to `LIBS` — WiringPi does not
  build on aarch64; the bcm2835 lib (built statically) is now the GPIO driver for both
  32-bit and 64-bit (see PiGpio.c and §5).
- Added `-DRASPI_GPIO` to `COMMON_FLAGS` so the Pi slot/LED code paths compile.
- Removed from `SOURCE_FILES`: `romMapperMoonsound.c`, `romMapperMsxMusic.c`,
  `Moonsound.c`, `OpenMsxYM2413.cpp`, `OpenMsxYM2413_2.cpp`, `OpenMsxYMF262.cpp`,
  `OpenMsxYMF278.cpp`, `YM2413.cpp` (the C++ sound emulators, see §1).
- Added `stubs.c` to `SOURCE_FILES`.

### `Src/Pi/PiGpio.c`

WiringPi is armhf-only, so the whole 74HC595 shift-register driver was rewritten on top
of the **bcm2835** lib (works on armhf *and* aarch64). The guard is now
`#if defined(__arm__) || defined(__aarch64__)` and the three entry points
(`gpioInit`, `gpioShutdown`, `gpioUpdateLeds`) are real implementations that drive the
board's front panel: 74HC595 on SRCLK=GPIO22 / RCLK=GPIO23 / SER=GPIO26 (LSB-first),
verified bit map PWR=0x80, SLT2=0x40, SLT1=0x20, I/O=0x10, HAN=0x08, CAPS=0x04
(6 LEDs, no FDD/TURBO on this board — see §5 and `Doc/RPMC.md`). If `bcm2835_init()`
fails (e.g. no `/dev/mem` permission) it prints "slot LEDs disabled" and the emulator
continues without LEDs. The committed binary links bcm2835 **statically**, so a stock
OS image needs no extra GPIO package.

### `Src/SoundChips/OpenMsxY8950Adpcm.h`, `OpenMsxYM2413.h`, `OpenMsxYM2413_2.h`, `OpenMsxYMF262.h`, `OpenMsxYMF278.h`

`typedef unsigned char byte;` collided with a system-provided `byte` type under the
aarch64 toolchain. Renamed the typedef to `byte_t` and updated all uses in
`OpenMsxYMF278.h` (struct members, method signatures, pointers).

### `Src/Pi/PiUdev.c`

- Added missing includes: `Disk.h`, `pthread.h`, `unistd.h`.
- `udevMon()` is passed to `pthread_create`, which expects `void*(*)(void*)`. The old
  `void udevMon(void*)` is an incompatible pointer on 64-bit (and would misread the
  thread's return path). Fixed the signature to `static void* udevMon(void*)` and added
  `return NULL;` at the end.

### `Src/Pi/PiNotifications.c`

- Added missing includes: `string.h`, `Crc32Calc.h`, `ziphelper.h`.
- `compressedSize` widened from `int` to `unsigned long` (zlib compressed sizes).

### `Src/Pi/PiMouse.c`

`sdlCreateCursor()` takes `const char**`; the X11 cursor maps are non-const arrays, so
the calls now cast explicitly: `sdlCreateCursor((const char**)arrow, 0, 0)` (and the
cross cursor likewise).

### `Src/IoDevice/MidiIO.c`

MIDI *file* I/O was unguarded: with an empty filename or a failed `fopen`, the code ran
`setbuf`/`fclose`/`fwrite` on a NULL handle (crash or silent UB depending on the path).
Now:

- empty filename (`theOutFileName[0] == '\0'`) is treated as "no MIDI file";
- `midiIo->outFile`/`inFile` are NULL-initialized and NULL-checked before
  `setbuf`/`fwrite`/`fclose`, and cleared to NULL after `fclose`.

---

## 3. Display-path fixes (`Src/Pi/PiMain.c`, `Src/Pi/PiVideo.c`)

### SDL subsystem init (PiMain.c)

`SDL_Init(SDL_INIT_EVERYTHING)` initializes SDL's video driver, which grabs DRM/KMS and
fights the application's own raw DRM/GBM/EGL path. Replaced with the exact subsystems the
emulator needs:

```c
SDL_InitSubSystem(SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO)
```

### Event handlers (PiMain.c)

The keyboard/joystick handlers were called with the whole `SDL_Event*` instead of the
specific union member. `SDL_KeyboardEvent`, `SDL_JoyButtonEvent` and `SDL_JoyAxisEvent`
have different layouts, so the handlers were reading unrelated fields (SDL2's key values
are 32-bit — the old "garbage keyboard mapping" behavior). Fixed:

```c
joystickButtonUpdate(&event->jbutton);
joystickAxisUpdate(&event->jaxis);
keyboardUpdate(&event->key);   /* KEYDOWN and KEYUP */
```

Also added the missing `#include "PiMouse.h"` / `#include "PiInput.h"` and forward
declarations for `keyboardInit`, `keyboardUpdate`, `joystickButtonUpdate`,
`joystickAxisUpdate`, `actionToggleVideoColorMode` so the file compiles under stricter
implicit-declaration checking.

### EGL API (PiVideo.c)

`eglBindAPI(EGL_OPENGL_API)` → `eglBindAPI(EGL_OPENGL_ES_API)`. The VideoCore V3D
driver exposes OpenGL ES 2, not desktop OpenGL.

### Framebuffer creation (PiVideo.c)

`drmModeAddFB()` (deprecated) replaced with `drmModeAddFB2()` using an explicit pixel
format and handle/pitch/offset arrays:

```c
uint32_t handles[4] = { handle, 0, 0, 0 };
uint32_t pitches[4] = { pitch, 0, 0, 0 };
uint32_t offsets[4] = { 0, 0, 0, 0 };
int ret = drmModeAddFB2(device, mode.hdisplay, mode.vdisplay, DRM_FORMAT_XRGB8888,
                        handles, pitches, offsets, &fb, 0);
if (ret) fprintf(stderr, "drmModeAddFB2 failed: %d\n", ret);
```

`drmModeSetCrtc()` now also reports failure instead of ignoring it:

```c
ret = drmModeSetCrtc(device, crtc->crtc_id, fb, 0, 0, &connectorId, 1, &mode);
if (ret) fprintf(stderr, "drmModeSetCrtc failed: %d\n", ret);
```

(Both error prints are part of the real fix — they only fire on genuine failure. When
running over SSH, `drmModeSetCrtc failed: -13` / EPERM is expected, because the local
console owns the CRTC; from the console it is silent.)

### Shader uniform (PiVideo.c)

`glUniformMatrix4fv(sh->u_vp_matrix, 1, GL_FALSE, projection)` →
`glUniformMatrix4fv(sh->u_vp_matrix, 1, GL_FALSE, (const GLfloat *)projection)` —
pointer-type mismatch that the 64-bit toolchain rejects.

### Includes (PiVideo.c)

Added `string.h` (in both include blocks) and `drm/drm_fourcc.h` (for
`DRM_FORMAT_XRGB8888`).

---

## 4. Verification

- **Build**: `make -j4` — clean, 0 errors, `stubs.o` linked into `bluemsx-pi`.
- **Smoke test** (headless, over SSH): `timeout -s KILL 25 ./bluemsx-pi` — the process
  runs the full 25 s and is killed only by the timeout (exit 137); no crash, assert,
  abort, or segfault. Boot log reaches "Powering on" and the emulation loop.
- **On device**: the DSI 800x480 display shows the emulator; games run (verified with
  `ROM/knightmare.rom`).
- **Unit repro**: the two-TU void-stub repro from §1 prints `success=0` before the fix
  and `success=1` with the `int ... return 1` stub — same flags as the project build.
- **Instrumentation audit**: `grep -rnF '[DBG]' Src/` returns nothing; the six
  debug-only files are byte-identical to HEAD (`git diff` empty).

## 5. Front-panel LEDs (74HC595) — working on both 32-bit and 64-bit

The RPMC board's front panel is driven by a single 74HC595 shift register. Full wiring
(SRCLK=GPIO22, RCLK=GPIO23, SER=GPIO26, LSB-first) and the complete board pin-out are in
`Doc/RPMC.md`.

### Verified LED bit map (sweep test)

Each bit was shifted out with a standalone bcm2835 sweep program (`/tmp/sweep_leds.c` on
the Pi) while watching the panel:

| Bit | Value | LED |
|-----|-------|-----|
| 7 | 0x80 | PWR |
| 6 | 0x40 | SLT2 |
| 5 | 0x20 | SLT1 |
| 4 | 0x10 | I/O |
| 3 | 0x08 | HAN (Kana) |
| 2 | 0x04 | CAPS |
| 1/0 | 0x02/0x01 | not connected |

The board has **6 LEDs — no FDD or TURBO**. The legacy status builder in `Emulator.c`
(compiled out — `RPMC_FRONTLED` is not defined) assumed an 8-LED panel (FDD2=bit4,
FDD1=bit1, TURBO=bit0) and never matched the board.

### What changed

- `Src/Pi/PiGpio.c` — rewritten on bcm2835 (see §2); PWR bit set in `gpioInit()`,
  cleared in `gpioShutdown()`; `gpioUpdateLeds()` recomputes I/O = SLT1 || SLT2 and only
  re-shifts when the byte changes; called every frame from `PiMain.c`
  (`EVENT_UPDATE_DISPLAY`).
- `Src/IoDevice/MsxBus.cpp` — `readMemory()` now sets the per-slot busy flag (throttled
  to ~1/100 reads) under `RASPI_GPIO`, so the SLT1/SLT2 LEDs light while a slot is being
  accessed.
- `Src/IoDevice/Led.c` — fixed `ledSetCapslock()`, which called the *setter*
  `ledSetSlot2Busy()` where it needed the *getter* `ledGetSlot2Busy()` (it would have
  clobbered the slot-2 busy flag with 0).
- `Src/IoDevice/MsxBusPi.c` — `frontled()` (the other 74HC595 driver, raw GPIO, from
  the 2016 msxslot core) is kept but effectively dead code: its only live call is
  `frontled(0x0)` in `msxinit()` (a no-op — the `static oldbyte` starts at 0), and the
  per-frame status call sites are wrapped in `#ifdef RPMC_FRONTLED`, which the Makefile
  does not define.
- `Dockerfile.arm` / `Dockerfile.build.arm` — armhf cross-build image builds bcm2835
  from the in-tree `bcm2835-1.68` instead of WiringPi.

### Verification

- Bit map confirmed by the sweep test on the board (6 of 8 bits drive LEDs; bits 1–0 do
  nothing).
- Running a game: PWR lights at boot; SLT1/SLT2 light as the cartridge is read; CAPS and
  HAN follow the keyboard; I/O lights with cartridge I/O.
- `ldd bluemsx-pi` shows no `libbcm2835` — the driver is statically linked, so the
  committed aarch64 binary runs on a stock 64-bit OS image with no extra GPIO package.

## 6. Known limitations / follow-ups

- **Silent Moonsound / MSX-Music / YM2413**: the stubs make those cartridges *load*
  successfully but produce no sound — music in games that use them is missing. The
  proper follow-up is to re-introduce the real C++ emulators (the `CXXFLAGS` line is
  already in place) or port them to C.
- **HDMI0 only**: pre-existing issue, unchanged (see README "Known issues").
- **Keyboard mapping** (resolved): the union-member fix in §3 was the actual bug — the
  handlers were reading the wrong fields of `SDL_Event`. Verified working on hardware;
  moved to README "Resolved issues".

## 7. File-by-file change list

| File | Change |
|---|---|
| `Makefile` | `CXXFLAGS` added; warning suppressions; `-lwiringPi` removed; `-lbcm2835` + `-DRASPI_GPIO` added; 8 sound files removed from build; `stubs.c` added |
| `Src/Pi/stubs.c` | **new** — `int` ROM-mapper stubs with correct ownership semantics; no-op chip stubs |
| `Src/Pi/PiMain.c` | `SDL_InitSubSystem` instead of `SDL_Init(EVERYTHING)`; event union-member pointer fixes; includes + forward decls |
| `Src/Pi/PiVideo.c` | `EGL_OPENGL_ES_API`; `drmModeAddFB2` + error reporting on `drmModeSetCrtc`; `glUniformMatrix4fv` cast; includes |
| `Src/Pi/PiGpio.c` | rewritten on bcm2835 — 74HC595 front-panel LEDs (6 LEDs, verified bit map) work on armhf **and** aarch64; static link, graceful fallback |
| `Src/Pi/PiUdev.c` | missing includes; pthread-safe `udevMon` signature (`void*` + `return NULL`) |
| `Src/Pi/PiNotifications.c` | missing includes; `unsigned long` compressed size |
| `Src/Pi/PiMouse.c` | `(const char**)` cursor-map casts |
| `Src/IoDevice/MsxBusPi.c` | `frontled()` (74HC595, msxslot core) kept; effectively dead code — see §5 |
| `Src/IoDevice/MsxBus.cpp` | slot-busy flag in `readMemory()` (~1/100 reads) drives the SLT1/SLT2 LEDs |
| `Src/IoDevice/Led.c` | `ledSetCapslock()`: `ledSetSlot2Busy()` → `ledGetSlot2Busy()` |
| `Src/IoDevice/MidiIO.c` | NULL/empty-filename guards for MIDI file I/O |
| `Src/SoundChips/OpenMsxY*.h` (5 headers) | `byte` → `byte_t` typedef |
| `Dockerfile.arm`, `Dockerfile.build.arm` | bcm2835 (in-tree 1.68) replaces WiringPi in the armhf build image |
| `Doc/RPMC.md` | **new** — complete RPMC board reference (pin-out, bus protocol, GPCLK0, 595 LED map, build, test) |
| `README.md` | aarch64 install (no GPIO build step — bcm2835 static in the committed binary); WiringPi→bcm2835 in build/cross-compile; LED + keyboard moved to Resolved |
| `CHANGES.md` | this document |

## 8. Working-tree artifacts

- `ROM/knightmare.rom` — 32 KB test ROM used for on-device verification (untracked).
- `test_drm`, `test_drm.c` — standalone DRM test programs written during debugging
  (untracked, can be deleted).
- `*.predbg.bak`, `*.bak`, `*.orig` files — pre-debugging backups of touched sources
  (can be deleted).
- `output` — a 1116-line `strace` dump that was accidentally committed during debugging;
  deleted from the tree.
- `/tmp/preclean/` on the Pi — backups made by the cleanup script before the debug
  instrumentation was removed.
- `/tmp/abitest/` on the Pi — the two-TU ABI repro from §1 (`voidver`, `intver`,
  `main.c`, `stubs.c`, `header.h`).
- `/tmp/sweep_leds.c` (+ compiled `sweep_leds`) on the Pi — standalone bcm2835 bit-map
  sweep program used to verify the 74HC595 LED map (see `Doc/RPMC.md` §7).

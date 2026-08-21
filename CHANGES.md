# BlueberryMSX 2.0 Plus — aarch64 Port: Full Change Record

Source material for the article. All work verified on a Raspberry Pi 3B running 64-bit
(aarch64) Raspberry Pi OS with the 800x480 DSI display.

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
- Removed `LIBS += -lwiringPi` — WiringPi does not build on aarch64 (see PiGpio.c).
- Removed from `SOURCE_FILES`: `romMapperMoonsound.c`, `romMapperMsxMusic.c`,
  `Moonsound.c`, `OpenMsxYM2413.cpp`, `OpenMsxYM2413_2.cpp`, `OpenMsxYMF262.cpp`,
  `OpenMsxYMF278.cpp`, `YM2413.cpp` (the C++ sound emulators, see §1).
- Added `stubs.c` to `SOURCE_FILES`.

### `Src/Pi/PiGpio.c`

WiringPi is armhf-only. The whole shift-register LED code (CLOCK/LATCH/DATA pins,
`gpioShiftLeds`) is now wrapped in `#ifdef __arm__ ... #else ... #endif`; on aarch64 the
three entry points (`gpioInit`, `gpioShutdown`, `gpioUpdateLeds`) compile as empty
functions. **Consequence: slot-board LEDs do not light up on 64-bit.**

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

## 5. Known limitations / follow-ups

- **Silent Moonsound / MSX-Music / YM2413**: the stubs make those cartridges *load*
  successfully but produce no sound — music in games that use them is missing. The
  proper follow-up is to re-introduce the real C++ emulators (the `CXXFLAGS` line is
  already in place) or port them to C.
- **GPIO slot LEDs off on aarch64**: WiringPi is armhf-only; the `#ifdef __arm__` guard
  makes the LED functions no-ops on 64-bit.
- **HDMI0 only**: pre-existing issue, unchanged (see README "Known issues").
- **Keyboard mapping**: pre-existing issue; the union-member fix in §3 is a step in the
  right direction but a full rework is still wanted (see README "Known issues").

## 6. File-by-file change list

| File | Change |
|---|---|
| `Makefile` | `CXXFLAGS` added; warning suppressions; `-lwiringPi` removed; 8 sound files removed from build; `stubs.c` added |
| `Src/Pi/stubs.c` | **new** — `int` ROM-mapper stubs with correct ownership semantics; no-op chip stubs |
| `Src/Pi/PiMain.c` | `SDL_InitSubSystem` instead of `SDL_Init(EVERYTHING)`; event union-member pointer fixes; includes + forward decls |
| `Src/Pi/PiVideo.c` | `EGL_OPENGL_ES_API`; `drmModeAddFB2` + error reporting on `drmModeSetCrtc`; `glUniformMatrix4fv` cast; includes |
| `Src/Pi/PiGpio.c` | `#ifdef __arm__` guard — LED functions are no-ops on aarch64 |
| `Src/Pi/PiUdev.c` | missing includes; pthread-safe `udevMon` signature (`void*` + `return NULL`) |
| `Src/Pi/PiNotifications.c` | missing includes; `unsigned long` compressed size |
| `Src/Pi/PiMouse.c` | `(const char**)` cursor-map casts |
| `Src/IoDevice/MidiIO.c` | NULL/empty-filename guards for MIDI file I/O |
| `Src/SoundChips/OpenMsxY*.h` (5 headers) | `byte` → `byte_t` typedef |

## 7. Working-tree artifacts

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

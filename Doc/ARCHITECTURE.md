# BlueberryMSX 2.0 Plus - Architecture & Code Knowledge Base

This document describes the internal structure of the BlueberryMSX 2.0 Plus
Raspberry Pi port (aarch64). It is a reference for anyone working on the
codebase. File:line references are against the current tree.

## 1. Overview

BlueberryMSX is a hardware MSX emulator that runs full-screen and unattended
on a Raspberry Pi (no X11, no windowing). It is a port of the original
BlueMSX 2.0 codebase (SDL 1.2, RISC OS + ARM + x86) to:

- **aarch64 Linux** (Debian 13 "trixie" / Raspberry Pi OS 64-bit)
- **SDL 2.0** for the non-video subsystems (audio, timer, joystick, events)
- **DRM/KMS + GBM + EGL (OpenGL ES 2.0)** for frame presentation, replacing
  the old DispmanX/RISC OS video path

The Pi runs as an MSX2+ console: screen out over DSI/HDMI, front-panel LEDs
via a 74HC595 shift register on GPIO, and (on this build) an MSX expansion
board (RPMC) on the expansion bus. See `Doc/RPMC.md` for the board.

## 2. Repository layout

    BlueberryMSX-2.0plus/
    |-- Makefile                 single makefile, builds bluemsx-pi
    |-- README.md                user-facing docs
    |-- CHANGES.md               port change record (build + black-screen fixes)
    |-- Doc/
    |   |-- RPMC.md              MSX expansion board (GPIO + I/O)
    |   |-- GPIO_schema.png      board GPIO wiring
    |   `-- ARCHITECTURE.md      this file
    |-- Machines/                machine definitions (MSX1, MSX2, MSX2+, ...)
    |   `-- MSX2+/config.ini     slot/RAM/VDP/cassette config per machine
    |-- Media/                   ROM/cassette/disk images (user media)
    `-- Src/
        |-- Arch/                architecture *headers* (event, file, sound, timer, thread, ...)
        |-- Pi/                  **the Pi port** (entry point, video, input, shortcuts, GPIO, udev)
        |-- Sdl/                 SDL2 implementations of the Arch layer (sound, timer, thread, ...)
        |-- Emulator/            emulator core (Emulator.c, Actions, Properties, CommandLine)
        |-- Board/               MSX board: Z80+VDP glue (Board.c, Machine.c, MSX.c)
        |-- Z80/                 R800 CPU core (pure C under NO_ASM)
        |-- VideoChips/          VDP: CRTC6845, VDP, FrameBuffer, VideoManager
        |-- VideoRender/         frame post-processing: VideoRender, hq2x/3x, Scale2x/3x, Scalebit
        |-- SoundChips/          AY8910, YM2151, VLM5030, Moonsound, AudioMixer
        |-- IoDevice/            I/O: MsxBus (slots), FDC (WD2793, NEC765), SCSI/IDE,
        |                        I8254/8255/8250/8251, LED, JoystickIO, UartIO, ...
        |-- Input/               input devices: MsxJoystick, MsxMouse, Coleco*, Sg1000*, Svi*
        |-- Memory/              RAM, ROM mappers (romMapper*.c), SlotManager, DeviceManager
        |-- Media/               MediaDb (ROM type detection), Crc32Calc, Sha1
        |-- Language/            UI strings
        |-- Utils/               IniFileParser, ZipFromMem, PacketFileSystem, blowfish, ...
        |-- Common/              ArrayList, HashTable, DebugMemory
        |-- TinyXML/ Unzip/      vendored libs
        |-- Debugger/            debugger (active under -DUSE_DEBUGGER)
        `-- HwTest/              hardware self-test

Layering (bottom up):

    Z80 (R800)          ->  Board (MSX machine)     ->  Emulator core (threads, sync)
    VideoChips (VDP)    ->  VideoRender             ->  Pi/PiVideo.c (DRM/GBM/EGL)
    SoundChips          ->  AudioMixer              ->  Sdl/SdlSound.c (SDL audio)
    IoDevice            (MsxBus, FDC, SCSI, LED, GPIO)
    Pi/PiInput.c (evdev) + Pi/PiShortcuts.c         ->  SDL event queue

## 3. Build system (Makefile, 465 lines)

Single makefile, GNU make. Key facts:

- `CC=gcc`, `CXX=g++`. C++ is compiled with `-std=c++98 -Wno-error=ambiguous
  -fpermissive` (L30) - the original 2003-era code does not compile as C++11/14.
- `COMMON_FLAGS` (L40), the feature switches that define this port:
  - `-DUSESDL2 -DUSESDL2Main` - SDL 2 API (1.2-era code guarded by `#if USESDL2` blocks)
  - `-DUSE_EGL -DUSESDL_egl -DUSE-GLESv2` - OpenGL ES 2 via EGL (video)
  - `-DIS_RPI -DRASPI -DRASPI_GPIO` - Pi-specific paths and bcm2835 GPIO
  - `-DLSB_FIRST` - little-endian (ARM)
  - `-DNO_ASM` - pure-C R800 CPU core (no ARM asm)
  - `-DNO_HIRES_TIMERS` - 10 ms emulation sync period (see section 6)
  - `-DNO_FILE_HISTORY` - no recent-files UI
  - `-DNO_EMBEDDED_SAMPLES` - no built-in media
  - `SINGLE_THREADED` is **commented out** (L47) -> multithreaded build
- `CFLAGS` (L41): `-g -w -O3 -Wno-implicit-function-declaration
  -Wno-incompatible-pointer-types -Wno-int-conversion -ffast-math
  -fstrict-aliasing -fomit-frame-pointer -finstrument-functions`
- `LIBS` (L44): `-lSDL2main -lSDL2 -lz -lpthread -ludev -lbcm2835` +
  `pkg-config --libs libdrm` + `-lgbm -lGLESv2 -lEGL`
- `TARGET = bluemsx-pi` (L52); objects in `objs/`; per-file rule with `.d`
  dependency generation (L72-73).
- Build: `make -j$(nproc)` -> `bluemsx-pi`.
- Cross-compile from x86_64: `make ARM_CROSS_COMPILE=aarch64-linux-gnu-` (L21-22).
- Toolchain (native on the Pi): gcc 14.2.0 (Debian 13), make 4.4.1, SDL2 dev
  2.30, libudev, libdrm, libgbm, libEGL/libGLESv2, plus the in-tree bcm2835.

## 4. Entry point & startup (Src/Pi/PiMain.c, ~518 lines)

`main()` at `Src/Pi/PiMain.c:264`. Startup order (all line numbers in PiMain.c):

| # | Call | Line | Purpose |
|---|------|------|---------|
| 1 | `logInit()` / `LOG_INFO("BlueberryMSX 2.0 Plus v%s")` | 266-267 | spdlog init, then banner — silent by default (sec. 12) |
| 2 | `signal(SIGINT, SIG_IGN)` | 273 | Ctrl+C no longer kills; evdev grab also blocks it (sec. 8) |
| 3 | `gpioInit()` | 277 | bcm2835 init, LED shift-register PWR byte |
| 4 | `piInitVideo()` | 280 | DRM/KMS + GBM + EGL setup (sec. 7) |
| 5 | `piInitUdev()` | 285 | udev hotplug monitor (disk/cassette hot-insert) |
| 6 | `SDL_InitSubSystem(TIMER\|EVENTS\|GAMECONTROLLER\|AUDIO)` | 291 | **No SDL_INIT_VIDEO** - SDL must not touch DRM/KMS |
| 7 | `SDL_ShowCursor(DISABLE)`, `SDL_JoystickEventState(ENABLE)` | 304-305 | |
| 8 | parse argv into `szLine` | 309-324 | `char szLine[8192]` at 309 |
| 9 | open up to 2 SDL joysticks | 326 | `SDL_JoystickOpen(i)` |
| 10 | `setDefaultPaths(archGetCurrentDirectory())` | 329 | creates runtime dirs (sec. 12) |
| 11 | `emuCheckResetArgument(szLine)` | 331 | `-reset` flag |
| 12 | `propCreate(resetProperties, 0, P_KBD_EUROPEAN, 0, "")` | 337 | loads `bluemsx.ini` from CWD |
| 13 | `videoCreate`, `videoSetPalMode(VIDEO_PAL_FAST)` | 344-347 | PAL 50 Hz fast |
| 14 | video params from properties (saturation, scanlines, ...) | 348-353 | |
| 15 | `keyboardInit(properties)` | 359 | MSX keyboard table (EU layout) |
| 16 | `piKeyboardEvdevInit()` | 360 | evdev keyboard bridge thread (sec. 8) |
| 17 | `mixerCreate`, per-channel volume/pan/mute | 365-375 | |
| 18 | `emulatorInit(properties, mixer)` | 377 | |
| 19 | `actionInit(video, properties, mixer)` | 378 | |
| 20 | `tapeSetReadOnly` | 379 | cassette read-only by default |
| 21 | `langInit` / `langSetLanguage` | 381-382 | |
| 22 | `joystickPortSetType(0/1, ...)` | 384-385 | |
| 23 | printer/uart/midi/yk port setup | 387-398 | |
| 24 | `emulatorRestartSound()` | 400 | opens audio device (44100 Hz, SDL callback) |
| 25 | `videoUpdateAll()` | 402 | |
| 26 | `shortcuts = shortcutsCreate()` | 404 | loads `bluemsx.ini` [Shortcuts] (sec. 8) |
| 27 | insert cartridges/diskettes/cassettes from props | 410-440 | `insertCartridge()` at 410 |
| 28 | `machineCreate` test + destroy | 443-451 | `boardSetMachine()` at 445 |
| 29 | `boardSet*` (fdcTiming, y8950, ym2413, moonsound, autodetect) | 455-459 | |
| 30 | `emuTryStartWithArguments(...)` / `emulatorStart(NULL)` | 461, 471 | boots the machine |
| 31 | `piScanDevices()` | 480 | udev: hot-insert media found at boot |
| 32 | `LOG_INFO("Powering on")` | 482 | only printed when log level >= info |

**Main loop** (PiMain.c:484-493):

    doQuit = 0;
    while (!doQuit) {
        SDL_WaitEvent(&event);
        do {
            handleEvent(&event);
        } while (SDL_PollEvent(&event));
    }

**Teardown** (PiMain.c:494-513): `videoDestroy` -> `propDestroy` ->
`archSoundDestroy` -> `mixerDestroy` -> `gpioShutdown` -> `piDestroyVideo` ->
`piDestroyUdev` -> `piKeyboardEvdevDestroy` -> `SDL_Quit` ->
`LOG_INFO("Powered off")`.

### 4.1 Event dispatch - handleEvent() (PiMain.c:156-215)

| SDL event | Lines | Action |
|-----------|-------|--------|
| `SDL_USEREVENT` `EVENT_UPDATE_DISPLAY` (=2, L73) | 159-175 | first frame: `piMouseInit` + capture rect + `inputEventReset`; every frame: `piUpdateEmuDisplay()` (sec. 7) + `updateLeds()` (GPIO shift) |
| `SDL_JOYBUTTONDOWN/UP` | 177-191 | combo shortcuts on release: L+R=quit, L+BTN4=color mode, L+BTN2=scanlines, R=quick disk change; then `joystickButtonUpdate` (MSX gamepad) |
| `SDL_JOYAXISMOTION` | 192-193 | `joystickAxisUpdate` (+/-3200 deadzone) |
| `SDL_KEYDOWN/UP` | 195-202 | `keyboardUpdate()` (MSX key state) **and** `shortcutCheckDown/Up()` (F-key commands) |
| `SDL_MOUSEBUTTONDOWN/UP` | 207-210 | `piMouseButton` (MSX mouse, see caveat sec. 8.3) |
| `SDL_MOUSEMOTION` | 211-212 | `piMouseMove` |

`archUpdateEmuDisplay()` (PiMain.c:92) is the frame-ready callback: when the
emulator thread finishes a frame it posts `SDL_USEREVENT`
(`EVENT_UPDATE_DISPLAY`) onto the SDL queue (coalesced by
`pendingDisplayEvents`), waking the main loop which then renders.

## 5. Threads

| Thread | Created by | Job |
|--------|-----------|-----|
| main | `main()` | SDL event loop, rendering, LED update |
| arch timer | `emulatorStart` -> `archCreateTimer(10 ms)` (Sdl/SdlTimer) | `timerCallback` (sec. 6) |
| emulator | `emulatorStart` -> `archThreadCreate(emulatorThread, THREAD_PRIO_HIGH)` (Sdl/SdlThread) | Z80 + VDP + I/O (`boardRun`) |
| audio | SDL audio device | `soundCallback` -> `mixerWrite` |
| evdev kbd | `piKeyboardEvdevInit` (pthread) | reads `/dev/input/event*`, pushes SDL_KEYDOWN/UP (sec. 8) |
| udev | `piInitUdev` (libudev monitor) | media hotplug |

## 6. Emulator core (Src/Emulator/Emulator.c)

State machine (`emulatorGetState`/`emulatorSetState`, L232-258):
`EMU_RUNNING / EMU_PAUSED / EMU_SUSPENDED / EMU_STEP / EMU_STEP_BACK /
EMU_STOPPED`. Step-back is implemented by the board's snapshot/rewind
(`boardRewindOne`).

Key functions:

- `emulatorInit` (L219) - stores properties + mixer.
- `emulatorGetSyncPeriod` (L261-268) - sync period: **10 ms** when
  `NO_HIRES_TIMERS` (this build), else 2 ms for SYNCAUTO/SYNCNONE, 1 ms for
  SYNCTOVBLANK.
- `timerCallback` (L271-313) - runs in the arch timer thread every 10 ms:
  - tracks `frameCount` against the board refresh rate
    (`boardGetRefreshRate`, 50 Hz PAL default)
  - `framePeriod = (frameSkip + 1) * 1000` ms - frame skipping
  - when due + `EMU_RUNNING`: posts `archUpdateEmuDisplay(0)` (auto sync) or
    `archUpdateEmuDisplay(syncMethod)` (sync-to-vblank async)
  - sets `emuSyncEvent` -> wakes the emulator thread
- `emulatorThread` (L378-416) - the emulator thread entry:
  - `emulatorSetFrequency` (L558-566): Z80 clock = 3 579 545 Hz * 2^((speed-50)/15.0515)
  - `boardRun(machine, &deviceInfo, mixer, stateName, frequency, reversePeriod,
    reverseBufferCnt, WaitForSync)` - the core loop: emulates Z80 + VDP + I/O in
    small quanta, calling `WaitForSync` periodically
- `WaitForSync` (non-WII variant, L718+) - real-time pacing:
  - reverse play: `WaitReverse` (L694) - waits 50 ms, `boardRewind()`
  - `syncPeriod = emulatorGetSyncPeriod()`, `li1 = archGetHiresTimer()`
  - single-step / breakpoint -> pause, suspend sound+MIDI
  - not running -> `archEventSet(emuStartEvent)` (lets `emulatorStart` proceed),
    `emuSysTime = 0`
  - `archPollInput()` every other call
  - SYNCTOVBLANK: `emulatorSyncScreen()` overflow accounting + wait on
    `emuSyncEvent`
  - otherwise: wait on `emuSyncEvent` (the 10 ms tick) until at least
    `syncPeriod` has elapsed since `li1` - this is what keeps emulation at
    real-time speed
- `emulatorStart` (L419-511):
  - `machineCreate` (L438), `boardSetMachine` (L448)
  - `emuSyncEvent`/`emuStartEvent` = `archEventCreate` (L452-454)
  - `emuTimer = archCreateTimer(10, timerCallback)` (L457)
  - `inputEventReset`, `archSoundResume`, state = `EMU_PAUSED` (L463-468)
  - `archThreadCreate(emulatorThread, THREAD_PRIO_HIGH)` (L487)
  - `archEventWait(emuStartEvent, 3000)` (L490) - blocks until the emulator
    thread signals its first sync point (or 3 s timeout)
  - on success: `boardSetYm2413Oversampling`, `boardSetY8950Oversampling`,
    `boardSetMoonsoundOversampling`; copies machine name into properties;
    `debuggerNotifyEmulatorStart`; state = `EMU_RUNNING` (L493-509)
- `emulatorStop` (L513-554): state = `EMU_STOPPED`, wait for suspend flag,
  `emuExitFlag = 1`, `archSoundSuspend`, `archThreadJoin`, `machineDestroy`.
- `emulatorSuspend`/`emulatorResume` (L568-588)
- `emulatorRestart` (L595): stop + recreate machine (used by soft/hard reset)
- `emulatorRestartSound` (L605): suspend -> `archSoundDestroy` ->
  `archSoundCreate(mixer, 44100, bufSize, channels)` -> resume
- `emulatorSyncScreen` (L653): frame-skip counter -> `archUpdateEmuDisplay`
- `RefreshScreen` (L667): VBLANK hook from the video code; in SYNCFRAMES mode
  calls `emulatorSyncScreen`

## 7. Video path (Src/Pi/PiVideo.c, 936 lines)

Replaces the old RISC OS/DispmanX path. Stack: DRM (KMS) -> GBM -> EGL ->
OpenGL ES 2.0 -> GBM front buffer.

### 7.1 piInitVideo() (L393)

1. Open `/dev/dri/card0` (fallback `card1`) - L399-411
2. `getDisplay` (L276): `drmModeGetResources` -> `getConnector` (L252, first
   connected) -> `findEncoder` (L267) -> `drmModeSetCrtc` to program the
   connector with the native mode; create the GBM device + surface
   (`DRM_FORMAT_XRGB8888`)
3. `eglInitialize` (L426), `eglBindAPI(EGL_OPENGL_ES_API)` (L436)
4. `eglChooseConfig` (L445); `matchConfigToVisual` (L316) picks the config
   matching `GBM_FORMAT_XRGB8888` (L456)
5. `eglCreateContext` (L467) with `EGL_CONTEXT_CLIENT_VERSION = 2` (L390)
6. `eglCreateWindowSurface(display, config, gbmSurface, {EGL_RENDER_BUFFER,
   EGL_BACK_BUFFER})` (L479-485) - GBM surface, not a window
7. `eglMakeCurrent` (L499), `glViewport(0,0,hdisplay,vdisplay)` (L508)
8. Build the GLES2 shader program (vertex/fragment, `#version 100`), VBOs,
   RGB565 texture, and the `msxScreen` intermediate buffer

### 7.2 piUpdateEmuDisplay() (L741) - called once per presented frame

1. `glClear`; if `properties->video.force4x3ratio` -> letterbox via
   `glViewport` offset (L750-753)
2. `frameBufferFlipViewFrame(syncMethod == P_EMU_SYNCTOVBLANKASYNC)` (L761) -
   the VDP's front/back frame buffer flip; `NULL` -> white-noise test pattern
   (L763)
3. `videoRender(video, frameBuffer, BIT_DEPTH, 1, msxScreen, 0, msxScreenPitch*2, -1)`
   (L765) - `Src/VideoRender/VideoRender.c`: converts the VDP framebuffer to an
   RGB565 image with scanline/interlace/hstretch/vstretch processing
4. `glTexSubImage2D` (L790) uploads the RGB565 region into the GL texture
5. On resolution change: recompute projection matrix (`setOrtho`, L792-809)
6. `drawQuad` (L810) - full-screen textured quad through the shader
7. `gbmSwapBuffers` (L332, called L817) - flips the GBM front buffer -> KMS
   scanout (replaces the old `eglSwapBuffers`)

`piDestroyVideo` (L708) tears down in reverse.

## 8. Input

### 8.1 Keyboard - evdev bridge (Src/Pi/PiInput.c)

The physical keyboard is shared with the Linux console (tty). Since this
build never initializes `SDL_INIT_VIDEO`, SDL itself cannot deliver keyboard
events, so a dedicated bridge thread is used:

- `piKeyboardEvdevInit` (L547): scans `/dev/input/event*`, keeps devices
  advertising `EV_KEY`, and **grabs them exclusively** with `EVIOCGRAB`
  (L581) - while grabbed, the kernel delivers those events only to this
  process, so keystrokes no longer reach the console (no echo, no shell
  input, no Ctrl+C -> SIGINT). The grab auto-releases when the fd is closed.
- `evdevThreadMain` (L495): `select()` over all grabbed fds; for each
  `EV_KEY` event:
  - `evdevUpdateModifiers` (L374) tracks shift/ctrl/alt/win in
    `evdevModState` (KMOD_*)
  - `evdevKeyToScancode` (L264) / `evdevScancodeToKey` (L389) map Linux
    `KEY_*` to/from `SDL_Scancode`
  - posts `SDL_KEYDOWN/UP` with `keysym.scancode`, `keysym.sym`, and the
    tracked mods via `SDL_PushEvent` (L533-540)
- `piKeyboardEvdevDestroy` (L602): stop thread, ungrab, close.

Downstream, `handleEvent` (PiMain.c:195) routes each key through:

- `keyboardUpdate` (PiInput.c:241): scancode -> MSX keyboard bit via
  `kbdTable` (EU layout; ColecoVision variant at L229-239)
- `shortcutCheckDown/Up` (PiShortcuts.c): F-key commands (sec. 8.2)

### 8.2 Shortcuts (Src/Pi/PiShortcuts.c)

`ShortcutHotkey` (L565-569): `{ unsigned type:8; unsigned mods:8; long key:8; }`
- 32-bit little-endian in the ini file. `key` holds a **Windows virtual-key
code** (not an SDLK) for keyboard entries; `toSDLhotkey` (L668) converts
`KBD_*` mods -> `KMOD_*` and VK -> SDL key via the `sdlkeys[256]` table
(entries 0x03 and 0x90+ are dead - no mapping).

`struct Shortcuts` (L614-661) - every assignable command: quit, fdcTiming,
spritesEnable, switchMsxAudio, switchFront, switchPause, captureAudio,
captureScreenshot, cpuStateQuickLoad/Save, cartRemove[2], cartAutoReset,
diskChange[0]/diskRemove[2]/diskAutoReset, cas* (rewind/remove/readonly/
autoRewind/save), emulationRunPause/Stop, emuSpeed* (full/normal/inc/dec/
toggle), resetSoft/resetHard/resetClean, volume* (increase/decrease/mute/
stereo), windowSize* (normal/fullscreen/toggle), scanlinesToggle,
aspectRatioToggle.

`shortcutsCreate` (L794): reads `bluemsx.ini` `[Shortcuts]` entries
(`LOAD_SHORTCUT` macro, L664), falling back to the hardcoded defaults
(L581-610):

| Command | Default |
|---------|---------|
| quit | F12 |
| no frame skip | F11 |
| frame skip 1 / 2 / 3 | F10 / F9 / F8 (bodies currently disabled, L900-908) |
| soft reset / hard reset | F7 (both map to `resetHard` = F7) |
| screenshot | F6 |
| toggle scanlines | Alt+F10 |
| toggle aspect ratio | Ctrl+F10 |

All other `struct Shortcuts` members are ini-only (no hardcoded default).
`shortcutCheckDown/Up` compares `HOTKEY_EQ` (L666, 32-bit compare) and calls
the matching `action*` (Src/Emulator/Actions.c).

### 8.3 Joysticks & mouse

- Joysticks: SDL2 joystick subsystem (initialized in `main`, PiMain.c:283);
  up to 2 devices (PiMain.c:317-319). Port assignment in
  `joystickPortUpdate` (PiInput.c:195-227): joysticks take ports 0/1 first;
  if an SDL mouse is detected it takes the remaining port
  (`JOYSTICK_PORT_MOUSE`).
- Mouse: `Src/Pi/PiMouse.c` - `piMouseInit` (L173), `piMouseButton` (L181),
  `piMouseMove` (L198), `archMouseGetState` (L226), `archMouseEmuEnable`
  (L256); capture rect via `piMouseSetCaptureRect` (L153), set on first
  frame (PiMain.c:165-168).
- **Caveat:** the evdev bridge forwards `EV_KEY` only (PiInput.c:529 skips
  non-key events). In this headless build SDL has no video driver, so SDL
  mouse events are not delivered - a USB mouse is not usable; joystick
  emulation (MSX mouse dongle) remains the practical mouse path.

## 9. Audio (Src/Sdl/SdlSound.c, Src/SoundChips/)

- `archSoundCreate` (L132): `SDL_OpenAudioDevice` (L59) with the mixer's
  sample rate (44100) / buffer size / channels; callback `soundCallback`
  (L75) -> `soundWrite` (L89) -> `mixerWrite`.
- `archSoundDestroy/Resume/Suspend` (L202/211/216).
- `Src/SoundChips/AudioMixer.c` mixes the channels:
  - `AY8910/` - PSG (AY-8910 / MSM523)
  - `MameYM2151/` - FM (YM2151)
  - `Fmopl/` + `MameVLM5030/` - V9938 FM
  - `Moonsound/` - Moonsound cartridge
  - `DAC/`, `KeyClick/`
- Oversampling is enabled at start (`emulatorStart`, Emulator.c:493-500).
- Volume/pan/mute per channel from `bluemsx.ini` (PiMain.c:358-367).

## 10. I/O devices & GPIO

- **Slot bus**: `Src/IoDevice/MsxBus.cpp` - the MSX expansion (RPMC) board.
  `readMemory` sets the slot-busy flag used by the LEDs. Board docs:
  `Doc/RPMC.md`. Legacy 74HC595 `frontled()` driver in `MsxBusPi.c` is dead
  code under `RPMc_FRONTLED`.
- **Front-panel LEDs**: `Src/Pi/PiGpio.c` - 74HC595 on GPIO
  (SRCLK=22, RCLK=23, SER=26, bcm2835).
  - `gpioInit` (L24): pin setup, initial PWR byte (0x80)
  - `gpioUpdateLeds` (L42): computes I/O byte (SLT1|SLT2 from `ledSlotBusy`),
    shifts only on change - called every presented frame from `handleEvent`
    (PiMain.c:173)
  - `gpioShiftLeds` (L52): LSB-first 8-bit shift
  - Non-ARM fallback stubs (L67-69). LEDs: PWR, SLT2, SLT1, I/O, HAND, CAPS
    (see `Doc/RPMC.md`).
- **FDC**: `WD2793` (3.5" DD/HD) and `NEC765` (5.25"/3" SD); `Disk.c`,
  `FdcAudio.c` (FDC audio).
- **SCSI/IDE**: `HarddiskIDE` (SunriseIDE) with `wd33c93` (SCSI), `rtl8019`
  (RTL8019 LAN), `sl811hs`, `ft245`; `ScsiDevice`.
- **Misc**: `I8254` (timer), `I8255`/`MsxPPI`, `I8250`/`I8251`, `UartIO`,
  `PrinterIO`, `MidiIO`/`MSXMidi`, `JoystickIO`, `LED`, `Switches`, `RTC`,
  `Microchip24x00`/`Microwire93Cx6` (EEPROM), `TC8566AF`, `TurboRIO`,
  `GameReader` (cassette), `Casette`.
- **Hotplug**: `Src/Pi/PiUdev.c` - libudev monitor; `piScanDevices`
  (PiMain.c:472) inserts media found at boot; runtime insert/remove of
  disks/cassettes.

## 11. Media & ROMs

- `Src/Media/MediaDb.cpp` - ROM/disk/cassette type detection (MediaDb).
- `Src/Memory/` - RAM (`MsxRAM`), ROM loading (`RomLoader`), slot tree
  (`SlotManager`, `DeviceManager`), and ~40 ROM mappers (`romMapper*.c`) -
  these are C++ classes; the aarch64 port required real constructor
  implementations (see `CHANGES.md` sec. 1).
- `Src/Utils/ZipFromMem` + `PacketFileSystem` - media packs.
- User media lives in `Media/` (cartridges, diskettes, cassettes, harddisks);
  machine definitions in `Machines/<name>/config.ini`.

## 12. Configuration

- `bluemsx.ini` (current directory) - parsed by `IniFileParser`
  (`Src/Utils/IniFileParser.c`), stored in `Properties`
  (`Src/Emulator/Properties.c`). Sections: `[emulation]` (machine, speed,
  syncMethod, frameSkip, reverseEnable), `[video]` (scanLinesEnable,
  force4x3ratio, saturation, ...), per-channel `[audio]` volume/pan/mute,
  `[joy1]/[joy2]`, `[printer]/[uart]/[midi]`, `[Shortcuts]`, `[media]`
  (cartridges/diskettes/cassettes to insert at boot).
- `setDefaultPaths` (PiMain.c:217-257) creates the runtime directories in CWD:
  `Audio Capture`, `Video Capture`, `QuickSave`, `SRAM`, `Casinfo`,
  `Databases`, `Shortcut Profiles`, `Machines`, `Media`.
- Machine definition: `Machines/MSX2+/config.ini` (slots, RAM, VDP type,
  cassette). Default machine for this build.
- **Logging** (`Src/Utils/Log.{h,cpp}`, spdlog): the emulator is **silent by
  default** (level `warn` — only real errors reach stderr). Level resolution,
  first match wins:
  1. env `BLUEMSX_LOG_LEVEL` (`trace|debug|info|warn|error|critical|off`),
  2. a `settings.logLevel=<level>` line in `bluemsx.ini` (`[config]`
     section, read as a plain text scan — not via `IniFileParser`),
  3. default `warn`.
  `logInit()` (called first thing in `main()`) applies the config; every
  startup print is a `LOG_*` macro (so startup is output-free when the level
  is `warn`). `LOG_INFO` → `info`, `LOG_DEBUG`/`LOG_TRACE` → `debug`/`trace`.
  Under `ROM_TESTER_BUILD` the macros degrade to plain `printf`/`fprintf`, so
  `rom_tester` keeps its console output with no spdlog dependency.

## 13. Key-file quick index

| Area | Files |
|------|-------|
| Entry point, main loop | `Src/Pi/PiMain.c` |
| Video (DRM/GBM/EGL) | `Src/Pi/PiVideo.c` |
| Keyboard evdev bridge | `Src/Pi/PiInput.c` |
| F-key shortcuts | `Src/Pi/PiShortcuts.c` |
| Mouse | `Src/Pi/PiMouse.c` |
| Front-panel LEDs (GPIO) | `Src/Pi/PiGpio.c` |
| Media hotplug (udev) | `Src/Pi/PiUdev.c` |
| Arch-layer glue (Pi) | `Src/Pi/PiEvent.c`, `Src/Pi/stubs.c` |
| SDL audio/timer/thread | `Src/Sdl/SdlSound.c`, `Src/Sdl/SdlTimer.c`, `Src/Sdl/SdlThread.c` |
| Emulator core | `Src/Emulator/Emulator.c` |
| Actions (what shortcuts call) | `Src/Emulator/Actions.c` |
| Properties / INI | `Src/Emulator/Properties.c`, `Src/Utils/IniFileParser.c` |
| MSX board | `Src/Board/Board.c`, `Src/Board/Machine.c`, `Src/Board/MSX.c` |
| CPU | `Src/Z80/R800.c` |
| VDP | `Src/VideoChips/CRTC6845.c`, `VDP.c`, `FrameBuffer.c`, `VideoManager.c` |
| Frame post-processing | `Src/VideoRender/VideoRender.c` |
| Slot bus (RPMC) | `Src/IoDevice/MsxBus.cpp`, `Doc/RPMC.md` |
| Logging (spdlog) | `Src/Utils/Log.h`, `Src/Utils/Log.cpp` |
| Build | `Makefile` |

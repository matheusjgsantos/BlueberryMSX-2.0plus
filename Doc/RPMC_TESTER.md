# rom_tester — standalone cartridge dumper utility

Standalone ROM dumper for the **RPMC** (Raspberry Pi MSX Clone) board used by
BlueberryMSX 2.0 Plus. This utility reads physical MSX cartridges directly from
the board's GPIO bus without launching the full emulator — useful for quick ROM
extraction, cartridge verification, and I/O port inspection.

- Standalone source: `rom_tester.c`
- Low-level GPIO access: `Src/IoDevice/MsxBusPi.c` (compiled under
  `#define ROM_TESTER_BUILD`)
- Build target: `Makefile` (`rom_tester` target)

---

## 1. Overview

`rom_tester` is a minimal program that:

1. Initializes the RPMC GPIO bus via `bcm2835` (same path as the live emulator).
2. Reads raw cartridge bytes from the selected slot at the specified offset and
   size.
3. Optionally auto-probes sub-slots to find where ROM data is present.
4. Writes the raw bytes to a file or to stdout.
5. Supports an `--io` mode to dump all 256 I/O port values.

Because it does not link the full emulator, it starts faster and uses far less
memory than launching `bluemsx-pi` solely to read a cartridge.

---

## 2. Build

From the BlueberryMSX 2.0 Plus source root:

```
make rom_tester
```

This compiles `rom_tester.c` with `-DROM_TESTER_BUILD`, includes
`Src/IoDevice/` and `Src/Utils/` (for the shared `Log.h`), and links against
`-lbcm2835 -lpthread`. The binary is placed in the source root as
`./rom_tester`.

`rom_tester` deliberately does **not** use the emulator's spdlog logging:
under `ROM_TESTER_BUILD` the `LOG_*` macros in `Src/Utils/Log.h` degrade to
plain `printf`/`fprintf`, so the tool always prints to the console regardless
of the `settings.logLevel` / `BLUEMSX_LOG_LEVEL` settings that apply to
`bluemsx-pi`.

---

## 3. Usage

```
Usage: ./rom_tester [OPTIONS]

Options:
  -f FILE    Write ROM dump to FILE (default: stdout only)
  -o OFFSET  Memory offset to start reading (default: 0x4000)
  -s SIZE    Number of bytes to dump (default: 0x8000 = 32 KB)
  -S SLOT    MSX parent slot (0 or 1, default: 0)
             Slot 0 -> board connector 1 (SLTSL1 / MSX slot 1)
             Slot 1 -> board connector 2 (SLTSL3 / MSX slot 3)
  --io       Dump all 256 I/O port values instead of ROM
  -h, --help Show this help message
```

### Examples

Dump slot 0 ROM (board connector 1) to file:
```
sudo ./rom_tester -f game.rom
```

Dump slot 1 ROM (board connector 2):
```
sudo ./rom_tester -f upper.rom -S 1
```

Dump only the first 16 KB of a cartridge:
```
sudo ./rom_tester -f small.rom -s 0x4000
```

Read and display all I/O port values:
```
sudo ./rom_tester --io
```

---

## 4. Slot mapping

| `-S` flag | Board connector | MSX select line | emulator slot |
|-----------|-----------------|-----------------|---------------|
| 0 (default) | Connector 1 | SLTSL1 (slot 1) | slot 0 |
| 1 | Connector 2 | SLTSL3 (slot 3) | slot 1 |

This matches the slot wiring used by `bluemsx-pi` (see `Doc/RPMC.md` §1).

---

## 5. Privileges

The utility accesses `/dev/mem` directly via `bcm2835_init()`. Run as root or
with the user in the `gpio` group. If `bcm2835_init()` fails, the program prints
an error and exits (unlike the emulator, which can continue without LEDs in
this case).

---

## 6. Sub-slot auto-probing

If the cartridge uses split sub-slots, `rom_tester` will auto-probe the four
sub-slot regions (0x0000–0x3FFF, 0x4000–0x7FFF, 0x8000–0xBFFF, 0xC000–0xDFFF)
to detect which contain actual ROM data, and report the result before dumping.

---

## 7. Troubleshooting

- **bcm2835_init failed** — confirm you're running as root or that `/dev/mem`
  is accessible. The `bcm2835` library must be installed (`sudo apt install
  libbcm2835-dev` or built from source per the README).
- **All bytes read as 0xFF** — no cartridge detected in the slot. Check that a
  cartridge is inserted and that the slot connector has solid contact.
- **Unexpected bytes at high addresses** — the cartridge may have bank-switching
  logic. `rom_tester` performs a linear read only; bank-switched cartridges
  require the full emulator to cycle through bank states.

---

## 8. References

| Item | Location |
|------|----------|
| Standalone dumper source | `rom_tester.c` |
| GPIO hardware access | `Src/IoDevice/MsxBusPi.c` |
| Build target | `Makefile` (target `rom_tester`) |
| Slot wiring details | `Doc/RPMC.md` §2 |
| bcm2835 library | `bcm2835-1.68/` (installed to `/usr/local/lib`) |

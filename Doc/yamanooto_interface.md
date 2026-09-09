# Yamanooto / RPMC K5 Interface Findings

## Overview
This document describes the current state and diagnostic findings for the Yamanooto (RPMC) K5 mapper interface on the BlueberryMSX-2.0Plus GPIO slot board.

## Status
- **Reads**: Fully Functional. The FPGA registers and flash memory (via bank windows) are read correctly.
- **Writes**: Non-functional. Bank switching and register configuration (ENAR, CFGR, OFFR) do not take effect.

## Findings

### 1. Power-on State
Verification using known markers in `B-MASTER.EXT` confirms that the hardware starts with the bank registers in the default `iota` state:
`bankRegs = {0, 1, 2, 3}`. This matches the openMSX implementation.

### 2. Software Bug: Latch Timing (`MsxBusPi.c`)
A bug was identified in the `SetData` function of `MsxBusPi.c`. The function was clearing the Latch Enable signal `LE_C` (GPIO17) before asserting `MSX_WR`. Since U4 is a 74LS373 transparent latch, clearing `LE_C` freezes the output, effectively blocking the `/WR` pulse from reaching the cartridge slot.

### 3. Hardware Fault Diagnosis
To verify if the issue was purely software, a decisive probe (`wrtest11`) was implemented using a corrected write protocol that keeps `LE_C` HIGH throughout the write cycle.

**Test Results:**
- **ENAR Write**: Writing `0x01` (REGEN=1) to `0x7FFFh` failed. Readback returned `0x00`.
- **Bank Switching**: Writes to `$9000` and `$5000` did not change the memory windows.
- **Wait States**: All write attempts returned `WAIT=0`, indicating the FPGA did not acknowledge the write request.

### Conclusion
Since the software protocol was corrected and writes still failed to reach the FPGA, the failure is attributed to a **physical hardware fault on the `/WR` signal path**. 

**Likely cause**: A broken trace or cold solder joint between **U4 pin 15** (output) and **P2 connector pin 13** (/WR).

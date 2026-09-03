# BlueberryMSX-2.0plus Improvements Implementation

This file documents the improvements made to enhance the BlueberryMSX-2.0plus fork compared to your version, with focus on:

## Key Updates Implemented

### 1. GPIO Implementation (MsxBusPi.c)
- Replaced legacy bcm2835 library approach with modern GPIO v2 using `/dev/gpiomem0` 
- Added support for proper chip detection and memory mapping
- Implemented RP1 clock management for the 3.579545 MHz MSX clock generator
- Enhanced error handling for GPIO operations
- Maintained existing functionality while improving hardware interaction

### 2. Keyboard/Shortcut Improvements (PiShortcuts.c)
- Corrected core shortcut mappings:
  - Reset Hard (F6) and Screenshot (F7) key assignments swapped to match RGR version
  - More robust keyboard handling with improved hotkey matching using `hotkeyMatches()`
- Expanded disk change array support from 0 to 2 elements
- Added better debugging output for shortcut actions
- Enhanced function implementation with more complete logic coverage

## Integration Notes

The changes are designed to work alongside your existing DRM/KMS/EGL/GLES video libraries without conflicts, since both approaches utilize the same underlying hardware interfaces. The GPIO approach is purely an improvement over the previous implementation while maintaining compatibility.

## Files Changed
1. Src/IoDevice/MsxBusPi.c - GPIO system improvements
2. Src/Pi/PiShortcuts.c - Keyboard and shortcut handling enhancements

These changes have been implemented in a new branch 'newq' for review before merging.
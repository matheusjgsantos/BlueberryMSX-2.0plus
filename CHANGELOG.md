# BlueberryMSX-2.0plus Improvements Changelog

## Version 2.1.0 - GPIO v2 and Shortcut Enhancements

This release introduces significant improvements to the GPIO implementation and keyboard shortcut handling, enhancing compatibility and functionality while maintaining full backward compatibility with existing configurations.

### Key Improvements

#### 1. Modernized GPIO Implementation (MsxBusPi.c)
- **Replaced legacy bcm2835 library** approach with modern GPIO v2 using `/dev/gpiomem0`
- **Added proper chip detection and memory mapping** for improved hardware compatibility  
- **Implemented RP1 clock management** for the 3.579545 MHz MSX clock generator
- **Enhanced error handling** while maintaining existing hardware functionality
- **Optimized GPIO operations** for better performance

#### 2. Enhanced Keyboard/Shortcut Handling (PiShortcuts.c)
- **Fixed core shortcut key mappings** - Reset Hard (F6) and Screenshot (F7) swapped to correct positions
- **Added robust `hotkeyMatches()` function** instead of simple equality checks
- **Expanded disk change array support** from 0 to 2 elements for better compatibility
- **Improved debugging output** and more complete logic coverage

### Backward Compatibility
All changes are fully backward compatible with existing DRM/KMS/EGL/GLES video libraries. Both implementations utilize the same underlying hardware interfaces, with the GPIO v2 approach being purely an enhancement over previous implementation.

### Files Modified
1. Src/IoDevice/MsxBusPi.c - GPIO system improvements
2. Src/Pi/PiShortcuts.c - Keyboard and shortcut handling enhancements

### Implementation Verification
The implementation has been successfully completed and tested with:
- Proper compilation without legacy bcm2835 library errors
- Fixed keyboard shortcut mappings (Reset Hard F6, Screenshot F7)  
- Expanded disk change support (2 elements instead of 0)
- Full GPIO v2 implementation using `/dev/gpiomem0` directly
- Maintained compatibility with existing video libraries

The improvements provide modernized GPIO access and enhanced keyboard handling while maintaining full compatibility with your existing hardware configuration.
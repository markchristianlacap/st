# Sixel Graphics Support for st

This implementation adds sixel graphics protocol support to st (simple terminal).
Sixel is a bitmap graphics format that allows inline image display in terminal emulators.

## What is Sixel?

Sixel (Six Pixels) is a bitmap graphics format introduced by DEC terminals.
It enables applications like nvim, yazi, and others to display inline graphics.

## Implementation Details

### Protocol Support

The implementation supports the DCS (Device Control String) sixel sequences:
- Format: `ESC P <parameters> q <sixel data> ESC \`
- Color palette definition and RGB colors
- Sixel repeat sequences
- Carriage return and line feed controls

### Features

- Full sixel data parser
- Color palette with 256 color support
- RGB color definition
- Image rendering integrated with X11 drawing
- Multiple images can be displayed simultaneously

### Limitations

- Images are stored in memory and rendered on each draw call
- No automatic image cleanup on scroll (images persist)
- Images use cell-based positioning
- Transparency is basic (alpha channel support)

## Usage

Applications that support sixel output (like nvim with image plugins, yazi file manager,
etc.) will automatically work with this terminal.

To test sixel support manually, use the test script:
```bash
./test_sixel.sh
```

## Technical Architecture

### Files Modified/Added

1. **sixel.h** - Header file with data structures and function prototypes
2. **sixel.c** - Sixel parser and image storage implementation
3. **st.c** - Integration with DCS sequence handling
4. **x.c** - X11 rendering implementation
5. **Makefile** - Build system updates

### Key Components

- `SixelState`: Parser state machine for processing sixel data
- `ImageList`: Linked list of images with position and data
- `sixel_parse_dcs()`: Main parser for sixel sequences
- `xfinishdraw()`: Modified to render sixel images with X11

### Color Format

Images are stored in RGBA format (4 bytes per pixel):
- R: Red (0-255)
- G: Green (0-255)  
- B: Blue (0-255)
- A: Alpha (0-255)

X11 rendering converts to appropriate pixel format for the display.

## Building

The sixel support is automatically included when building st:

```bash
make clean
make
```

## Future Enhancements

Potential improvements:
- Image cleanup on scroll
- Image reflow on terminal resize
- Kitty graphics protocol support
- iTerm2 inline images protocol
- Memory optimization for large images
- Image caching and compression

## References

- [Sixel Graphics Protocol](https://www.vt100.net/docs/vt3xx-gp/chapter14.html)
- [DEC VT340 Programmer Reference](https://vt100.net/docs/vt3xx-gp/)

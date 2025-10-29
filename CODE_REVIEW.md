# Code Review: st - Simple Terminal

## Overview
This is a comprehensive code review of the st (simple terminal) codebase. St is a terminal emulator for X that follows the suckless philosophy. The codebase consists of approximately 5,400 lines of C code across 4 main source files.

## Positive Aspects

### 1. **Clean Architecture**
- Well-separated concerns with distinct modules:
  - `st.c` - Terminal emulation logic (~2,781 lines)
  - `x.c` - X11 windowing and rendering (~2,340 lines)
  - `boxdraw.c` - Box drawing characters (~194 lines)
  - `hb.c` - HarfBuzz text shaping integration (~125 lines)
- Clear header files defining interfaces (`st.h`, `win.h`)

### 2. **Good Code Organization**
- Consistent use of static functions for internal implementation
- Clear separation of public API and private implementation
- Well-organized data structures (Term, Selection, Glyph, etc.)

### 3. **Consistent Coding Style**
- K&R/OpenBSD style formatting throughout
- Consistent naming conventions
- Good use of enums for constants instead of magic numbers

### 4. **Memory Management**
- Custom wrapper functions (`xmalloc`, `xrealloc`, `xstrdup`) that die on allocation failure
- Clear ownership semantics in most places

## Areas for Improvement

### 1. **Missing Build Artifacts in Version Control**

**Issue:** Build artifacts (`.o` files, compiled `st` binary) were committed to the repository.

**Recommendation:** Create a `.gitignore` file to exclude build artifacts:
```gitignore
# Build artifacts
*.o
st
st-*.tar.gz

# Editor backups
*~
*.swp
*.swo
.*.swp

# OS files
.DS_Store
Thumbs.db
```

**Priority:** High
**Effort:** Minimal

---

### 2. **Error Handling Improvements**

**Issue:** Some functions use `die()` which immediately exits, making the code less testable and harder to use as a library.

**Examples:**
```c
// st.c:264-265
if (!(p = malloc(len)))
    die("malloc: %s\n", strerror(errno));
```

**Recommendation:** 
- Consider returning error codes or NULL for library-style functions
- Reserve `die()` for truly unrecoverable errors in the main application
- For a suckless terminal this may be acceptable, but consider whether you want any library-style reusability

**Priority:** Low (depends on design goals)
**Effort:** Medium

---

### 3. **Magic Numbers in Macros**

**Issue:** Some macros have embedded constants that could be more descriptive.

**Example:**
```c
// st.c:46-48
#define TLINE(y)  ((y) < term.scr ? term.hist[((y) + term.histi - \
          term.scr + HISTSIZE + 1) % HISTSIZE] : \
          term.line[(y) - term.scr])
```

**Recommendation:** Break complex macros into inline functions for better debugging and type safety:
```c
static inline Line
tline(int y)
{
    if (y < term.scr) {
        int idx = ((y) + term.histi - term.scr + HISTSIZE + 1) % HISTSIZE;
        return term.hist[idx];
    }
    return term.line[y - term.scr];
}
```

**Priority:** Medium
**Effort:** Low to Medium

---

### 4. **Incomplete TODO/FIXME Items**

**Issue:** There are numerous TODO and FIXME comments throughout the code (at least 20 in st.c alone).

**Examples:**
```c
// st.c:636 - FIXME: Fix the computer world.
// st.c:892 - FIXME: Migrate the world to Plan 9.
// st.c:2034 - TODO if defaultbg color is changed, borders are dirty
// st.c:2319-2349 - Multiple TODO items for unimplemented control codes
```

**Recommendation:**
- Review each TODO/FIXME and determine if it's still relevant
- Create issues/tickets for actionable items
- Remove humorous but non-actionable items (like "Fix the computer world")
- Implement or document why unimplemented control codes are not needed

**Priority:** Medium
**Effort:** High (requires research and decisions)

---

### 5. **Documentation Improvements**

**Issue:** Limited inline documentation for complex algorithms and data structures.

**Recommendation:**
- Add comprehensive comments to complex functions like `selsnap()`, `drawboxlines()`
- Document the terminal state machine and escape sequence handling
- Add ASCII art diagrams where appropriate (like for box drawing calculations)
- Document assumptions about terminal behavior

**Example improvement:**
```c
/**
 * selsnap - Snap selection to word or line boundaries
 * @x: pointer to x coordinate, will be modified
 * @y: pointer to y coordinate, will be modified
 * @direction: -1 for backward, 1 for forward
 *
 * Adjusts the selection point to the nearest word or line boundary
 * based on sel.snap setting. Handles line wrapping correctly.
 */
void
selsnap(int *x, int *y, int direction)
{
    // ... implementation
}
```

**Priority:** Medium
**Effort:** Medium

---

### 6. **Buffer Safety**

**Issue:** Some buffer operations could be made safer.

**Examples:**
```c
// st.c:600-601 - Buffer size calculation could overflow for large terminals
bufsize = (term.col+1) * (sel.ne.y-sel.nb.y+1) * UTF_SIZ;
ptr = str = xmalloc(bufsize);
```

**Recommendation:**
- Use `calloc()` for zero-initialized allocations
- Add overflow checks for buffer size calculations:
```c
// Check for potential integer overflow
if (term.col > SIZE_MAX / UTF_SIZ / (sel.ne.y - sel.nb.y + 2)) {
    die("selection too large\n");
}
bufsize = (term.col + 1) * (sel.ne.y - sel.nb.y + 1) * UTF_SIZ;
```
- Consider using `strlcpy/strlcat` equivalents or explicit bounds checking

**Priority:** Medium
**Effort:** Low to Medium

---

### 7. **Global State**

**Issue:** Heavy use of global variables makes testing difficult and limits multiple instances.

**Examples:**
```c
// st.c:229-235
static Term term;
static Selection sel;
static CSIEscape csiescseq;
static STREscape strescseq;
static int iofd = 1;
static int cmdfd;
static pid_t pid;
```

**Recommendation:**
- Encapsulate related globals into context structures
- Pass context pointers to functions instead of accessing globals
- This would enable:
  - Multiple terminal instances
  - Better unit testing
  - Clearer data dependencies

**Example refactoring:**
```c
typedef struct {
    Term term;
    Selection sel;
    CSIEscape csiescseq;
    STREscape strescseq;
    int iofd;
    int cmdfd;
    pid_t pid;
} TerminalContext;

// Functions would become:
void tputc_ctx(TerminalContext *ctx, Rune u);
```

**Priority:** Low (architectural change, may conflict with suckless philosophy)
**Effort:** High

---

### 8. **Function Length**

**Issue:** Some functions are very long and handle multiple responsibilities.

**Examples:**
- `strhandle()` in st.c (~170 lines) handles all string escape sequences
- `twrite()` in st.c is complex with nested control flow

**Recommendation:**
- Break large functions into smaller, focused functions
- Each function should have a single clear responsibility
- Example: Split `strhandle()` by escape sequence type

**Priority:** Medium
**Effort:** Medium

---

### 9. **Type Safety**

**Issue:** Some type definitions could be more specific.

**Examples:**
```c
// st.h:57-60
typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;
```

**Recommendation:**
- Consider using standard `<stdint.h>` types where sizes matter:
  - `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- This makes the code more portable and explicit about size requirements
- Keep convenience typedefs for truly generic uses

**Priority:** Low
**Effort:** Low

---

### 10. **Macro Safety**

**Issue:** Some macros don't protect their arguments properly.

**Examples:**
```c
// st.h:7-13
#define MIN(a, b)    ((a) < (b) ? (a) : (b))
#define MAX(a, b)    ((a) < (b) ? (b) : (a))
```

**Issue:** These macros evaluate arguments multiple times, which can cause issues with side effects:
```c
x = MIN(i++, j++);  // i and j may be incremented twice!
```

**Recommendation:**
- Use inline functions instead:
```c
static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a < b ? b : a; }
```
- Or use GNU statement expressions if GCC-only is acceptable:
```c
#define MIN(a, b) ({ \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b; \
})
```

**Priority:** Medium
**Effort:** Low

---

### 11. **HarfBuzz Integration**

**Issue:** The HarfBuzz integration is clean but has some potential improvements.

**Observations in `hb.c`:**
- Empty features array (line 41) - are font features desired?
- Memory allocations in hot paths (`hbtransform` function)
- No error handling for failed allocations in some paths

**Recommendations:**
- Document which font features are supported and why the array is empty
- Consider pre-allocating buffers for common cases
- Add more robust error handling

**Priority:** Low
**Effort:** Low to Medium

---

### 12. **Box Drawing Optimization**

**Issue:** Box drawing code in `boxdraw.c` is well-written but could benefit from comments explaining the coordinate systems.

**Recommendation:**
- Add diagrams explaining the box drawing coordinate system
- Document the meaning of flags (BDB, BDL, BDA, etc.)
- Example header comment:
```c
/**
 * Box drawing coordinate system:
 *
 *     (x, y)                    (x+w, y)
 *        +------------------------+
 *        |                        |
 *        |     center texel:      |
 *        |     (x+w2, y+h2)       |
 *        |                        |
 *        +------------------------+
 *     (x, y+h)                (x+w, y+h)
 *
 * Variables:
 *   w2 = (w - s) / 2  - horizontal offset to center
 *   h2 = (h - s) / 2  - vertical offset to center
 *   s  = stem thickness
 */
```

**Priority:** Low
**Effort:** Low

---

### 13. **Command-Line Argument Parsing**

**Issue:** The custom `arg.h` argument parser is clever but non-standard.

**Observations:**
- Uses macro magic that can be hard to debug
- Not widely known pattern outside suckless community
- Mixing statements and declarations in macro expansion

**Recommendation:**
- Keep it if it aligns with suckless philosophy
- Document it better with examples
- Consider alternatives like `getopt()` if POSIX compatibility is important
- Add comment about why this custom approach is used

**Priority:** Low (style preference)
**Effort:** Low (documentation) to High (replacement)

---

### 14. **Testing Infrastructure**

**Issue:** No visible test suite or testing infrastructure.

**Recommendation:**
- Add basic unit tests for utility functions:
  - UTF-8 encoding/decoding
  - Buffer management
  - Selection logic
- Add integration tests for escape sequence handling
- Consider fuzzing for security
- Testing framework options:
  - Unity (lightweight C testing)
  - Check framework
  - Custom minimal framework (suckless style)

**Example test structure:**
```c
// tests/test_utf8.c
void test_utf8_encode_decode(void) {
    char buf[UTF_SIZ];
    Rune decoded;
    
    size_t len = utf8encode(0x1F4A9, buf);  // 💩
    assert(len == 4);
    
    utf8decode(buf, &decoded, len);
    assert(decoded == 0x1F4A9);
}
```

**Priority:** Medium (for reliability)
**Effort:** High

---

### 15. **Security Considerations**

**Current state:** The code appears generally secure but could benefit from:

**Recommendations:**
1. **Input validation:**
   - Validate all escape sequence parameters
   - Add bounds checking on all array accesses
   - Validate UTF-8 sequences strictly

2. **Memory safety:**
   - Consider using static analysis tools (cppcheck, clang-analyzer)
   - Run with AddressSanitizer during development
   - Review all buffer operations

3. **Privilege separation:**
   - Document security model
   - Ensure proper privilege dropping if running setuid (doesn't appear to)

**Priority:** High
**Effort:** Medium

---

## Code Quality Metrics

### Strengths:
- ✅ Consistent style
- ✅ Modular architecture  
- ✅ Clear separation of concerns
- ✅ Minimal dependencies
- ✅ Efficient rendering logic

### Areas for Improvement:
- ⚠️ Limited documentation
- ⚠️ No test coverage
- ⚠️ Some complex functions
- ⚠️ Many TODO items
- ⚠️ Heavy global state

---

## Recommendations by Priority

### High Priority:
1. ✅ Add `.gitignore` for build artifacts
2. Perform security audit with static analysis
3. Address buffer safety concerns

### Medium Priority:
4. Convert unsafe macros to inline functions
5. Add comprehensive documentation
6. Break up long functions
7. Review and address TODO/FIXME items
8. Add testing infrastructure

### Low Priority:
9. Improve error handling patterns
10. Consider reducing global state
11. Standardize on `<stdint.h>` types
12. Document HarfBuzz features
13. Add box drawing coordinate documentation

---

## Conclusion

The st codebase is well-structured and follows the suckless philosophy effectively. The code is generally clean and maintainable, with consistent style and good modular separation.

**Key Strengths:**
- Clean, readable C code
- Efficient implementation
- Good separation between terminal logic and rendering
- Minimal external dependencies

**Key Opportunities:**
- Better documentation of complex algorithms
- Testing infrastructure
- Address accumulation of TODO items
- Some modern C safety practices

**Overall Assessment:** ⭐⭐⭐⭐☆ (4/5)

The code is production-quality and demonstrates good software engineering practices. The recommendations above would make it even more maintainable and robust, but the current state is quite good. The main improvements would be documentation, testing, and addressing the technical debt represented by TODO items.

---

## Next Steps

If you decide to act on these recommendations:

1. Start with `.gitignore` (5 minutes)
2. Run static analysis tools (1 hour)
3. Document complex functions (ongoing)
4. Convert unsafe macros (2-3 hours)
5. Add unit tests (1-2 weeks for comprehensive coverage)
6. Address TODO items (ongoing, prioritize by impact)

Feel free to ask questions about any specific recommendations or if you'd like code examples for any improvements!

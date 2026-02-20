#ifndef COLOR_H
#define COLOR_H

namespace ansi {
    // These constants provide ANSI escape sequences for terminal color and
    // style control.
    //
    // - Used for diagnostic output and highlighting in the UI layer.
    // - Invariants: Values must remain valid ANSI codes; consumers must reset
    // formatting
    //   after use to avoid leaking styles into unrelated output.
    // - Performance: Inline const char* avoids runtime construction/allocation.
    // - Any changes here may impact downstream consumers relying on specific
    // color codes
    //   (e.g., test harnesses parsing output).
    inline const char* reset  = "\x1b[0m";
    inline const char* bold   = "\x1b[1m";
    inline const char* dim    = "\x1b[2m";
    inline const char* red    = "\x1b[31m";
    inline const char* green  = "\x1b[32m";
    inline const char* yellow = "\x1b[33m";
    inline const char* cyan   = "\x1b[36m";
} // namespace ansi

#endif // COLOR_H
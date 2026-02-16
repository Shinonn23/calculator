#ifndef COLOR_H
#define COLOR_H
namespace ansi {
    inline const char* reset  = "\x1b[0m";
    inline const char* bold   = "\x1b[1m";
    inline const char* dim    = "\x1b[2m";
    inline const char* red    = "\x1b[31m";
    inline const char* green  = "\x1b[32m";
    inline const char* yellow = "\x1b[33m";
    inline const char* cyan   = "\x1b[36m";
} // namespace ansi

#endif // COLOR_H
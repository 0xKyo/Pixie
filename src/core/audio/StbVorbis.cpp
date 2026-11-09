// stb_vorbis gets its own translation unit because it defines one-letter macros that leak.
#define STB_VORBIS_NO_STDIO 1
#ifdef _MSC_VER
#pragma warning(push, 0)        // third-party code: don't report its warnings
#pragma warning(disable : 4701) // this one is emitted during code generation, after push(0) applies
#endif
#include <stb_vorbis.c>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

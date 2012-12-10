#ifdef SPEC
#ifndef IM_SPEC_ALREADY
#define read(a,b,c) spec_read(a,b,c)
#define write(a,b,c) spec_write(a,b,c)

#define ferror(x) 0
#define getc(f) spec_getc(f)
#define ungetc(c,f) spec_ungetc(c,f)
#endif
#endif

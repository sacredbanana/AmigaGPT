/*
 * Dummy start so the linker is happy. This file is a shared library,
 * not a program; running it from a shell must just fail cleanly.
 */
#if defined(__amigaos3__)
asm(".text\n\
     .even\n\
     .globl _start\n\
    _start:\n\
     moveq #20,d0\n\
     rts");
#elif defined(__MORPHOS__)
LONG __start(void)
{
  return 20;
}
#else
LONG _start(void)
{
  return 20;
}
#endif

#include <proto/exec.h>

STATIC const ULONG tricky=0x16c04e75; /* move.b d0,(a3)+ ; rts */

VOID sprintf(STRPTR buffer,STRPTR fmt,...)
{ APTR SysBase = *(APTR *)4L;
  STRPTR *arg = &fmt+1;

  RawDoFmt(fmt,arg,(void (*)())&tricky,buffer);
} /* Not very clean, but ... */

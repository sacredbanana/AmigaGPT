/*
 * BestTextEditor.mcc - library glue
 *
 * mccinit.c turns this into a MUI custom class library for AmigaOS 3,
 * AmigaOS 4 and MorphOS. The class needs datatypes.library (inline images)
 * and diskfont.library (fonts from the preferences); both are optional and
 * opened once here, since one copy of the library serves every program.
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include "SDI_compiler.h"
#include "SDI_lib.h"
#include "SDI_hook.h"
#include "Debug.h"
#include <mui/BestTextEditor_mcc.h>
#include "BestTextEditorPrivate.h"
#include "version.h"

#define VERSION LIB_VERSION
#define REVISION LIB_REVISION
#define CLASS MUIC_BestTextEditor
#define SUPERCLASS MUIC_Area
#define INSTDATA RTEData
#define USERLIBID CLASS " " LIB_REV_STRING " (" LIB_DATE ") " LIB_COPYRIGHT
#define MASTERVERSION 19
#define MIN_STACKSIZE 8192
#define USEDCLASSESP usedClassesP
#define CLASSINIT
#define CLASSEXPUNGE

static const char *const usedClassesP[] = {MUIC_BestTextEditorP, NULL};

BOOL ClassInit(struct Library *base);
VOID ClassExpunge(struct Library *base);

#include "shellstart.c"
#include "mccinit.c"

struct Library *rteDataTypesBase = NULL;
struct Library *DiskfontBase = NULL;
#if defined(__AMIGAOS4__)
struct DataTypesIFace *rteIDataTypes = NULL;
struct DiskfontIFace *IDiskfont = NULL;
#endif

BOOL ClassInit(UNUSED struct Library *base) {
    rteDataTypesBase = OpenLibrary("datatypes.library", 39);
    DiskfontBase = OpenLibrary("diskfont.library", 36);
#if defined(__AMIGAOS4__)
    if (rteDataTypesBase != NULL &&
        (rteIDataTypes = (struct DataTypesIFace *)GetInterface(
             rteDataTypesBase, "main", 1, NULL)) == NULL) {
        CloseLibrary(rteDataTypesBase);
        rteDataTypesBase = NULL;
    }
    if (DiskfontBase != NULL &&
        (IDiskfont = (struct DiskfontIFace *)GetInterface(DiskfontBase, "main",
                                                           1, NULL)) == NULL) {
        CloseLibrary(DiskfontBase);
        DiskfontBase = NULL;
    }
#endif
    return TRUE;
}

VOID ClassExpunge(UNUSED struct Library *base) {
#if defined(__AMIGAOS4__)
    if (IDiskfont != NULL)
        DropInterface((struct Interface *)IDiskfont);
    if (rteIDataTypes != NULL)
        DropInterface((struct Interface *)rteIDataTypes);
    IDiskfont = NULL;
    rteIDataTypes = NULL;
#endif
    if (DiskfontBase != NULL)
        CloseLibrary(DiskfontBase);
    if (rteDataTypesBase != NULL)
        CloseLibrary(rteDataTypesBase);
    DiskfontBase = NULL;
    rteDataTypesBase = NULL;
}

#if defined(__amigaos4__)
#include <exec/emulation.h>
#include <exec/interfaces.h>

#ifdef Open
#undef Open
#endif
#ifdef Close
#undef Close
#endif

STATIC struct Library *stub_OpenPPC(ULONG *regarray) {
    struct Library *Base = (struct Library *)regarray[REG68K_A6 / 4];
    struct ExtendedLibrary *ExtLib =
        (struct ExtendedLibrary *)((ULONG)Base + Base->lib_PosSize);
    struct LibraryManagerInterface *Self =
        (struct LibraryManagerInterface *)ExtLib->ILibrary;

    return Self->Open(0);
}

STATIC APTR stub_ClosePPC(ULONG *regarray) {
    struct Library *Base = (struct Library *)regarray[REG68K_A6 / 4];
    struct ExtendedLibrary *ExtLib =
        (struct ExtendedLibrary *)((ULONG)Base + Base->lib_PosSize);
    struct LibraryManagerInterface *Self =
        (struct LibraryManagerInterface *)ExtLib->ILibrary;

    return Self->Close();
}

STATIC APTR stub_ExpungePPC(ULONG *regarray UNUSED) { return NULL; }

STATIC ULONG stub_ReservedPPC(ULONG *regarray UNUSED) { return 0UL; }

STATIC ULONG stub_MCC_QueryPPC(ULONG *regarray) {
    return (ULONG)MCC_Query(NULL, (LONG)regarray[0]);
}

STATIC CONST struct EmuTrap stub_Open = {
    TRAPINST, TRAPTYPE, (ULONG(*)(ULONG *))stub_OpenPPC};
STATIC CONST struct EmuTrap stub_Close = {
    TRAPINST, TRAPTYPE, (ULONG(*)(ULONG *))stub_ClosePPC};
STATIC CONST struct EmuTrap stub_Expunge = {
    TRAPINST, TRAPTYPE, (ULONG(*)(ULONG *))stub_ExpungePPC};
STATIC CONST struct EmuTrap stub_Reserved = {TRAPINST, TRAPTYPE,
                                             stub_ReservedPPC};
STATIC CONST struct EmuTrap stub_MCC_Query = {
    TRAPINST, TRAPTYPE, (ULONG(*)(ULONG *))stub_MCC_QueryPPC};

CONST APTR VecTable68K[] = {(APTR)&stub_Open,     (APTR)&stub_Close,
                            (APTR)&stub_Expunge,  (APTR)&stub_Reserved,
                            (APTR)&stub_MCC_Query, (APTR)-1};
#endif

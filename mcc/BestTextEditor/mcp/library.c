/*
 * BestTextEditor.mcp - library glue
 *
 * The preferences class shown on the BestTextEditor page of MUI
 * preferences. Its texts come from BestTextEditor_mcp.catalog.
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/locale.h>
#include "SDI_compiler.h"
#include "SDI_lib.h"
#include "SDI_hook.h"
#include "Debug.h"
#include <mui/BestTextEditor_mcc.h>
#include "private.h"
#include "version.h"

#define VERSION LIB_VERSION
#define REVISION LIB_REVISION
#define CLASS MUIC_BestTextEditorP
#define SUPERCLASSP MUIC_Mccprefs
#define INSTDATAP PrefsData
#define USERLIBID CLASS " " LIB_REV_STRING " (" LIB_DATE ") " LIB_COPYRIGHT
#define MASTERVERSION 19
#define MIN_STACKSIZE 8192
#define SHORTHELP "Text editor with Markdown and chat bubbles"
#define CLASSINIT
#define CLASSEXPUNGE

BOOL ClassInit(struct Library *base);
VOID ClassExpunge(struct Library *base);

#include "shellstart.c"
#include "mccinit.c"

struct Library *LocaleBase = NULL;
#if defined(__AMIGAOS4__)
struct LocaleIFace *ILocale = NULL;
#endif

BOOL ClassInit(UNUSED struct Library *base) {
    LocaleBase = OpenLibrary("locale.library", 38);
#if defined(__AMIGAOS4__)
    if (LocaleBase != NULL &&
        (ILocale = (struct LocaleIFace *)GetInterface(LocaleBase, "main", 1,
                                                      NULL)) == NULL) {
        CloseLibrary(LocaleBase);
        LocaleBase = NULL;
    }
#endif
    prefsOpenCatalog();
    prefsInitStrings();
    return TRUE;
}

VOID ClassExpunge(UNUSED struct Library *base) {
    prefsCloseCatalog();
#if defined(__AMIGAOS4__)
    if (ILocale != NULL)
        DropInterface((struct Interface *)ILocale);
    ILocale = NULL;
#endif
    if (LocaleBase != NULL)
        CloseLibrary(LocaleBase);
    LocaleBase = NULL;
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

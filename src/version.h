/*
 * MorphOS-style version (library.morph.zone/Style_Guide):
 *   $VER: <Name> <Version>.<Revision> (DD.MM.YYYY) © <Year> <Author>
 *
 * Official version (About, MUI, AmigaOS3/4, MorphOS): MAJOR.MINOR.BUILD_NUMBER
 *
 * Version  = APP_VERSION_MAJOR.APP_VERSION_MINOR (release line, bumped manually)
 * Revision = BUILD_NUMBER (auto per MorphOS cross-build; do not reset on minor bump)
 * APP_VERSION_PATCH = upstream merge base (not shown in MorphOS UI); keep at "0" in fork
 * Upstream release line at last sync: 3.1.0
 */

#ifndef BUILD_DATE
#define BUILD_DATE "??.??.????"
#endif

#define APP_VERSION_MAJOR "2"
#define APP_VERSION_MINOR "18"
#define APP_VERSION_PATCH "0"
#define BUILD_NUMBER "8817"

#define APP_VERSION_MORPHOS APP_VERSION_MAJOR "." APP_VERSION_MINOR
#define APP_VERSION_VER_REV APP_VERSION_MORPHOS "." BUILD_NUMBER

/* Shown in About, MUI Application_Version, logs */
#define APP_VERSION APP_VERSION_VER_REV
#define APP_VERSION_AMIGA APP_VERSION_VER_REV

#define APP_COPYRIGHT_YEAR "2023-2026"
#define APP_COPYRIGHT_HOLDER "Cameron Armstrong (Nightfox/sacredbanana)"
#define APP_COPYRIGHT "\251 " APP_COPYRIGHT_YEAR " " APP_COPYRIGHT_HOLDER

#define APP_VER_STRING_AMIGAGPT                                               \
    "$VER: AmigaGPT " APP_VERSION_VER_REV " (" BUILD_DATE ") " APP_COPYRIGHT
#define APP_VER_STRING_AMIGAGPTD                                              \
    "$VER: AmigaGPTD " APP_VERSION_VER_REV " (" BUILD_DATE ") " APP_COPYRIGHT

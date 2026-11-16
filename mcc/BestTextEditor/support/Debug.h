#ifndef MCC_DEBUG_H
#define MCC_DEBUG_H

/* Stubs so mccinit.c compiles without the full MCC debug framework. */

#ifndef ENTER
#define ENTER() ((void)0)
#endif
#ifndef LEAVE
#define LEAVE() ((void)0)
#endif
#ifndef RETURN
#define RETURN(x) ((void)0)
#endif
#ifndef E
#define E(...) ((void)0)
#endif
#ifndef D
#define D(...) ((void)0)
#endif
#ifndef DBF_STARTUP
#define DBF_STARTUP 0
#endif

static inline void SetupDebug(void) {}
static inline void CleanupDebug(void) {}

#endif

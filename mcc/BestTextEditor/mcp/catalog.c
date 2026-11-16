/*
 * BestTextEditor.mcp - catalog
 *
 * FlexCat writes BestTextEditor_mcp_cat.c for a program: it opens the catalog
 * from a constructor and closes it from a destructor. A library has no
 * startup code to run those, so the generated file is included here and its
 * functions are called from ClassInit() and ClassExpunge() instead.
 */
#include "BestTextEditor_mcp_cat.c"
#include "private.h"

void prefsOpenCatalog(void) { _STIOpenBestTextEditor_mcpCatalog(); }

void prefsCloseCatalog(void) {
    _STDCloseBestTextEditor_mcpCatalog();
    BestTextEditor_mcpCatalog = NULL;
}

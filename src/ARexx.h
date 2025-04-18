#include <libraries/mui.h>
#include <classes/arexx.h>
#include <intuition/classes.h>

extern struct MUI_Command arexxList[];
extern Object *arexxObject;

/**
 * Initialize the ARexx object
 * @return RETURN_OK on success, RETURN_ERROR on failure
 */
LONG initARexx();

/**
 * Close the ARexx object
 */
void closeARexx();

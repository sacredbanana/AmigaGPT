#include <libraries/mui.h>
#include <classes/arexx.h>
#include <intuition/classes.h>

extern struct MUI_Command arexxList[];
extern Object *arexxObject;

/**
 * Generate a picture with the active image profile and save it
 * @param prompt what the picture should show, in the local character set
 * @param destination the file to write
 * @return TRUE if the picture was saved
 **/
BOOL createImageFile(CONST_STRPTR prompt, CONST_STRPTR destination);

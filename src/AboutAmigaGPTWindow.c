#include <libraries/mui.h>
#include <mui/Aboutbox_mcc.h>
#include "AboutAmigaGPTWindow.h"
#include "gui.h"
#include "version.h"

Object *aboutAmigaGPTWindowObject;

/**
 * Create the about window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createAboutAmigaGPTWindow() {
    STRPTR buildString = AllocVec(512, MEMF_CLEAR | MEMF_ANY);
    STRPTR bodyString = AllocVec(2048, MEMF_CLEAR | MEMF_ANY);
    LONG success = RETURN_ERROR;
    snprintf(buildString, 512,
             BUILD_NUMBER " ("__DATE__
                          ")\n"
                          "%s: " GIT_COMMIT "\n"
                          "%s: " GIT_BRANCH "\n"
                          "%s: " GIT_TIMESTAMP,
             STRING_GIT_COMMIT, STRING_GIT_BRANCH, STRING_GIT_COMMIT_TIMESTAMP);
    snprintf(bodyString, 2048,
             "%s: https://paypal.me/sacredbanana\n\n"
             "%s\n\n"
             "\033b%%p\033n\n"
             "\t\033iCameron Armstrong\033n\n"
             "\t(@sacredbanana - GitHub/YouTube/Twitter, @Nightfox - "
             "EAB/Discord)\n\n"
             "\033b%%I\033n\n"
             "\t\033iMauricio Sandoval\033n\n\n"
             "\033b%%T\033n\n"
             "\t\033iBebbo\033n\n"
             "\t%s\n"
             "\thttps://github.com/bebbo\n"
             "\n"
             "\t\033iOpenAI\033n\n"
             "\t%s\n"
             "\n"
             "\t\033iEAB\033n\n"
             "\t%s\n"
             "\n"
             "\t\033iJan Zahurancik\033n\n"
             "\t%s\n"
             "\thttps://www.amikit.amiga.sk\n"
             "\n"
             "\t\033iCoffinOS\033n\n"
             "\t%s\n"
             "\thttps://getcoffin.net\n"
             "\n"
             "\t\033iAmiga Future Magazine\033n\n"
             "\t%s\n"
             "\thttps://www.amigafuture.de\n"
             "\n"
             "\t\033iWhatIFF? Magazine\033n\n"
             "\t%s\n"
             "\thttps://www.whatiff.info\n"
             "\n"
             "\t\033iDan Wood\033n\n"
             "\t%s\n"
             "\thttps://www.youtube.com/watch?v=-OA28r8Up5U\n"
             "\n"
             "\t\033iProteque-CBN\033n\n"
             "\t%s\n"
             "\thttps://www.youtube.com/watch?v=t3q8HQ6wrnw\n"
             "\n"
             "\t\033iAmigaBill\033n\n"
             "\t%s\n"
             "\thttps://www.twitch.tv/amigabill\n"
             "\n"
             "\t\033iLes Docs\033n\n"
             "\t%s\n"
             "\thttps://www.youtube.com/watch?v=BV5Fq1PresE\n"
             "\n"
             "\033b%s033n\n"
             "\tMIT\n"
             "\n"
             "\033b%%W\033n\n"
             "\thttps://github.com/sacredbanana/AmigaGPT/issues\n"
             "\thttps://eab.abime.net/showthread.php?t=114798\n",
             STRING_ABOUT_WINDOW_SUPPORT, STRING_ABOUT_WINDOW_BUILD_DETAILS,
             STRING_ABOUT_WINDOW_BEBBO, STRING_ABOUT_WINDOW_OPENAI,
             STRING_ABOUT_WINDOW_EAB, STRING_ABOUT_WINDOW_JAN,
             STRING_ABOUT_WINDOW_COFFINOS, STRING_ABOUT_WINDOW_AMIGA_FUTURE,
             STRING_ABOUT_WINDOW_WHATIFF, STRING_ABOUT_WINDOW_YOUTUBE_REVIEW,
             STRING_ABOUT_WINDOW_YOUTUBE_REVIEW, STRING_ABOUT_WINDOW_AMIGABILL,
             STRING_ABOUT_WINDOW_LES_DOCS, STRING_ABOUT_WINDOW_LICENSE);
    if ((aboutAmigaGPTWindowObject = AboutboxObject, MUIA_Aboutbox_Build,
         buildString, MUIA_Aboutbox_Credits, bodyString, MUIA_Aboutbox_URL,
         "https://github.com/sacredbanana/AmigaGPT", MUIA_Aboutbox_URLText,
         STRING_ABOUT_WINDOW_URL_TEXT, End)) {
        DoMethod(aboutAmigaGPTWindowObject, MUIM_Notify,
                 MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Window_Open, FALSE);
        success = RETURN_OK;
    }
    FreeVec(buildString);
    FreeVec(bodyString);
    return success;
}
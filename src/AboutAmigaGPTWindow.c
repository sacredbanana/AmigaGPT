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
    if ((aboutAmigaGPTWindowObject = AboutboxObject, MUIA_Aboutbox_Build,
         BUILD_NUMBER " ("__DATE__
                      ")\n"
                      "Git commit: " GIT_COMMIT "\nGit branch: " GIT_BRANCH "\n"
                      "Commit timestamp: " GIT_TIMESTAMP,
         MUIA_Aboutbox_Credits,
         "This app will always remain free but if you would like to support me "
         "you can do so at https://paypal.me/sacredbanana\n"
         "\n"
         "Click the version string above for build details.\n"
         "\n"
         "\033b%p\033n\n"
         "\t\033iCameron Armstrong\033n\n"
         "\t(@sacredbanana on GitHub, YouTube and Twitter, @Nightfox on EAB)\n"
         "\n"
         "\033b%I\033n\n"
         "\t\033iMauricio Sandoval\033n\n"
         "\n"
         "\033b%T\033n\n"
         "\t\033iBebbo\033n\n"
         "\tfor creating the Amiga GCC toolchain\n"
         "\thttps://github.com/bebbo\n"
         "\n"
         "\t\033iOpenAI\033n\n"
         "\tfor creating the GPT and DALL-E models\n"
         "\n"
         "\t\033iEAB\033n\n"
         "\tfor being a great community!\n"
         "\n"
         "\t\033iJan Zahurancik\033n\n"
         "\tfor all the thorough testing, bundling AmigaGPT into AmiKit and "
         "for all the moral support\n"
         "\thttps://www.amikit.amiga.sk\n"
         "\n"
         "\t\033iCoffinOS\033n\n"
         "\tfor bundling AmigaGPT into CoffinOS\n"
         "\thttps://getcoffin.net\n"
         "\n"
         "\t\033iAmiga Future Magazine\033n\n"
         "\tfor reviewing AmigaGPT and publishing several of its updates in "
         "the News from Aminet section\n"
         "\thttps://www.amigafuture.de\n"
         "\n"
         "\t\033iWhatIFF? Magazine\033n\n"
         "\tfor reviewing AmigaGPT and interviewing me in issue 14\n"
         "\thttps://www.whatiff.info\n"
         "\n"
         "\t\033iDan Wood\033n\n"
         "\tfor reviewing AmigaGPT on his YouTube channel\n"
         "\thttps://www.youtube.com/watch?v=-OA28r8Up5U\n"
         "\n"
         "\t\033iProteque-CBN\033n\n"
         "\tfor reviewing AmigaGPT on his YouTube channel\n"
         "\thttps://www.youtube.com/watch?v=t3q8HQ6wrnw\n"
         "\n"
         "\t\033iAmigaBill\033n\n"
         "\tfor covering AmigaGPT in the Amiga News section on his Twitch "
         "streams and allowing me to join his stream to promote it\n"
         "\thttps://www.twitch.tv/amigabill\n"
         "\n"
         "\t\033iLes Docs\033n\n"
         "\tfor making a video review and giving a tutorial on how to add "
         "support for the French accent\n"
         "\thttps://www.youtube.com/watch?v=BV5Fq1PresE\n"
         "\n"
         "\033bLicense\033n\n"
         "\tMIT\n"
         "\n"
         "\033b%W\033n\n"
         "\thttps://github.com/sacredbanana/AmigaGPT/issues\n"
         "\thttps://eab.abime.net/showthread.php?t=114798\n",
         MUIA_Aboutbox_URL, "https://github.com/sacredbanana/AmigaGPT",
         MUIA_Aboutbox_URLText,
         "Visit the GitHub repository for the latest release", End)) {
        DoMethod(aboutAmigaGPTWindowObject, MUIM_Notify,
                 MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Window_Open, FALSE);
        return RETURN_OK;
    } else {
        // printf("Warning: Could not create aboutAmigaGPTWindowObject\nThe
        // installed MUI version is probably too old.\nFalling back to a simple
        // requester.\n");
        return RETURN_ERROR;
    }
}
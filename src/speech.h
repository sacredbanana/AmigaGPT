#include <proto/dos.h>

enum SpeechSystem {
    SpeechSystemNone,
    SpeechSystemOld,
    SpeechSystemNew
};

void speakText(UBYTE *text);
LONG initSpeech(enum SpeechSystem speechSystem);
void closeSpeech();

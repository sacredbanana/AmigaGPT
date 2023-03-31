#include <proto/dos.h>

void speakText(UBYTE *text);
LONG openSpeechLibraries();
LONG openSpeechDevices();
void closeSpeechLibraries();
void closeSpeechDevices();
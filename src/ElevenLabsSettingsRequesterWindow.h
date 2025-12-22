#ifndef ELEVENLABS_SETTINGS_REQUESTER_WINDOW_H
#define ELEVENLABS_SETTINGS_REQUESTER_WINDOW_H

#include <proto/dos.h>

extern Object *elevenLabsSettingsRequesterWindowObject;
extern Object *elevenLabsAPIKeyString;
extern Object *elevenLabsModelList;
extern Object *elevenLabsVoiceSearchString;
extern Object *elevenLabsVoiceList;

extern struct Hook ElevenLabsSettingsWindowOpenHook;

/**
 * Create the ElevenLabs settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createElevenLabsSettingsRequesterWindow();

#endif

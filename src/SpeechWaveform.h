#ifndef SPEECHWAVEFORM_H
#define SPEECHWAVEFORM_H

#define SpeechWaveform_Dummy (0xbd001000UL)
#define MUIA_SpeechWaveform_FileName (SpeechWaveform_Dummy + 0x01)
#define MUIA_SpeechWaveform_Position (SpeechWaveform_Dummy + 0x02)
#define MUIA_SpeechWaveform_Duration (SpeechWaveform_Dummy + 0x03)
#define MUIA_SpeechWaveform_Seek (SpeechWaveform_Dummy + 0x04)
#define MUIA_SpeechWaveform_Playing (SpeechWaveform_Dummy + 0x05)
#define MUIA_SpeechWaveform_Paused (SpeechWaveform_Dummy + 0x06)
#define MUIA_SpeechWaveform_HasSpeech (SpeechWaveform_Dummy + 0x07)
#define MUIA_SpeechWaveform_Command (SpeechWaveform_Dummy + 0x08)

#define MUIV_SpeechWaveform_Command_Play 1
#define MUIV_SpeechWaveform_Command_Pause 2
#define MUIV_SpeechWaveform_Command_Stop 3
#define MUIV_SpeechWaveform_Command_Rewind 4

#define MUIC_SpeechWaveform speechWaveformClass->mcc_Class

#if defined(__AROS__) && !defined(NO_INLINE_STDARG)
#define SpeechWaveformObject MUIOBJMACRO_START(MUIC_SpeechWaveform)
#else
#define SpeechWaveformObject NewObject(MUIC_SpeechWaveform, NULL
#endif

extern struct MUI_CustomClass *speechWaveformClass;

LONG createSpeechWaveformClass(void);
void deleteSpeechWaveformClass(void);
void speechWaveformSetFile(Object *obj, CONST_STRPTR filename);
void speechWaveformLoadPlayback(Object *obj);

#endif

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
#define MUIA_SpeechWaveform_SampleRate (SpeechWaveform_Dummy + 0x09)
#define MUIA_SpeechWaveform_Channels (SpeechWaveform_Dummy + 0x0A)
#define MUIA_SpeechWaveform_Bits (SpeechWaveform_Dummy + 0x0B)
#define MUIA_SpeechWaveform_LittleEndian (SpeechWaveform_Dummy + 0x0C)
#define MUIA_SpeechWaveform_WavePen (SpeechWaveform_Dummy + 0x0D)
#define MUIA_SpeechWaveform_PeakPen (SpeechWaveform_Dummy + 0x0E)
#define MUIA_SpeechWaveform_SpectrumPen (SpeechWaveform_Dummy + 0x0F)
#define MUIA_SpeechWaveform_BackPen (SpeechWaveform_Dummy + 0x10)
#define MUIA_SpeechWaveform_BarPen (SpeechWaveform_Dummy + 0x11)
#define MUIA_SpeechWaveform_TextPen (SpeechWaveform_Dummy + 0x12)

#define MUIV_SpeechWaveform_Pen_Default ((ULONG)~0)

#define MUIV_SpeechWaveform_Command_Play 1
#define MUIV_SpeechWaveform_Command_Pause 2
#define MUIV_SpeechWaveform_Command_Stop 3
#define MUIV_SpeechWaveform_Command_Rewind 4

#define MUIM_SpeechWaveform_Play (SpeechWaveform_Dummy + 0x20)
#define MUIM_SpeechWaveform_Pause (SpeechWaveform_Dummy + 0x21)
#define MUIM_SpeechWaveform_Stop (SpeechWaveform_Dummy + 0x22)
#define MUIM_SpeechWaveform_Rewind (SpeechWaveform_Dummy + 0x23)
#define MUIM_SpeechWaveform_Service (SpeechWaveform_Dummy + 0x24)

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
void speechWaveformService(Object *obj);

#endif

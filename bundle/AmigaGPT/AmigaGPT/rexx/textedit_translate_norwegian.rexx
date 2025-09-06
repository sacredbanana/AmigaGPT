/* AmigaGPT Translates the selected text in TextEdit to Norwegian */
OPTIONS RESULTS

AMIGAGPT_PORT = "AMIGAGPT"

IF ~SHOW('P',AMIGAGPT_PORT) THEN DO
  /* The main AmigaGPT app is not open. Attempt to connect to AmigaGPTD instead */
    AMIGAGPT_PORT = "AMIGAGPTD"
    IF ~SHOW('P',AMIGAGPT_PORT) THEN DO
      SAY 'Cannot contact AmigaGPT. Please start either AmigaGPT or AmigaGPTD first.'
      EXIT 1
    END
END

ADDRESS 'TEXTEDIT.1'
'GETSELECTEDTEXT'

ADDRESS VALUE AMIGAGPT_PORT
'SENDMESSAGE M=gpt-5-mini Translate the following into Norwegian: 'SELECTEDTEXT

TRANSLATEDTEXT = RESULT

ADDRESS 'TEXTEDIT.1'
'BACKSPACE'
'TEXT 'TRANSLATEDTEXT

EXIT

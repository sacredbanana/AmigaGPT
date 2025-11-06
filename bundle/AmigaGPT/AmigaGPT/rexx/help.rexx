/* Shows AmigaGPT's supported ARexx commands */
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
ADDRESS VALUE AMIGAGPT_PORT

SAY "ARexx commands:"
'?'
SAY RESULT
SAY "Chat models:"
'LISTCHATMODELS'
SAY RESULT
SAY "Image models:"
'LISTIMAGEMODELS'
SAY RESULT
SAY "Image sizes:"
'LISTIMAGESIZES'
SAY RESULT
SAY "Voice models:"
'LISTVOICEMODELS'
SAY RESULT
SAY "Voices:"
'LISTVOICES'
SAY RESULT
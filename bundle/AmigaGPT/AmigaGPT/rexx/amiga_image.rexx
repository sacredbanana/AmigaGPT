/* AmigaGPT Display an image of a Commodore Amiga */
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
'CREATEIMAGE P=A beautiful Amiga computer on a desk'

IMAGE = RESULT
cmd = FindMultiView() || ' "' || IMAGE || '"'
ADDRESS COMMAND
(cmd)

EXIT 0

/* MultiView lives in SYS:Utilities, which is not always on the Shell path */
FindMultiView: PROCEDURE
  candidates = 'SYS:Utilities/MultiView C:MultiView'
  i = 1
  DO WHILE i <= WORDS(candidates)
    path = WORD(candidates, i)
    IF OPEN('mvtest', path, 'r') THEN DO
      CALL CLOSE('mvtest')
      RETURN path
    END
    i = i + 1
  END
  RETURN 'SYS:Utilities/MultiView'
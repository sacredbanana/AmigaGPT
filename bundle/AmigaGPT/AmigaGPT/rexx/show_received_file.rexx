/* Ask the hosted code interpreter for a file, save it and open it */
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
'SENDMESSAGE CI D=RAM: P=Use the code interpreter to write a 4-line poem about the Commodore Amiga to a file named amiga.txt. Return that file.'

REPLY = RESULT
FILE = FirstReceivedFile(REPLY)
IF FILE = "" THEN DO
  SAY 'AmigaGPT did not return a downloadable file.'
  IF REPLY ~= "" THEN SAY REPLY
  EXIT 1
END

cmd = FindMultiView() || ' "' || FILE || '"'
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

/* SENDMESSAGE appends FILES: and one saved path per line when files arrive */
FirstReceivedFile: PROCEDURE
  PARSE ARG text
  nl = '0A'x
  cr = '0D'x
  p = POS('FILES:', text)
  IF p = 0 THEN RETURN ""
  rest = SUBSTR(text, p + 6)
  DO WHILE rest ~= "" & (LEFT(rest,1) = nl | LEFT(rest,1) = cr | LEFT(rest,1) = ' ')
    rest = SUBSTR(rest, 2)
  END
  p = POS(nl, rest)
  IF p = 0 THEN line = rest
  ELSE line = LEFT(rest, p - 1)
  IF RIGHT(line,1) = cr THEN line = LEFT(line, LENGTH(line) - 1)
  RETURN STRIP(line)

/* AmigaGPT Say something */
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

ADDRESS COMMAND

CMDLINE = 'c:requeststring >t:AmigaGPTInput "Say something" "Type what you want to say"'

(CMDLINE)

IF OPEN(HANDLE,"t:AmigaGPTInput",'r') THEN DO
	INPUT = READLN(HANDLE)
	CALL CLOSE(HANDLE)
	ADDRESS COMMAND 'c:delete T:AmigaGPTInput QUIET'
END

ADDRESS VALUE AMIGAGPT_PORT
'SPEAKTEXT 'INPUT

EXIT 0
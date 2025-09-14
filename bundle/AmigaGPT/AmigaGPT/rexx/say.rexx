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

'Requeststring >T:AmigaGPTInput "Say something" "Type what you want to say"'

IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
	INPUT = READLN(HANDLE)
	CALL CLOSE(HANDLE)
	ADDRESS COMMAND 'Delete T:AmigaGPTInput QUIET'
END

PROMPT = INPUT

/* Get the list of voices */
ADDRESS VALUE AMIGAGPT_PORT
'LISTVOICES'
VOICES = RESULT
BUTTONS = TRANSLATE(VOICES, ", ", '0A'X)

ADDRESS COMMAND
'REQUESTCHOICE  >T:AmigaGPTInput "Select Voice" "Choose a voice from the list:" 'BUTTONS' "Cancel"'

IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
	INPUT = READLN(HANDLE)
	CALL CLOSE(HANDLE)
	ADDRESS COMMAND 'Delete T:AmigaGPTInput QUIET'
END

/* Show the chosen voice */
IF INPUT > 0 THEN DO
	VOICE = WORD(BUTTONS, INPUT)
	SAY "You selected:" VOICE
END

ADDRESS VALUE AMIGAGPT_PORT
'SPEAKTEXT V='VOICE PROMPT

EXIT 0
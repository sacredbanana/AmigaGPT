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
	'Delete T:AmigaGPTInput QUIET'
END

PROMPT = INPUT

SAY PROMPT

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
	'Delete T:AmigaGPTInput QUIET'
END

/* Show the chosen voice */
IF INPUT > 0 THEN DO
	VOICE = WORD(BUTTONS, INPUT)
END

'REQUESTCHOICE  >T:AmigaGPTInput "Select Mode" "Play the audio out loud or save the audio to a file?" "Play audio" "Write to file"'

IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
	INPUT = READLN(HANDLE)
	CALL CLOSE(HANDLE)
	'Delete T:AmigaGPTInput QUIET'
END

MODE = INPUT

SAY MODE

IF MODE == 1 THEN DO
	ADDRESS VALUE AMIGAGPT_PORT
	'SPEAKTEXT V='VOICE 'P='PROMPT
END

IF MODE == 0 THEN DO
	'REQUESTFILE >T:AmigaGPTInput DRAWER SYS: TITLE "Save voice clip" NOICONS'
	IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
		INPUT = READLN(HANDLE)
		CALL CLOSE(HANDLE)
		'Delete T:AmigaGPTInput QUIET'
	END

	OUTPUT = INPUT

	ADDRESS VALUE AMIGAGPT_PORT
	'SPEAKTEXT V='VOICE 'O='OUTPUT 'P='PROMPT
END

EXIT 0
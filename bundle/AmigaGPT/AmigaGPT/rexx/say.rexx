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

IF INPUT == 0 THEN DO
	EXIT 0
END

VOICE = WORD(BUTTONS, INPUT)

'REQUESTCHOICE  >T:AmigaGPTInput "Select Mode" "Play the audio out loud or save the audio to a file?" "Play audio" "Write to file" "Cancel"'

IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
	INPUT = READLN(HANDLE)
	CALL CLOSE(HANDLE)
	'Delete T:AmigaGPTInput QUIET'
END

IF INPUT == 0 THEN DO
	EXIT 0
END

MODE = INPUT

IF MODE == 1 THEN DO
	ADDRESS VALUE AMIGAGPT_PORT
	'SPEAKTEXT V='VOICE 'P='PROMPT
END

IF MODE == 2 THEN DO
	/* Get the list of audio formats */
	ADDRESS VALUE AMIGAGPT_PORT
	'LISTAUDIOFORMATS'
	AUDIO_FORMATS = RESULT
	BUTTONS = TRANSLATE(AUDIO_FORMATS, ", ", '0A'X)


	ADDRESS COMMAND
	'REQUESTCHOICE  >T:AmigaGPTInput "Select Audio Format" "Choose an audio format from the list:" 'BUTTONS' "Cancel"'

	IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
		INPUT = READLN(HANDLE)
		CALL CLOSE(HANDLE)
		'Delete T:AmigaGPTInput QUIET'
	END

	IF INPUT == 0 THEN DO
		EXIT 0
	END

	AUDIO_FORMAT = WORD(BUTTONS, INPUT)

	'REQUESTFILE >T:AmigaGPTInput DRAWER SYS: FILE "audio.'AUDIO_FORMAT'" TITLE "Save voice clip" POSITIVE "Save" NEGATIVE "Cancel" SAVEMODE NOICONS'
	IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
		INPUT = READLN(HANDLE)
		CALL CLOSE(HANDLE)
		'Delete T:AmigaGPTInput QUIET'
	END

	OUTPUT = INPUT

	ADDRESS VALUE AMIGAGPT_PORT
	'SPEAKTEXT V='VOICE 'O='OUTPUT 'F='AUDIO_FORMAT 'P='PROMPT
END

EXIT 0
/* AmigaGPT Sing a song */
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

SAY "Generating song. Please wait..."

ADDRESS VALUE AMIGAGPT_PORT
'SENDMESSAGE M=gpt-5-mini P=Sing a medium length song. Just give me the lyrics, no other text.'
SONG = TRANSLATE(RESULT, ", ", '0A'X)
'SPEAKTEXT V='VOICE 'P='SONG

EXIT 0
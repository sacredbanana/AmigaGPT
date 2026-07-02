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

'Requeststring  >T:AmigaGPTInput TITLE="Say something" BODY="Type what you want to say"'

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
BUTTONS = QuoteLines(VOICES)

ADDRESS COMMAND
'REQUESTCHOICE >T:AmigaGPTInput "Select Voice" "Choose a voice from the list:" 'BUTTONS' "Cancel"'

IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
	INPUT = READLN(HANDLE)
	CALL CLOSE(HANDLE)
	'Delete T:AmigaGPTInput QUIET'
END

IF INPUT == 0 THEN DO
	EXIT 0
END

VOICE = GetLine(VOICES, INPUT)

IF IsBuiltInSpeechSet(VOICES) THEN DO
	ADDRESS VALUE AMIGAGPT_PORT
	'SPEAKTEXT V="'VOICE'"' 'P='PROMPT
	EXIT 0
END

'REQUESTCHOICE >T:AmigaGPTInput "Select Mode" "Play the audio out loud or save the audio to a file?" "Play audio" "Write to file" "Cancel"'

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
	'SPEAKTEXT V="'VOICE'"' 'P='PROMPT
END

IF MODE == 2 THEN DO
	/* Get the list of audio formats */
	ADDRESS VALUE AMIGAGPT_PORT
	'LISTAUDIOFORMATS'
	AUDIO_FORMATS = RESULT
	BUTTONS = TRANSLATE(AUDIO_FORMATS, ", ", '0A'X)


	ADDRESS COMMAND
	'REQUESTCHOICE >T:AmigaGPTInput "Select Audio Format" "Choose an audio format from the list:" 'BUTTONS' "Cancel"'

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
	'SPEAKTEXT V="'VOICE'"' 'O='OUTPUT 'F='AUDIO_FORMAT 'P='PROMPT
END

IsBuiltInSpeechSet: PROCEDURE
	PARSE ARG voices

	nl = '0A'X
	count = 0

	DO WHILE voices ~= ""
		p = POS(nl, voices)

		IF p = 0 THEN DO
			line = STRIP(voices)
			voices = ""
		END
		ELSE DO
			line = STRIP(SUBSTR(voices, 1, p - 1))
			voices = SUBSTR(voices, p + 1)
		END

		IF RIGHT(line,1) = '0D'X THEN
			line = LEFT(line, LENGTH(line) - 1)

		IF line ~= "" THEN DO
			count = count + 1
			SELECT
				WHEN count = 1 THEN IF TRANSLATE(line) ~= "MALE" THEN RETURN 0
				WHEN count = 2 THEN IF TRANSLATE(line) ~= "FEMALE" THEN RETURN 0
				WHEN count = 3 THEN IF TRANSLATE(line) ~= "MALE ROBOT" THEN RETURN 0
				WHEN count = 4 THEN IF TRANSLATE(line) ~= "FEMALE ROBOT" THEN RETURN 0
				OTHERWISE RETURN 0
			END
		END
	END

	IF count = 4 THEN RETURN 1
	RETURN 0

QuoteLines: PROCEDURE
	PARSE ARG s

	out = ""
	nl = '0A'X

	DO WHILE s ~= ""
		p = POS(nl, s)

		IF p = 0 THEN DO
			line = STRIP(s)
			s = ""
		END
		ELSE DO
			line = STRIP(SUBSTR(s, 1, p - 1))
			s = SUBSTR(s, p + 1)
		END

	/* remove CR if LISTVOICES returned CR/LF */
	IF RIGHT(line,1) = '0D'X THEN
		line = LEFT(line, LENGTH(line) - 1)

	IF line ~= "" THEN DO
		IF out ~= "" THEN out = out || " "
		out = out || '"' || line || '"'
		END
	END

	RETURN out

GetLine: PROCEDURE
	PARSE ARG s, n

	nl = '0A'X
	idx = 1

	DO WHILE s ~= ""
		p = POS(nl, s)

		IF p = 0 THEN DO
			line = STRIP(s)
			s = ""
		END
		ELSE DO
			line = STRIP(SUBSTR(s, 1, p - 1))
			s = SUBSTR(s, p + 1)
		END

		IF RIGHT(line,1) = '0D'X THEN
			line = LEFT(line, LENGTH(line) - 1)

		IF line ~= "" THEN DO
			IF idx = n THEN RETURN line
		idx = idx + 1
		END
	END

	RETURN ""

EXIT 0
/* AmigaGPT Say something */
OPTIONS RESULTS

ADDRESS COMMAND

CMDLINE = 'c:requeststring >t:AmigaGPTInput "Say something" "Type what you want to say"'

(CMDLINE)

IF OPEN(HANDLE,"t:AmigaGPTInput",'r') THEN DO
	INPUT = READLN(HANDLE)
	CALL CLOSE(HANDLE)
	ADDRESS COMMAND 'c:delete T:AmigaGPTInput'
END

ADDRESS 'AMIGAGPT'

'SPEAKTEXT 'INPUT

RETURN INPUT

EXIT

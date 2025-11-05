/* Asks AmigaGPT a question and then displays it in a popup window */
/* Uses RxMUI to create a GUI or fallback to a simple requester if RxMUI is not installed */
OPTIONS RESULTS
SIGNAL ON HALT
SIGNAL ON BREAK_C

/* -------- main -------- */
FALLBACK = 0
AMIGAGPT_PORT = "AMIGAGPT"
CALL Init
IF FALLBACK THEN DO
  CALL Fallback_RequestChoice
  EXIT 0
END
CALL CreateApp
CALL HandleApp
EXIT 0

/* -------- procedures -------- */

Init: PROCEDURE EXPOSE AMIGAGPT_PORT FALLBACK
  IF ~SHOW('P',AMIGAGPT_PORT) THEN DO
  /* The main AmigaGPT app is not open. Attempt to connect to AmigaGPTD instead */
    AMIGAGPT_PORT = "AMIGAGPTD"
    IF ~SHOW('P',AMIGAGPT_PORT) THEN DO
      SAY 'Cannot contact AmigaGPT. Please start either AmigaGPT or AmigaGPTD first.'
      EXIT 1
    END
  END
  l="rmh.library"; IF ~SHOW("L",l) THEN ; IF ~ADDLIB(l,0,-30) THEN DO; FALLBACK=1; RETURN; END
  ADDRESS COMMAND 'VERSION rxmui.library >NIL:'
  IF RC ~= 0 THEN DO
    FALLBACK = 1
    RETURN
  END
  IF AddLibrary("rexxsupport.library","rxmui.library") ~= 0 THEN DO
    FALLBACK = 1
    RETURN
  END
  CALL RxMUIOpt("DebugMode ShowErr")
  RETURN

Fallback_RequestChoice: PROCEDURE EXPOSE AMIGAGPT_PORT
  SAY "RxMUI not installed. Install RxMUI to be able to use all features of this script"
  SAY "Retrieving answer using AmigaGPT. Please wait..."
  'Requeststring >T:AmigaGPTInput "Ask me a question" "Type what you want to ask"'

  IF OPEN(HANDLE,"T:AmigaGPTInput",'r') THEN DO
    INPUT = READLN(HANDLE)
    CALL CLOSE(HANDLE)
    'Delete T:AmigaGPTInput QUIET'
  END

  PROMPT = INPUT
  ADDRESS VALUE AMIGAGPT_PORT
  'SENDMESSAGE M=gpt-5-nano 'PROMPT
  ADDRESS COMMAND
  'REQUESTCHOICE >NIL: "Ask AmigaGPT" 'RESULT' "Ok"'
  RETURN

GetAnswer: PROCEDURE EXPOSE AMIGAGPT_PORT
  CALL GetAttr("asktext","contents", QUESTION)
  QUESTION = TRANSLATE(QUESTION, ", ", '0A'X)
  ADDRESS VALUE AMIGAGPT_PORT
  'SENDMESSAGE M=gpt-5-mini 'QUESTION
  ADDRESS COMMAND
  ANSWER = ParseText(RESULT)
  SAY ANSWER
  EXIT 0

CreateApp: PROCEDURE
  app.Title      = "AmigaGPT Answer"
  app.Base       = "AMIGAGPT_ANSWER"
  app.SubWindow  = "win"

   win.Title     = "Answer"
   win.Contents  = "root"
   win.SizeGadget = 1
   win.DragBar    = 1
   win.width      = -100-50
   win.height = -100-50

    /* vertical stack */
    root.class        = "group"

    /* multi-line text */
    root.0            = "asktext"

    asktext.class     = "TextEditor"
    asktext.background = "textback"
    /* asktext.frame     = "group" */
    asktext.selected = 1

    /* buttons row */
    root.1            = "btns"
     btns.class       = "group"
     btns.horiz       = 1

     /* _Ok */
     btns.0           = "btnOk"
      btnOk.class      = "text"
      btnOk.frame      = "button"
      btnOk.inputmode  = "relverify"
      btnOk.contents   = "Ok"

  res = NewObj("application","app")
  IF res > 0 THEN EXIT 30

  /* events → APPEVENTs (still "pressed") */
  CALL Notify("win","closerequest",1,"app","ReturnID","quit")
  CALL Notify("btnOk","pressed",1,"app","ReturnID")

  CALL Set("win","open",1)

  CALL Set("win", "ActiveObject", "asktext")
  RETURN

HandleApp: PROCEDURE EXPOSE AMIGAGPT_PORT
  ctrl_c = 2**12
  DO FOREVER
    CALL NewHandle("APP","H",ctrl_c)
    IF AND(h.signals,ctrl_c) > 0 THEN LEAVE
    SELECT
      WHEN h.event = "QUIT" THEN LEAVE
      WHEN h.event = "BTNOK" THEN CALL GetAnswer
      OTHERWISE INTERPRET h.event
    END
  END
  RETURN

/* -------- signals -------- */
HALT:
BREAK_C:
  EXIT 5
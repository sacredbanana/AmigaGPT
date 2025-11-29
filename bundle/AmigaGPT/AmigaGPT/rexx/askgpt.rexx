/* Asks AmigaGPT a question then displays it */
/* Uses RxMUI to create a GUI or fallback to a simple requester if RxMUI is not installed */
OPTIONS RESULTS
SIGNAL ON HALT
SIGNAL ON BREAK_C

ARG ARG1 PROMPT
PARSE ARG PROMPT

GUI = 0
IF ARG1 = "GUI" THEN DO
  GUI = 1
  PARSE ARG ARG1 PROMPT
END

/* -------- main -------- */
AMIGAGPT_PORT = "AMIGAGPT"
CALL Init
IF ~GUI THEN DO
  CALL NoGUI_RequestChoice
  EXIT 0
END
CALL CreateApp
CALL HandleApp
EXIT 0

/* -------- procedures -------- */

Init: PROCEDURE EXPOSE AMIGAGPT_PORT GUI
  IF ~SHOW('P',AMIGAGPT_PORT) THEN DO
  /* The main AmigaGPT app is not open. Attempt to connect to AmigaGPTD instead */
    AMIGAGPT_PORT = "AMIGAGPTD"
    IF ~SHOW('P',AMIGAGPT_PORT) THEN DO
      SAY 'Cannot contact AmigaGPT. Please start either AmigaGPT or AmigaGPTD first.'
      EXIT 1
    END
  END
  IF GUI = 1 THEN DO
    l="rmh.library"; IF ~SHOW("L",l) THEN ; IF ~ADDLIB(l,0,-30) THEN DO; GUI=0; RETURN; END
    ADDRESS COMMAND 'VERSION rxmui.library >NIL:'
    IF RC ~= 0 THEN DO
      SAY "RxMUI not installed. Install RxMUI to be able to use all features of this script"
      GUI = 0
      RETURN
    END
  END
  IF AddLibrary("rexxsupport.library","rxmui.library") ~= 0 THEN DO
    GUI = 0
    RETURN
  END
  CALL RxMUIOpt("DebugMode ShowErr")
  RETURN

NoGUI_RequestChoice: PROCEDURE EXPOSE AMIGAGPT_PORT PROMPT
  IF PROMPT = "" THEN DO
    SAY "What do you want to ask?"
    PARSE PULL PROMPT
  END

  ADDRESS VALUE AMIGAGPT_PORT
  SAY "Retrieving answer using AmigaGPT. Please wait..."
  'SENDMESSAGE P='PROMPT
  SAY ParseText(RESULT)
  RETURN

GetAnswer: PROCEDURE EXPOSE AMIGAGPT_PORT
  CALL GetAttr("asktext","Contents", QUESTION)
  QUESTION = TRANSLATE(QUESTION, ", ", '0A'X)
  CALL SetAttr("asktext","Contents", "Retrieving answer using AmigaGPT. Please wait...")
  CALL SetAttr("asktext", "ReadOnly", 1)
  ADDRESS VALUE AMIGAGPT_PORT
  'SENDMESSAGE WEBSEARCH P='QUESTION
  ADDRESS COMMAND
  ANSWER = ParseText(RESULT)
  CALL SetAttr("asktext","Contents", '1B'X"b"QUESTION '0A'X'0A'X ANSWER)
  CALL KillNotify("btnOk","app")
  CALL Notify("btnOk","pressed",1,"app","ReturnID", "quit")
  RETURN

CreateApp: PROCEDURE
  app.Title      = "AmigaGPT AskGPT"
  app.Base       = "AMIGAGPT_ASKGPT"
  app.SubWindow  = "win"

   win.Title     = "AskGPT"
   win.Contents  = "root"
   win.SizeGadget = 1
   win.DragBar    = 1
   win.width      = -100-50
   win.height = -100-50

    /* vertical stack */
    root.class        = "group"

    root.0            = "header"
    
    header.class      = "Text"
    header.contents   = '1B'X"c"'1B'X"bWhat would you like to ask?"

    /* multi-line text */
    root.1            = "asktext"

    asktext.class     = "TextEditor"
    asktext.background = "textback"

    /* buttons row */
    root.2             = "btns"
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
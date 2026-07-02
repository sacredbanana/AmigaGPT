/* Asks AmigaGPT a question then displays it */
/* If the GUI argument is provided, then it uses RxMUI to create a GUI or fallback to a simple requester if RxMUI is not installed */
/* If the NEWCHAT argument is provided, then it clears the conversation history and starts a new chat */
/* If the PROFILE/PR argument is provided, then it uses the specified profile */
OPTIONS RESULTS
SIGNAL ON HALT
SIGNAL ON BREAK_C

GUI = 0
NEWCHAT = 0
PROFILE = ""
PROMPT = ""

PARSE ARG CMDLINE
CALL ParseArgs CMDLINE

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
  IF GUI = 0 THEN RETURN
  l="rmh.library"; IF ~SHOW("L",l) THEN ; IF ~ADDLIB(l,0,-30) THEN DO; GUI=0; RETURN; END
  ADDRESS COMMAND 'VERSION rxmui.library >NIL:'
  IF RC ~= 0 THEN DO
    SAY "RxMUI not installed. Install RxMUI to be able to use all features of this script"
    GUI = 0
    RETURN
  END
  IF AddLibrary("rxmui.library", "rexxsupport.library") ~= 0 THEN DO
    GUI = 0
    RETURN
  END
  CALL RxMUIOpt("DebugMode ShowErr")
  RETURN

Trim: PROCEDURE
  PARSE ARG s
  RETURN STRIP(s)

ParseArgs: PROCEDURE EXPOSE GUI PROFILE PROMPT NEWCHAT
  PARSE ARG s

  s = STRIP(s)
  PROFILE = ""
  PROMPT = ""
  NEWCHAT = 0

  /* Optional GUI / NEWCHAT arguments at the front, in any order */
  DO FOREVER
    first = TRANSLATE(WORD(s,1))
    SELECT
      WHEN first = "GUI" THEN DO
        GUI = 1
        s = STRIP(SUBWORD(s,2))
      END
      WHEN first = "NEWCHAT" THEN DO
        NEWCHAT = 1
        s = STRIP(SUBWORD(s,2))
      END
      OTHERWISE LEAVE
    END
  END

  rest = ExtractProfile(s)
  PROMPT = STRIP(rest)
  RETURN

ExtractProfile: PROCEDURE EXPOSE PROFILE
  PARSE ARG rest

  PROFILE = ""
  rest = STRIP(rest)
  upper = TRANSLATE(rest)

  p = FindProfilePos(upper)
  IF p = 0 THEN RETURN rest

  IF SUBSTR(upper,p,8) = "PROFILE=" THEN
    keylen = 8
  ELSE
    keylen = 3

  vstart = p + keylen
  ch = SUBSTR(rest,vstart,1)

  IF ch = '"' THEN DO
    q = POS('"', rest, vstart + 1)
    IF q = 0 THEN q = LENGTH(rest) + 1
    PROFILE = SUBSTR(rest, vstart + 1, q - vstart - 1)
    vend = q
  END
  ELSE DO
    sp = POS(' ', rest, vstart)
    IF sp = 0 THEN sp = LENGTH(rest) + 1
    PROFILE = SUBSTR(rest, vstart, sp - vstart)
    vend = sp - 1
  END

  before = STRIP(SUBSTR(rest,1,p-1))
  after  = STRIP(SUBSTR(rest,vend+1))

  IF before ~= "" & after ~= "" THEN
    RETURN before || " " || after
  ELSE
    RETURN before || after

FindProfilePos: PROCEDURE
  PARSE ARG s
  p = 1
  DO WHILE p <= LENGTH(s)
    p1 = POS("PROFILE=", s, p)
    p2 = POS("PR=", s, p)

    IF p1 = 0 THEN p1 = 999999
    IF p2 = 0 THEN p2 = 999999

    p = MIN(p1, p2)
    IF p = 999999 THEN RETURN 0

    IF p = 1 THEN RETURN p
    IF SUBSTR(s,p-1,1) = " " THEN RETURN p

    p = p + 1
  END
  RETURN 0

/* Replace all non-overlapping occurrences of old with new in s */
ReplaceAll: PROCEDURE
  PARSE ARG s, old, new
  IF old = '' THEN RETURN s            /* avoid infinite loop */
  p = 1; out = ''
  DO FOREVER
    i = POS(old, s, p)
    IF i = 0 THEN DO
      out = out || SUBSTR(s, p)        /* tack on the tail */
      LEAVE
    END
    out = out || SUBSTR(s, p, i - p) || new
    p = i + LENGTH(old)                 /* advance past the match */
  END
  RETURN out

NoGUI_RequestChoice: PROCEDURE EXPOSE AMIGAGPT_PORT PROMPT PROFILE NEWCHAT
  IF PROMPT = "" THEN DO
    SAY "What do you want to ask?"
    PARSE PULL PROMPT
  END

  ADDRESS VALUE AMIGAGPT_PORT
  SAY "Retrieving answer using AmigaGPT. Please wait..."
  IF NEWCHAT THEN 'NEWCHAT'
  CMD = 'SENDMESSAGE'
  IF PROFILE ~= "" THEN CMD = CMD ' PR="' || PROFILE || '"'
  CMD = CMD ' P='PROMPT
  CMD
  ANSWER = ReplaceAll(RESULT, "\n", '0A'X)
  SAY ANSWER
  RETURN

GetAnswer: PROCEDURE EXPOSE AMIGAGPT_PORT PROFILE NEWCHAT
  CALL GetAttr("asktext","Contents", PROMPT)
  PROMPT = TRANSLATE(PROMPT, ", ", '0A'X)
  CALL SetAttr("asktext","Contents", "Retrieving answer using AmigaGPT. Please wait...")
  CALL SetAttr("asktext", "ReadOnly", 1)
  ADDRESS VALUE AMIGAGPT_PORT
  IF NEWCHAT THEN 'NEWCHAT'
  CMD = 'SENDMESSAGE'
  IF PROFILE ~= "" THEN CMD = CMD ' PR="' || PROFILE || '"'
  CMD = CMD ' P='PROMPT
  CMD
  ADDRESS COMMAND
  ANSWER = ParseText(RESULT)
  CALL SetAttr("asktext","Contents", '1B'X"b"PROMPT '0A'X'0A'X ANSWER)
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

HandleApp: PROCEDURE EXPOSE AMIGAGPT_PORT PROFILE
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
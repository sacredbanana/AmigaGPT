/* Generates a random poem using AmigaGPT and then displays it in a popup window */
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
CALL GetPoem
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
  SAY "Retrieving poem using AmigaGPT. Please wait..."
  ADDRESS VALUE AMIGAGPT_PORT
  'SENDMESSAGE M=gpt-5-nano Tell me a short poem on a single line'
  ADDRESS COMMAND
  'REQUESTCHOICE >NIL: "Random Poem" 'RESULT' Ok"'
  RETURN

GetPoem: PROCEDURE EXPOSE AMIGAGPT_PORT
  CALL Set("poemtext","text","Generating a poem for ya")
  ADDRESS VALUE AMIGAGPT_PORT
  'SENDMESSAGE M=gpt-5-mini Tell me a medium length poem'
  ADDRESS COMMAND
  POEM = ParseText(RESULT)
  CALL Set("poemtext","text", POEM)
  RETURN

CreateApp: PROCEDURE
  app.Title      = "AmigaGPT Poem"
  app.Base       = "AMIGAGPT_POEM"
  app.SubWindow  = "win"

   win.Title     = "Random Poem"
   win.Contents  = "root"
   win.SizeGadget = 1
   win.DragBar    = 1
   win.width      = -100-50
   win.height = -100-50

    /* vertical stack */
    root.class        = "group"

    /* multi-line text */
    root.0            = "lv"

    lv.Class="NListview"
    lv.CycleChain=1
    lv.List="poemtext"

      poemtext.class     = "NFloatText"
      poemtext.background = "textback"
      poemtext.frame     = "group"

    /* buttons row */
    root.1            = "btns"
     btns.class       = "group"
     btns.horiz       = 1

     /* _Haha */
     btns.0           = "btnOk"
      btnOk.class      = "text"
      btnOk.frame      = "button"
      btnOk.inputmode  = "relverify"
      btnOk.contents   = "Ok"

     /* _More */
     btns.1           = "btnMore"
      btnMore.class      = "text"
      btnMore.frame      = "button"
      btnMore.inputmode  = "relverify"
      btnMore.contents   = "Tell me another poem"

  res = NewObj("application","app")
  IF res > 0 THEN EXIT 30

  /* events → APPEVENTs (still "pressed") */
  CALL Notify("win","closerequest",1,"app","ReturnID","quit")
  CALL Notify("btnOk","pressed",1,"app","ReturnID","quit")
  CALL Notify("btnMore","pressed",1,"app","ReturnID")

  CALL Set("win","open",1)
  RETURN

HandleApp: PROCEDURE EXPOSE AMIGAGPT_PORT
  ctrl_c = 2**12
  DO FOREVER
    CALL NewHandle("APP","H",ctrl_c)
    IF AND(h.signals,ctrl_c) > 0 THEN LEAVE
    SELECT
      WHEN h.event = "QUIT" THEN LEAVE
      WHEN h.event = "BTNMORE" THEN CALL GetPoem
      OTHERWISE INTERPRET h.event
    END
  END
  RETURN

/* -------- signals -------- */
HALT:
BREAK_C:
  EXIT 5
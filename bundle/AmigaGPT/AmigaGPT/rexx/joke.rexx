/* Generates a random joke using AmigaGPT and then displays it in a popup window */
/* Uses RxMUI to create a GUI or fallback to a simple requester if RxMUI is not installed */
OPTIONS RESULTS
SIGNAL ON HALT
SIGNAL ON BREAK_C

/* -------- main -------- */
fallback = 0
CALL Init
IF fallback THEN DO
  CALL Fallback_RequestChoice
  EXIT 0
END
CALL CreateApp
CALL GetJoke
CALL HandleApp
EXIT 0

/* -------- procedures -------- */

Init: PROCEDURE
  l="rmh.library"; IF ~SHOW("L",l) THEN ; IF ~ADDLIB(l,0,-30) THEN DO; fallback=1; RETURN; END
  IF AddLibrary("rexxsupport.library","rxmui.library") ~= 0 THEN DO; fallback=1; RETURN; END
  CALL RxMUIOpt("DebugMode ShowErr")
  RETURN

Fallback_RequestChoice: PROCEDURE
  SAY "RxMUI not installed. Install RxMUI to be able to use all features of this script"
  ADDRESS 'AMIGAGPT'
  'SENDMESSAGE M=gpt-5-nano Tell me a short funny joke on a single line'
  ADDRESS COMMAND
  'REQUESTCHOICE >NIL: "Random Joke" 'RESULT' "Haha" "ROFL" "LMAO" "Yikes"'
  RETURN

GetJoke: PROCEDURE EXPOSE joke
  IF ~SHOW('P','AMIGAGPT') THEN DO
    joke = 'Cannot contact AmigaGPT. Please start AmigaGPT first.'
    RETURN
  END
  CALL Set("joketext","text","Generating a funny joke for ya")
  ADDRESS 'AMIGAGPT'
  'SENDMESSAGE M=gpt-5-mini Tell me a medium length funny joke'
  ADDRESS COMMAND
  joke = ParseText(RESULT)
  CALL Set("joketext","text",joke)
  RETURN

CreateApp: PROCEDURE EXPOSE joke
  app.Title      = "AmigaGPT Joke"
  app.Base       = "AMIGAGPT_JOKE"
  app.SubWindow  = "win"

   win.Title     = "Random Joke"
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
    lv.List="joketext"

      joketext.class     = "NFloatText"
      joketext.background = "textback"
      joketext.frame     = "group"

    /* buttons row */
    root.1            = "btns"
     btns.class       = "group"
     btns.horiz       = 1

     /* _Haha */
     btns.0           = "btnHaha"
      btnHaha.class      = "text"
      btnHaha.frame      = "button"
      btnHaha.inputmode  = "relverify"
      btnHaha.contents   = "Haha"

     /* _More */
     btns.1           = "btnMore"
      btnMore.class      = "text"
      btnMore.frame      = "button"
      btnMore.inputmode  = "relverify"
      btnMore.contents   = "Lame. Tell me another joke."

  res = NewObj("application","app")
  IF res > 0 THEN EXIT 30

  /* events → APPEVENTs (still "pressed") */
  CALL Notify("win","closerequest",1,"app","ReturnID","quit")
  CALL Notify("btnHaha","pressed",1,"app","ReturnID","quit")
  CALL Notify("btnMore","pressed",1,"app","ReturnID")

  CALL Set("win","open",1)
  RETURN

HandleApp: PROCEDURE EXPOSE joke
  ctrl_c = 2**12
  DO FOREVER
    CALL NewHandle("APP","H",ctrl_c)
    IF AND(h.signals,ctrl_c) > 0 THEN LEAVE
    SELECT
      WHEN h.event = "QUIT" THEN LEAVE
      WHEN h.event = "BTNMORE" THEN CALL GetJoke
      OTHERWISE INTERPRET h.event
    END
  END
  RETURN

/* -------- signals -------- */
HALT:
BREAK_C:
  EXIT 5
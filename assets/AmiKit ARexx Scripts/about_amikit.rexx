/* Gives a description of AmiKit using AmigaGPT and then displays it in a popup window */
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
CALL GetAmiKit
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
  SAY "Retrieving AmiKit description using AmigaGPT. Please wait..."
  ADDRESS VALUE AMIGAGPT_PORT
  'SENDMESSAGE M=gpt-5-nano P=Give me a short description of AmiKit. Do it all on a single line.'
  ADDRESS COMMAND
  'REQUESTCHOICE >NIL: "AmiKit" 'RESULT' Ok"'
  RETURN

GetAmiKit: PROCEDURE EXPOSE AMIGAGPT_PORT
  CALL Set("amikittext","text","Generating a AmiKit description for ya")
  ADDRESS VALUE AMIGAGPT_PORT
  'SENDMESSAGE M=gpt-5-mini P=Give me a description of AmiKit.'
  ADDRESS COMMAND
  AMIKIT = ParseText(RESULT)
  CALL Set("amikittext","text", AMIKIT)
  RETURN

CreateApp: PROCEDURE
  app.Title      = "AmigaGPT AmiKit Description"
  app.Base       = "AMIGAGPT_AMIKit_DESCRIPTION"
  app.SubWindow  = "win"

   win.Title     = "AmiKit"
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
    lv.List="amikittext"

      amikittext.class     = "NFloatText"
      amikittext.background = "textback"
      amikittext.frame     = "group"

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

  res = NewObj("application","app")
  IF res > 0 THEN EXIT 30

  /* events → APPEVENTs (still "pressed") */
  CALL Notify("win","closerequest",1,"app","ReturnID","quit")
  CALL Notify("btnOk","pressed",1,"app","ReturnID","quit")

  CALL Set("win","open",1)
  RETURN

HandleApp: PROCEDURE
  ctrl_c = 2**12
  DO FOREVER
    CALL NewHandle("APP","H",ctrl_c)
    IF AND(h.signals,ctrl_c) > 0 THEN LEAVE
    SELECT
      WHEN h.event = "QUIT" THEN LEAVE
      OTHERWISE INTERPRET h.event
    END
  END
  RETURN

/* -------- signals -------- */
HALT:
BREAK_C:
  EXIT 5
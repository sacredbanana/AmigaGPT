/* AmigaGPT Translates the selected text in TextEdit to Norwegian */
OPTIONS RESULTS

ADDRESS 'TEXTEDIT.1'

'GETSELECTEDTEXT'

/* Remove newlines and replace them with commas to treat as a single string */
SELECTEDTEXT = TRANSLATE(RESULT, ", ", '0A'X)

ADDRESS 'AMIGAGPT'

'SENDMESSAGE M=gpt-4o Translate the following into Norwegian: 'SELECTEDTEXT

TRANSLATEDTEXT = RESULT

ADDRESS 'TEXTEDIT.1'

'BACKSPACE'

'TEXT 'TRANSLATEDTEXT

EXIT

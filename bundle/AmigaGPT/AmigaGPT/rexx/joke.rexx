/* AmigaGPT Random Joke Script */
OPTIONS RESULTS
ADDRESS 'AMIGAGPT'

'SENDMESSAGE P=Tell me a funny joke'
ADDRESS COMMAND
JOKE = TRANSLATE(RESULT, ", ", '0A'X)
'REQUESTCHOICE >NIL: "Random Joke" "'JOKE'" "_Haha" "_ROFL" "_LMAO" "_Yikes"'

EXIT
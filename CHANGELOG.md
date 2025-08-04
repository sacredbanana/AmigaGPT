# Changelog

## 2.8.1 (2025-08-04)

- Fixed some menu items not working
- The window position and size is now remembered between sessions

## 2.8.0 (2025-08-02)

- Add "Fixed width fonts" item in the "View" menu to toggle fixed width fonts for the chat input and chat output
- Add "User text alignment" and "Assistant text alignment" items in the "View" menu to set the alignment of user and assistant messages in the chat

## 2.7.0 (2025-07-27)

- Stop the stack size warning appearing on MorphOS
- Running ARexx scripts now working in MorphOS (requires the 68k version of rexxsyslib.library installed)
- Fix bug where the conversation window will say "(null)" if the user clicks in the conversation list below all the entries
- Chat window scrolls to bottom when new text is received
- Can now send the message by pressing return (right shift + return will create a new line without sending) (MUI 5 only)
- The status bar now has a dark background to make the status more visible
- Fixed bug where your sent message does not get formatted correctly if it has more than 1 line
- While receiving a message, the chat window only shows the current message. This dramatically speeds up text rendering. Long conversations no longer become increasingly sluggish after every new message

## 2.6.0 (2025-05-06)

- Added GPT Image 1 image model
- Added GPT-4.1 mini and GPT-4.1 nano chat models
- Retrieves image data directly from OpenAI instead of downloading from file server to speed up image retrieval
- Error messages can now be displayed in a requester if MUI is not finished initialising the app
- Now displays an informative error message if you are attempting to generate an image when you have insufficient credits in your OpenAI API account
- M68k version compiled with extra optimisations for faster code
- Fixed bug where AmigaGPT would get stuck after sending multiple chat messages
- Fixed the bsdsocket.library error that appears on MorphOS if the app is closed before it has finished loading
- Fix crash when toggling speech enabled in menu
- Fix crash when generating an image when OpenAI reports an error

## 2.5.0 (2025-04-25)

- Added an ARexx menu to allow you to run your own scripts. ARexx is not yet implemented in MorphOS so you can try installing the 68k version of ARexx and see if that works.
- Added sample scripts
- Status bar background colour adjusted to improve readability
- Latest OpenAI chat models added (gpt-4.1, o3, o4-mini)
- The menu has been rewritten and now gets built dynamically. It will now automatically create menu options for all the available models. This will make adding new models much easier and quicker

## 2.4.0 (2025-04-05)

- ARexx port added to allow issuing AmigaGPT commands from another app or from a script
- Fixed bug where the About AmigaGPT window wouldn't display all the text correctly
- Latest OpenAI chat models added
- Latest OpenAI text to speech models added
- Latest OpenAI text to speech voices added
- Can now give instructions to the OpenAI voice to change how it sounds (only works for the gpt-4o-mini-tts model)
- Chat now uses the new Responses endpoint instead of the Chat Completions one to allow for future expansion

## 2.3.1 (2025-03-30)

- Fix broken error requester in MorphOS
- Fix incorrect language name for english-british for OS4
- Running the "Version" command on the AmigaGPT executable will now report the version and build date
- The installer in MorphOS will now properly ask where you want to install the readme

## 2.3.0 (2025-03-23)

- New installer for easy and customisable installation
- Localisation support for 22 languages
- Removed close gadgets from API Key and Chat System requester windows
- Chat system requester text entry field is now multiline and supports clipboard

## 2.2.0 (2025-01-04)

- Support for ApolloOS
- Image preview now works in MorphOS and in MUI 3 (AmigaGPT now has MCC_Guigfx as a prerequisite)
- Replace the chat output sections from MCC_TextEditor class to MCC_NFloattext for improved memory usage and compatibility with ApolloOS
- Improved encoding to and from UTF-8 with enhanced character lookalike remapping (AmigaGPT now has codesets.library as a prerequisite)
- Fix crash when the first data chunk received from OpenAI is smaller than the HTTP header
- Fix crash when OpenAI sends a blank content string

## 2.1.2 (2024-12-31)

- Fix bug where not all messages were being saved in a new conversation
- Fix menu items not being selectable

## 2.1.1 (2024-12-30)

- Add more clear error message when the user's installed AmiSSL version is too old

## 2.1.0 (2024-12-28)

- Add proxy server support

## 2.0.2 (2024-12-24)

- Fix memory leaks
- Make more resiliant to connection errors or disconnections

## 2.0.1 (2024-12-23)

- Fix a bug preventing you being able to create a new image after making one
- Fix crash when receiving an error generating an image

## 2.0.0 (2024-12-22)

- Completely replace ReAction with MUI 5 with backwards compatibility with MUI 3
- Now fully compatible with AmigaOS 3.1
- New native version for MorphOS
- Enhanced "About AmigaGPT" panel
- New "About MUI" panel
- Fix memory leak when receiving messages
- View instant preview of generated images (MUI 5 only and not supported in MorphOS)
- Updated to the latest OpenAI models

## 1.6.3 (2024-09-18)

- Fixed error receiving chat messages

## 1.6.2 (2024-09-01)

- Increase max OpenAI API key size to 256 characters

## 1.6.1 (2024-08-31)

- Default to speech disabled
- Fix crash when trying to close speech system when it failed to initialise

## 1.6.0 (2024-08-10)

- Add the GPT4o Mini model
- Remove the unused Accent menu item from OS4 version
- Fix bug where some voice menu items in the OS4 version fail to display a checkmark after you select it

## 1.5.1 (2024-06-26)

- Send button no longer appears after image creation
- Create image button stays disabled after image creation. Select new image to create a new one
- Updated to latest json-c

## 1.5.0 (2024-05-18)

- Support for OpenAI text to speech voices. Fast internet connection recommended
- Updated to latest OpenAI chat models (including GPT-4o which will only work for ChatGPT Plus subscribers for now)
- Removed debug symbols for faster and smaller executable

## 1.4.6 (2024-04-13)

- Send button no longer appears in top left corner after closing a generated image for Amiga 3.X

## 1.4.5 (2024-04-06)

- Fixes the stack size calculation so it will no longer warn you that the stack size is too small if it really isn't

## 1.4.4 (2024-03-23)

- Fixes a bug in chat mode in AmigaOS 3.X where the send button stays disabled after an error occurs
- Change the stack size in the program .info file to the recommended 32768 bytes
- Fix bug where accents were not loaded properly resulting in the speech to fail

## 1.4.3 (2024-02-08)

- Updated to the latest OpenAI chat models
- Fix crash when config.json doesn't exist
- Fix crash when selecting the root menu items
- Adjust screen colours to enhance visibility
- Improved error handling for connection errors
- Use a stack cookie to set minimum stack size to 32768 bytes (AmigaOS 3.1.4 or higher required)
- Shows a warning if the stack size is smaller than 32768 bytes (AmigaOS 3 only)
- Send button no longer appears in corner of screen on image mode in 3.X after an error message is dismissed

## 1.4.2 (2024-01-31)

- Replaced clicktabs for AmigaOS 3.X since the version is too old and will crash. 3.X users can select the mode at startup.

## 1.4.1 (2024-01-22)

- Fixed bug where the prompt textbox remains in a readonly state when switching from image mode to chat mode

## 1.4.0 (2024-01-21)

- AI image generation! Switch between chat and image generation mode by clicking the tabs at the top of the screen
- Colour tweaks
- Status bar shows more information and colours
- About screen can now be dismissed with any key press or mouse click
- The OpenAI API key requester now populates with the existing key if it exists so you can more easily find and fix typos in the key
- Added the ability to set the chat system (new "Chat System" menu item)
- Fix crash when having a conversation consisting of a few messages

## 1.3.1 (2023-12-02)

- Fix pasting into chat input textbox (AmigaOS 3.2 and 4.1 only for now)
- Remove trailing newline from user messages
- Enclose all user lines in asterisks to make bold
- Fix memory leak that was present in downloading responses from OpenAI

## 1.3.0 (2023-11-14)

- Native PPC version for AmigaOS 4.1
- Scrollbar works again
- Improve response handling from OpenAI preventing a crash
- Latest OpenAI models added

## 1.2.1 (2023-11-04)

- Now fully backwards compatible with Cloanto's AmigaOS 3.X (included in Amiga Forever)

## 1.2.0 (2023-10-04)

- Now fully backwards compatible with AmigaOS 3.9

## 1.1.0 (2023-07-22)

- Now supports characters from Western languages other than English
- Blank responses from OpenAI no longer cause a crash

## 1.0.1 (2023-06-24)

- After setting UI font, the font is now applied fully without needing to restart the app

## 1.0 (2023-06-17)

- Initial release

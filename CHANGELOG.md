# Changelog

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

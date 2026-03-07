# AmigaGPT

AmigaGPT is a versatile ChatGPT client for AmigaOS 3.x, 4.1 and MorphOS. This powerful tool brings the capabilities of OpenAI's GPT to your Amiga system, enabling text generation, question answering, and creative exploration. AmigaGPT can also generate stunning images and includes support for speech output, making it easier than ever to interact with AI on your Amiga. Designed to integrate seamlessly with your system, AmigaGPT delivers modern AI technology while embracing the timeless Amiga experience.

<img width="953" alt="Screenshot 2023-06-15 at 10 26 38 pm" src="https://github.com/sacredbanana/AmigaGPT/assets/6903516/ca5e0db3-4e37-4ea9-a6ac-9fff2d5c195a">


## Features

- ### State-of-the-art language and image models

**AmigaGPT** uses the o1, o3, o4, GPT-5, GPT-4o, GPT-4 and GPT-3.5 models developed by OpenAI to generate coherent, context-aware responses to your input.

- ### AI Image Generation with DALL-E 2, DALL-E 3 and GPT-Image-1

**AmigaGPT** can access the powerful OpenAI models to generate images from a prompt. You can view and save the images right inside the app.

- ### Seamless integration with AmigaOS and MorphOS

**AmigaGPT** takes full advantage of the MUI framework to provide a smooth, native user experience that is responsive and easy to use.

- ### UI customisation

You can customise the look and feel of the application, including the ability to choose the fonts, colours and a choice of opening in the Workbench screen or a custom screen.

- ### Speech capability

**AmigaGPT** has support for OpenAI's high quality 16 bit voices. For AmigaOS 3, **AmigaGPT** can use the Amiga's speech synthesis capability to read the generated text aloud with support for switching between the old Workbench 1.x **v34** and the Workbench 2.0 **v37** speech synthesisers. For AmigaOS 4.1, it has support for `flite.device`.

- ### Shell Tool

**AmigaGPT** can let ChatGPT execute AmigaDOS commands directly on your Amiga. Enable it from the **Chat** menu and ask the AI to list files, create directories, show file contents, and more. All commands require your approval before execution for security.

- ### ARexx integration

**AmigaGPT** includes ARexx support, allowing you to control the application programmatically from external scripts or other applications. You can send prompts, generate images, and utilize speech synthesis through simple ARexx commands.

## System Requirements

Ensure you have the necessary system requirements:
- An OCS/ECS/AGA **Amiga** or a PowerPC machine capable of running MorphOS
- **AmigaOS 3.1** or higher, **AmigaOS 4.1** or **MorphOS**
- Motorola 68020 or higher CPU or PowerPC for AmigaOS 4/MorphOS
- 8MB Fast RAM minimum, 16MB or higher recommended
- Internet access using a TCP/IP stack such as **Roadshow** (<http://roadshow.apc-tcp.de/index-en.php>)
- For AmigaOS 3 & 4: **AmiSSL 5.18** or higher (<https://aminet.net/package/util/libs/AmiSSL-v5-OS3>) for OS3 and (<https://aminet.net/package/util/libs/AmiSSL-v5-OS4>) for OS4
- **MUI 3** minimum but **MUI 5** recommended for all features. MUI 5:(<https://github.com/amiga-mui/muidev/releases>) or MUI 3.9: (<https://github.com/amiga-mui/muidev/releases/download/MUI-3.9-2015R1/MUI-3.9-2015R1-os3.lha>)
- **render.library 40.08** or higher (<https://aminet.net/package/dev/misc/renderlib>)
- **guigfx.library 20.0** or higher (<https://aminet.net/dev/misc/guigfxlib.lha>) (FPU) or (<https://aminet.net/dev/misc/guigfxlib_nofpu.lha>) 
- **codesets.library 6.22** or higher (<http://aminet.net/package/util/libs/codesets-6.22>)
- **MCC_Guigfx** MUI custom class for displaying images (<http://aminet.net/package/dev/mui/MCC_Guigfx>)
- **MCC_NList** MUI custom class for lists (<http://aminet.net/package/dev/mui/MCC_NList-0.128>)
- **MCC_TextEditor 15.56** or higher (<https://aminet.net/dev/mui/MCC_TextEditor-15.56.lha>)
- **A PNG datatype** Any will do, but here is one (<https://aminet.net/util/dtype/vPNGdt.lha>)
- An **OpenAI account** with an active **API key**
- *Optional*: **AmigaOS 3 only**: A copy of the **Workbench 1.x** disk to install `narrator.device` **v34** and a copy of the **Workbench 2.0** disk to install `narrator.device` **v37**
- *Optional*: **AmigaOS 4 only**: **Flite device** (<http://aminet.net/package/mus/misc/flite_device>)
- *Optional*: For OpenAI voices, ***AHI*** needs to be installed
 (<http://aminet.net/package/driver/audio/ahiusr_4.18>)

## Installation
* For AmigaOS 3 & 4, Install AmiSSL and a TCP/IP stack if not already done so
* Download and install MUI. Version 5 recommended, version 3 minimum. Reboot.
* Download and install codesets.library, render.library, MCC_Guigfx, MCC_NList and MCC_TextEditor
* Download the latest release of **AmigaGPT**
* Extract the `amigagpt.lha` archive to a temporary location
* Run the provided installer

## *Optional steps to enable speech functionality*
AmigaGPT supports reading the output aloud. How AmigaGPT does this depends on whether you are using AmigaOS 3 or 4. Or for OpenAI voices, this works on every system.

### Installing AHI for OpenAI voices
If your OS does not come with AHI installed, you can get it from
 <https://aminet.net/package/driver/audio/ahiusr_4.18>

### AmigaOS 3 ###
**AmigaGPT** supports reading the output aloud. This requires a file called `narrator.device` which cannot be included with **AmigaGPT** because it is still under copyright. Therefore, you must copy this file legally from your Workbench disks so that **AmigaGPT** will be able to synthesise speech. There are 2 versions of `narrator.device` supported, **v34** and **v37**.

**v34** is the original version that came with Workbench 1.x. **v37** was an updated version included with Workbench 2.0.x. It has more features and sounds more natural, however it does sound quite different which is why **AmigaGPT** supports you installing both versions and your choice of version to be used can be selected in the **Speech** menu in the app.

Regardless of which version of `narrator.device` you choose to install (or both), **AmigaGPT** requires that you install the free third party `translator.library` **v43**. This works with both versions of `narrator.device`.

### Installing `translator.library` **v43**
Since `translator.library` **v43** is not available as a standalone install, you will need to install **v42** and then patch it to **v43**.
* Download <http://aminet.net/util/libs/translator42.lha> and extract the archive to any convenient location on your Amiga such as `RAM:`
* Navigate to that directory and double click the `Install` program
* Run the installer using all the default settings
* Download <http://aminet.net/util/libs/Tran43pch.lha> and once again extract it to a location of your choice
* Navigate to that directory and double click the `Install` program
* Run the installer using all the default settings
* Reboot your Amiga - It will not work until the system is restarted


### Installing `narrator.device` **v34**
* Insert your Workbench 1.x disk and copy `df0:devs/narrator.device` to `{AmigaGPTProgramDirectory}/devs/speech/34`

### Installing `narrator.device` **v37**
* Insert your Workbench 2.0.x (you cannot use 2.1 because the speech libraries were removed after version 2.0.4) disk and copy `df0:devs/narrator.device` to `{AmigaGPTProgramDirectory}/devs/speech/37`

### AmigaOS 4
* AmigaGPT for AmigaOS 4 uses the Flite device to provide speech synthesis. Download it from <http://aminet.net/package/mus/misc/flite_device>.
* Extract the archive and run the installer

## Launching **AmigaGPT**
* Launch the application by double-clicking the AmigaGPT icon
* You may also launch the app in the command line but before you do, run the command `STACK 32768` to give the program 32kb of stack since the default stack size for apps launched from the shell is 4kb and this is not enough for **AmigaGPT** and will cause random crashes due to stack overflow. This is not required when you launch the app by double clicking the icon since the stack size is saved in the icon

## Usage

When launching for the first time you will need to enter your OpenAI API key before you can start chatting. If you haven't already done so, create an OpenAI account and navigate to <https://platform.openai.com/account/api-keys> to generate an API key for use with **AmigaGPT**.

There are 2 main modes of operation: Chat and Image Generation. You can switch between them via the tabs in the top left corner.

### Chat

When the app has opened, you are presented with a text input box. You can type any prompt into this box and press "**Send**" to see the GPT model's response. The generated text appears in the box above the input. You can choose to have this text read aloud using the "**Speech**" menu option. To choose the chat model and provider, open "**Chat Provider Settings**" from the "**Chat**" menu.

To the left of the chat box is a conversation list which you can use to go to another saved conversation. New conversations can be created with the "**New chat**" button and conversations can be removed with the "**Delete chat**" button.

You can also chat to AmigaGPT within the Shell as long as either AmigaGPT or the AmigaGPTD daemon are running. Just type `askgpt` into the shell and it will ask you what you want to ask. Or just ask right away with the command `askgpt What is the capital of Australia?`. To activate a mini GUI for AskGPT, just add `GUI` as the first argument to AskGPT.

### Image Generation

To generate images, open "**Image Provider Settings**" from the "**Image**" menu to choose your provider and model, then type your prompt in the text box and hit the "**Create Image**" button. When it has been downloaded to your Amiga, you are then able to open the image to your desired scale, or save a copy of the file to a new location on your Amiga. Do note however that AmigaGPT will automatially save all your generated images until you delete them. This is just in case you would like to create a copy elsewhere.

### Chat and Image Provider Settings

AmigaGPT supports multiple AI providers. Open "**Chat Provider Settings**" from the "**Chat**" menu or "**Image Provider Settings**" from the "**Image**" menu.

The provider list includes built-in entries for **OpenAI**, **Google Gemini**, **xAI Grok** and **Anthropic Claude** (chat only); these cannot be removed. For built-in providers you can set the API key and choose the model; host, port and other connection settings are fixed. Known models are shown for each built-in; for OpenAI you can also use "Fetch Models" to refresh the list. You can add custom provider profiles (e.g. LM Studio) with templates and full control over all settings. Custom profile names cannot match built-in provider names.

When you switch to another profile without saving, AmigaGPT will ask whether to save your changes (Yes, No or Cancel).

### General

The "**Project**" menu includes an "**About**" option, which displays information about the program.

In the "**Edit**" menu, you'll find basic text editing commands like **Cut**, **Copy**, **Paste** and **Clear**.

The "**View**" menu allows you to change the appearance of the app.

AmigaGPT can run on its own screen or in Workbench. To configure this, open the **MUI Settings** menu item from the **View** menu and configure it in the **Screen** or **System** menu in the **MUI Setings** panel.

If you open in a new screen you have the ability to create a screen for the app to open in. **AmigaGPT** supports anything from 320x200 all the way up to 4k resolution if using a video card for RTG. Bear in mind text will appear very tiny in resolutions above 1080p so you may want to increase the font size settings from the MUI settings in the View menu when the app opens.

The "**Connection**" menu allows you to connect via a proxy server. It supports both HTTP and HTTPS proxy servers but if you use an unecrypted HTTP proxy server you can improve the performance of AmigaGPT by removing the need for the encryption of the OpenAI traffic to be done on the system running AmigaGPT. For an easy proxy server you can run on your local network you can try out <https://mitmproxy.org>

### ARexx Support

**AmigaGPT** now includes ARexx support, allowing you to control the application from external ARexx scripts or other applications. This powerful feature enables seamless integration with your Amiga workflow and automation of repetitive tasks.

Note: MorphOS does not yet have a complete ARexx implementation. You must install the 68k version of ARexx on MorphOS to be able to use ARexx with AmigaGPT.

AmigaGPT also comes with a companion daemon app called AmigaGPTD. You may run this in the shell. The daemon will respond to the same ARexx commands as the main app so you do not need to launch the main AmigaGPT app.

The daemon maintains conversation history in T: so it remembers context across multiple ARexx calls until the next reboot. This allows for follow-up questions and multi-turn conversations. Use the `NEWCHAT` ARexx command to clear the conversation history and start fresh.

Please note: AmigaGPT and AmigaGPTD listen on 2 differnt ARexx ports so adjust your scrips accordingly. AmigaGPT listens on **AMIGAGPT** and AmigaGPTD listens on **AMIGAGPTD**.

The following ARexx commands are available:

#### SENDMESSAGE
Sends a message using the selected chat profile and returns the response.
```
SENDMESSAGE PR=PROFILE/K,M=MODEL/K,S=SYSTEM/K,SF=SYSTEMFILE/K,K=APIKEY/K,W=WEBSEARCH/S,P=PROMPT/F
```
- `PR=PROFILE` - Optional, the chat profile to use. This can be a built-in profile such as OpenAI, Google Gemini, xAI Grok or Anthropic Claude, or a saved custom chat profile name. Use `LISTPROFILES` to see available profiles. If omitted, the active chat profile from AmigaGPT config is used
- `M=MODEL` - Optional, the chat model to use. Use `LISTMODELS PR=<profile>` to inspect the models for a profile. If omitted, the selected profile's model is used
- `S=SYSTEM` - Optional, system message to include
- `SF=SYSTEMFILE` - Optional, file containing the system message to include. If both `SF` and `S` are provided, the effective system message is the file contents concatenated with `S`.
- `K=APIKEY` - Optional, API key override. If omitted, the selected profile's API key is used
- `W=WEBSEARCH` - Optional, enable web search. If omitted, the selected profile's setting is used. If provided, it forces web search on even when the selected profile has it disabled
- `P=PROMPT` - Required, the prompt or question to send

#### CREATEIMAGE
Generates an image using the specified model.
```
CREATEIMAGE PR=PROFILE/K,M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/F
```
- `PR=PROFILE` - Optional, the image profile to use. This can be a built-in profile such as OpenAI, Google Gemini or xAI Grok, or a saved custom image profile name. Use `LISTPROFILES` to see available profiles. If omitted, the active image profile from AmigaGPT config is used
- `M=MODEL` - Optional, the image model to use. Use `LISTMODELS PR=<profile>` to inspect the models for a profile. If omitted, the selected profile's model is used
- `S=SIZE` - Optional, image size (use LISTIMAGESIZES to see available sizes). Default is 1024x1024. Note: some providers may ignore unsupported parameters (e.g. xAI currently does not support `size`) - see [xAI Image Generations](https://docs.x.ai/docs/guides/image-generations) and [Gemini OpenAI compatibility](https://ai.google.dev/gemini-api/docs/openai)
- `K=APIKEY` - Optional, API key override. If omitted, the selected profile's API key is used
- `D=DESTINATION` - Optional, the path where the image will be saved. Default is the creation of a temporary file. The destination will be the returned string from this function
- `P=PROMPT` - Required, description of the image to generate

#### SPEAKTEXT
Uses text-to-speech to speak the specified text.
```
SPEAKTEXT PR=PROFILE/K,M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,O=OUTPUT/K,F=FORMAT/K,P=PROMPT/F
```
- `PR=PROFILE` - Optional, the speech profile to use. This can be a built-in speech system name or a saved custom speech profile name. Use `LISTPROFILES` to see available profiles. If omitted, the active speech profile from AmigaGPT config is used
- `M=MODEL` - Optional, the OpenAI voice model to use. Use `LISTMODELS PR=<profile>` to inspect the models for a speech profile. If omitted, the selected speech profile's model is used
- `V=VOICE` - Optional voice override (use `LISTVOICES` to see available voices). For OpenAI speech profiles, this selects the OpenAI voice. For Workbench speech profiles, `male`, `female`, `male robot`, and `female robot` override the profile's narrator sex/mode. If omitted, the selected speech profile's voice settings are used
- `I=INSTRUCTIONS` - Optional, OpenAI voice instructions override
- `K=APIKEY` - Optional, API key override. If omitted, the selected speech profile's API key is used
- `O=OUTPUT` - Optional, file to write audio data to instead of playing it
- `F=FORMAT` - Optional, format of output file (use LISTAUDIOFORMATS to see available formats). Default is MP3 and ignored if OUTPUT is not provided
- `P=PROMPT` - Required, the text to speak

#### NEWCHAT
Clears the daemon's conversation history and starts a new chat. This is useful when you want to start a fresh conversation without context from previous messages.
```
NEWCHAT
```
This command takes no parameters and returns "Conversation history cleared" on success.

#### List Commands
#### LISTMODELS
Fetches the models available from the server for a profile. If `PR` is omitted, it fetches model sections for the active chat, image and speech profiles.

```
LISTMODELS PR=PROFILE/K
```
- `PR=PROFILE` - Optional, the profile to inspect. The same name can match chat, image and speech profiles, and `LISTMODELS` will include a section for each matching subsystem. Built-in aliases such as `ChatGPT`, `Gemini`, `Grok` and `Claude` are also accepted
- `LISTMODELS` queries the profile's configured server where possible, so the results reflect what that server currently exposes rather than only AmigaGPT's built-in known model lists

#### LISTAUDIOFORMATS
Lists all available audio formats for saving audio files
```
LISTAUDIOFORMATS
```

#### LISTPROFILES
Lists all available chat, image and speech profiles.
```
LISTPROFILES
```

#### LISTIMAGESIZES
Lists all available image sizes
```
LISTIMAGESIZES
```

#### LISTVOICES
Lists the available TTS voices for a speech profile. If `PR` is omitted, it uses the active speech profile from AmigaGPT config.
```
LISTVOICES PR=PROFILE/K
```
- `PR=PROFILE` - Optional, the speech profile to inspect. If the resolved profile uses ElevenLabs, `LISTVOICES` fetches the available voices from ElevenLabs. For Workbench speech profiles, it returns `male`, `female`, `male robot`, and `female robot`

#### Help (?)
Displays a list of available commands
```
?
```

#### Example ARexx Script
```rexx
/* Simple ARexx script for AmigaGPT */
OPTIONS RESULTS
ADDRESS 'AMIGAGPT'

/* Send a message to GPT and get a response */
'SENDMESSAGE P=What is the capital of France?'
SAY RESULT

/* Generate an image */
'CREATEIMAGE P=A beautiful Amiga computer on a desk'
SAY 'Image saved to:' RESULT

/* Speak some text */
'SPEAKTEXT P=Hello from ARexx!'

EXIT
```

The **ARexx** menu allows you to import and run ARexx scripts. Also, selecting **Arexx Shell** will open a shell and put all ARexx scripts in trace mode so you can run the scripts line by line for debugging purposes. Select **Arexx Shell** once again to turn off trace mode and close the shell.

## Models

For the latest information on the models you can use in **AmigaGPT**, please refer to OpenAI's documentation at <https://platform.openai.com/docs/models>

## Developing
You can either compile the code natively or with the Docker container.

### Native

#### Building the AmigaOS 3 app
If you would like to build this project from source you will need Bebbo's **amiga-gcc** toolchain here <https://github.com/bebbo/amiga-gcc>

Once installed, get the required other SDK's (AmiSSL, Translator, json-c) from <https://github.com/sacredbanana/AmigaSDK-gcc> and put these in your Amiga dev environment created in the above step.

#### Building the AmigaOS 4 app
Get this toolchain set up <https://github.com/sba1/adtools>

Once installed, get the required other SDK's (AmiSSL, Translator, json-c) from <https://github.com/sacredbanana/AmigaSDK-gcc> and put these in your Amiga dev environment created in the above step.

### Docker
You may use pre-prepared Docker images that are able to compile both the AmigaOS 3 and AmigaOS 4 versions of the app.

Just install Docker on your machine and run the `build_os3.sh` or `build_os4.sh` scripts depending on which version of the app you want to build. If you want to perform a clean build, you can set the environment variable `CLEAN=1` for example you can run `CLEAN=1 ./build_os3.sh`.

The build app will be saved to the `/out` directory.

## License

**AmigaGPT** is licensed under the MIT License.

## Contributing

We welcome contributions to **AmigaGPT**! If you have a bug to report, a feature to suggest, or a change you'd like to make to the code, please open a new issue or submit a pull request.

## Contributors
### Code
- **Cameron Armstrong (sacredbanana/Nightfox)** <https://github.com/sacredbanana/>

### Art
- **Mauricio Sandoval** - Icon design

### Translations
- **Mauricio Sandoval** - Spanish
- **Tobias Baeumer** - German

## Special Thanks
- **Bebbo** for creating the Amiga GCC toolchain <https://github.com/bebbo>
- **OpenAI** for making this all possible <https://openai.com>
- **EAB** and everyone in it for answering my questions <https://eab.abime.net/>
- **Ján Zahurančík** for all the thorough testing, bundling AmigaGPT into AmiKit and for all the moral support <https://www.amikit.amiga.sk>
- **CoffinOS** for bundling AmigaGPT into CoffinOS <https://getcoffin.net>
- **Amiga Future Magazine** for reviewing AmigaGPT and publishing several of its updates in the News from Aminet section <https://www.amigafuture.de/>
- **WhatIFF? Magaine** for reviewing AmigaGPT and interviewing me in issue 14 <https://www.whatiff.info>
- **Dan Wood** for reviewing AmigaGPT on his YouTube channel <https://www.youtube.com/watch?v=-OA28r8Up5U>
- **Proteque-CBN** for reviewing AmigaGPT on his YouTube channel <https://www.youtube.com/watch?v=t3q8HQ6wrnw>
- **AmigaBill** for covering AmigaGPT in the Amiga News section on his Twitch streams and allowing me to join his stream to promote it <https://www.twitch.tv/amigabill>
- **Les Docs** for making a video review and giving a tutorial on how to add support for the French accent <https://www.youtube.com/watch?v=BV5Fq1PresE>
- **amiga-news.de** for covering AmigaGPT news <http://amiga-news.de>

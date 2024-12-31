# AmigaGPT

AmigaGPT is a versatile ChatGPT client for AmigaOS 3.x, 4.1 and MorphOS. This powerful tool brings the capabilities of OpenAI’s GPT to your Amiga system, enabling text generation, question answering, and creative exploration. AmigaGPT can also generate stunning images using DALL-E and includes support for speech output, making it easier than ever to interact with AI on your Amiga. Designed to integrate seamlessly with your system, AmigaGPT delivers modern AI technology while embracing the timeless Amiga experience.

<img width="953" alt="Screenshot 2023-06-15 at 10 26 38 pm" src="https://github.com/sacredbanana/AmigaGPT/assets/6903516/ca5e0db3-4e37-4ea9-a6ac-9fff2d5c195a">


## Features

- ### State-of-the-art language model

**AmigaGPT** uses the o1, GPT-4o, GPT-4 and GPT-3.5 models developed by OpenAI to generate coherent, context-aware responses to your input.

- ### AI Image Generation with DALL-E 2 and DALL-E 3

**AmigaGPT** can access the powerful DALL-E models to generate images from a prompt. You can view and save the images right inside the app.

- ### Seamless integration with AmigaOS

**AmigaGPT** takes full advantage of the MUI framework to provide a smooth, native user experience that is responsive and easy to use.

- ### UI customisation

You can customise the look and feel of the application, including the ability to choose the fonts, colours and a choice of opening in the Workbench screen or a custom screen.

- ### Speech capability

**AmigaGPT** has support for OpenAI's high quality 16 bit voices. For AmigaOS 3, **AmigaGPT** can use the Amiga's speech synthesis capability to read the generated text aloud with support for switching between the old Workbench 1.x **v34** and the Workbench 2.0 **v37** speech synthesisers. For AmigaOS 4.1, it has support for `flite.device`.

## System Requirements

Ensure you have the necessary system requirements:
- An OCS/ECS/AGA **Amiga** or a PowerPC machine capable of running MorphOS
- **AmigaOS 3.1** or higher, **AmigaOS 4.1** or **MorphOS**
- Motorola 68020 or higher CPU or PowerPC for AmigaOS 4/MorphOS
- Internet access using a TCP/IP stack such as **Roadshow** (<http://roadshow.apc-tcp.de/index-en.php>)
- For AmigaOS 3 & 4: **AmiSSL 5.18** or higher (<https://aminet.net/package/util/libs/AmiSSL-v5-OS3>) for OS3 and (<https://aminet.net/package/util/libs/AmiSSL-v5-OS4>) for OS4
- **MUI 3** minimum but **MUI 5** recommended for all features (<https://github.com/amiga-mui/muidev/releases>)
- **MCC_NList** MUI custom class for lists (<http://aminet.net/package/dev/mui/MCC_NList-0.128>)
- **MCC_TextEditor** MUI custom class for text editors (<http://aminet.net/package/dev/mui/MCC_TextEditor-15.56>)
- An **OpenAI account** with an active **API key**
- *Optional*: **AmigaOS 3 only**: A copy of the **Workbench 1.x** disk to install `narrator.device` **v34** and a copy of the **Workbench 2.0** disk to install `narrator.device` **v37**
- *Optional*: **AmigaOS 4 only**: **Flite device** (<http://aminet.net/package/mus/misc/flite_device>)
- *Optional*: For OpenAI voices, ***AHI*** needs to be installed
 (<http://aminet.net/package/driver/audio/ahiusr_4.18>)

## Installation
* For AmigaOS 3 & 4, Install AmiSSL and a TCP/IP stack if not already done so
* Download and install MUI. Version 5 recommended, version 3 minimum. Reboot.
* Download and install MCC_NList and MCC_TextEditor
* Download the latest release of **AmigaGPT**
* Extract the `amigagpt.lha` archive to your desired location

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

When launched, **AmigaGPT** presents you with a choice of opening the app in a new screen or opening in Workbench. If you open in a new screen you have the ability to create a screen for the app to open in. **AmigaGPT** supports anything from **320x200** all the way up to **4k** resolution if using a video card for RTG. Bear in mind text will appear very tiny in resolutions above **1080p** so you may want to increase the font size settings from the **View** menu when the app opens.

When launching for the first time you will need to enter your OpenAI API key before you can start chatting. If you haven't already done so, create an OpenAI account and navigate to <https://platform.openai.com/account/api-keys> to generate an API key for use with **AmigaGPT**.

There are 2 main modes of operation: Chat and Image Generation. You can switch between them via the tabs in the top left corner.

### Chat

When the app has opened, you are presented with a text input box. You can type any prompt into this box and press "**Send**" to see the GPT model's response. The generated text appears in the box above the input. You can choose to have this text read aloud using the "**Speech**" menu option. You can also select which model for OpenAI to use in the "**OpenAI**" menu option.

To the left of the chat box is a conversation list which you can use to go to another saved conversation. New conversations can be created with the "**New chat**" button and conversations can be removed with the "**Delete chat**" button.

### Image Generation

To generate images, simply select your desired image generation model from the "**OpenAI**" menu then type your prompt in the text box then hit the "**Create Image**" button. When it has been downloaded to your Amiga, you are then able to open the image to your desired scale, or save a copy of the file to a new location on your Amiga. Do note however that AmigaGPT will automatially save all your generated images until you delete them. This is just in case you would like to create a copy elsewhere.

### General

The "**Project**" menu includes an "**About**" option, which displays information about the program.

In the "**Edit**" menu, you'll find basic text editing commands like **Cut**, **Copy**, **Paste** and **Clear**.

The "**View**" menu allows you to change the appearance of the app.

The "**Connection**" menu allows you to connect via a proxy server. It supports both HTTP and HTTPS proxy servers but if you use an unecrypted HTTP proxy server you can improve the performance of AmigaGPT by removing the need for the encryption of the OpenAI traffic to be done on the system running AmigaGPT. For an easy proxy server you can run on your local network you can try out <https://mitmproxy.org>

## Chat Models

### GPT-4o

GPT-4o (“o” for “omni”) is our most advanced GPT model. It is multimodal (accepting text or image inputs and outputting text), and it has the same high intelligence as GPT-4 Turbo but is much more efficient—it generates text 2x faster and is 50% cheaper. Additionally, GPT-4o has the best vision and performance across non-English languages of any of our models. GPT-4o is available in the OpenAI API to paying customers.

| Model | Description | Context Window | Max Output Tokens | Training Data |
| ----- | ----------- | -------------- | ----------------- | ------------- |
| gpt-4o | Our high-intelligence flagship model for complex, multi-step tasks. GPT-4o is cheaper and faster than GPT-4 Turbo. Currently points to gpt-4o-2024-08-06. | 128,000 | 16,384 | Oct 2023 |
| gpt-4o-2024-11-20 | Latest gpt-4o snapshot from November 20th, 2024. | 128,000 | 16,384 | Oct 2023 |
| gpt-4o-2024-08-06 | First snapshot that supports Structured Outputs. gpt-4o currently points to this version. | 128,000 | 16,384 | Oct 2023 |
| gpt-4o-2024-05-13 | Original gpt-4o snapshot from May 13, 2024. | 128,000 | 4,096 | Oct 2023 |
| chatgpt-4o-latest | The chatgpt-4o-latest model version continuously points to the version of GPT-4o used in ChatGPT, and is updated frequently, when there are significant changes. | 128,000 | 16,384 | Oct 2023 |

---
### GPT-4o mini

GPT-4o mini (“o” for “omni”) is our most advanced model in the small models category, and our cheapest model yet. It is multimodal (accepting text or image inputs and outputting text), has higher intelligence than **gpt-3.5-turbo** but is just as fast. It is meant to be used for smaller tasks, including vision tasks.

>We recommend choosing **gpt-4o-mini** where you would have previously used **gpt-3.5-turbo** as this model is more capable and cheaper.

| Model | Description | Context Window | Max Output Tokens | Training Data |
| ----- | ----------- | -------------- | ----------------- | ------------- |
| gpt-4o-mini | Our affordable and intelligent small model for fast, lightweight tasks. GPT-4o mini is cheaper and more capable than GPT-3.5 Turbo. Currently points to gpt-4o-mini-2024-07-18. | 128,000 | 16,384 | Oct 2023 |
| gpt-4o-mini-2024-07-18 | gpt-4o-mini currently points to this version. | 128,000 | 16,384 | Oct 2023 |

---
### o1-preview and o1-mini

The o1 series of large language models are trained with reinforcement learning to perform complex reasoning. o1 models think before they answer, producing a long internal chain of thought before responding to the user.

There are two model types available today:

- **o1-preview**: reasoning model designed to solve hard problems across domains.
- **o1-mini**: faster and cheaper reasoning model particularly good at coding, math, and science.

| Model | Description | Context Window | Max Output Tokens | Training Data |
| ----- | ----------- | -------------- | ----------------- | ------------- |
| o1 | Points to the most recent snapshot of the o1 model: o1-2024-12-17 | 200,000 | 100,000 | Oct 2023 |
| o1-2024-12-17 | The latest o1 model | 200,000 | 100,000 | Oct 2023 |
| o1-preview | Points to the most recent snapshot of the o1 model: o1-preview-2024-09-12 | 128,000 | 32,768 | Oct 2023 |
| o1-preview-2024-09-12 | Latest o1 model snapshot | 128,000 | 32,768 | Oct 2023 |
| o1-mini | Points to the most recent o1-mini snapshot: o1-mini-2024-09-12 | 128,000 | 65,536 | Oct 2023 |
| o1-mini-2024-09-12 | Latest o1-mini model snapshot | 128,000 | 65,536 | Oct 2023 |

#### GPT-4 Turbo and GPT-4

GPT-4 is a large multimodal model (accepting text or image inputs and outputting text) that can solve difficult problems with greater accuracy than any of our previous models, thanks to its broader general knowledge and advanced reasoning capabilities.

For many basic tasks, the difference between GPT-4 and GPT-3.5 models is not significant. However, in more complex reasoning situations, GPT-4 is much more capable than any of our previous models.

| Model | Description | Context Window | Max Output Tokens | Training Data |
| ----- | ----------- | -------------- | ----------------- | ------------- |
| gpt-4-turbo | The latest GPT-4 Turbo model with vision capabilities. Vision requests can now use JSON mode and function calling. Currently points to gpt-4-turbo-2024-04-09. | 128,000 | 4,096 | Dec 2023 |
| gpt-4-turbo-2024-04-09 | GPT-4 Turbo with Vision model. Vision requests can now use JSON mode and function calling. gpt-4-turbo currently points to this version. | 128,000 | 4,096 | Dec 2023 |
| gpt-4-turbo-preview | GPT-4 Turbo preview model. Currently points to gpt-4-0125-preview. | 128,000 | 4,096 | Dec 2023 |
| gpt-4-0125-preview | GPT-4 Turbo preview model intended to reduce cases of “laziness” where the model doesn’t complete a task. | 128,000 | 4,096 | Dec 2023 |
| gpt-4-1106-preview | GPT-4 Turbo preview model featuring improved instruction following, JSON mode, reproducible outputs, parallel function calling, and more. This is a preview model. | 128,000 | 4,096 | Apr 2023 |
| gpt-4 | Currently points to gpt-4-0613. | 8,192 | 8,192 | Sep 2021 |
| gpt-4-0613 | Snapshot of gpt-4 from June 13th 2023 with improved function calling support. | 8,192 | 8,192 | Sep 2021 |
| gpt-4-0314 | Snapshot of gpt-4 from March 14th 2023. | 8,192 | 8,192 | Sep 2021 |

---
#### GPT-3.5 Turbo

GPT-3.5 Turbo models can understand and generate natural language or code and have been optimized for chat using the Chat Completions API but work well for non-chat tasks as well.

>As of July 2024, gpt-4o-mini should be used in place of gpt-3.5-turbo, as it is cheaper, more capable, multimodal, and just as fast. gpt-3.5-turbo is still available for use in the API.

| Model | Description | Max Tokens | Training Data |
| ----- | ----------- | ---------- | ------------- |
| gpt-3.5-turbo-0125 | The latest GPT-3.5 Turbo model with higher accuracy at responding in requested formats and a fix for a bug which caused a text encoding issue for non-English language function calls. | 16,385 | 4,096 | Sep 2021 |
| gpt-3.5-turbo | Currently points to gpt-3.5-turbo-0125. | 16,385 | 4,096 | Sep 2021 |
| gpt-3.5-turbo-1106 | GPT-3.5 Turbo model with improved instruction following, JSON mode, reproducible outputs, parallel function calling, and more. | 16,385 | 4,096 | Sep 2021 |

## Image Generation Models

DALL·E is a AI system that can create realistic images and art from a description in natural language. DALL·E 3 currently supports the ability, given a prompt, to create a new image with a specific size. DALL·E 2 also support the ability to edit an existing image, or create variations of a user provided image.

### DALL-E 3

The latest DALL·E model released in Nov 2023.

### DALL-E 2

The previous DALL·E model released in Nov 2022. The 2nd iteration of DALL·E with more realistic, accurate, and 4x greater resolution images than the original model.

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
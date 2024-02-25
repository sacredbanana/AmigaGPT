# AmigaGPT

AmigaGPT is a text and image generation program that runs on the classic AmigaOS. Utilising the power of the OpenAI's GPT-3 and GPT-4 architectures, this program brings state-of-the-art language modeling to your Amiga computer.

<img width="953" alt="Screenshot 2023-06-15 at 10 26 38 pm" src="https://github.com/sacredbanana/AmigaGPT/assets/6903516/ca5e0db3-4e37-4ea9-a6ac-9fff2d5c195a">


## Features

- ### State-of-the-art language model

**AmigaGPT** uses the GPT-4 architecture developed by OpenAI to generate coherent, context-aware responses to your input.

- ### AI Image Generation with DALL-E 2 and DALL-E 3

**AmigaGPT** can access the powerful DALL-E models to generate images from a prompt. You can view and save the images right inside the app.

- ### Seamless integration with AmigaOS

**AmigaGPT** takes full advantage of the latest AmigaOS 3.2 API to provide a smooth, native user experience without the need to have third party frameworks installed.

- ### UI customisation

You can customise the look and feel of the application, including the ability to choose the fonts and a choice of opening in the Workbench screen or a custom screen.

- ### Speech capability

**AmigaGPT** can use the Amiga's speech synthesis capability to read the generated text aloud with support for switching between the old Workbench 1.x **v34** and the Workbench 2.0 **v37** speech synthesisers.

## System Requirements

Ensure you have the necessary system requirements:
- An OCS/ECS/AGA **Amiga**
- **AmigaOS 3.9** and **AmigaOS 3.X included in Amiga Forever** supported but **AmigaOS 3.2** or higher required for best experience.  **AmigaOS 4.1** version also included.
- Motorola 68020 or higher CPU or PPC for AmigaOS 4
- Internet access using a TCP/IP stack such as **Roadshow** (http://roadshow.apc-tcp.de/index-en.php)
- **AmiSSL 5.9** or higher (http://aminet.net/util/libs/AmiSSL-5.9-OS3.lha)
- An **OpenAI account** with an active **API key**
- *Optional*: **AmigaOS 3 only**: A copy of the **Workbench 1.x** disk to install `narrator.device` **v34** and a copy of the **Workbench 2.0** disk to install `narrator.device` **v37**
- *Optional*: **AmigaOS 4 only**: **Flite device** (http://aminet.net/package/mus/misc/flite_device)

## Installation
* Install AmiSSL and a TCP/IP stack if not already done so
* Download the latest release of **AmigaGPT**
* Extract the `amigagpt.lha` archive to your desired location

## *Optional steps to enable speech functionality*
### AmigaOS 3 ###
**AmigaGPT** supports reading the output aloud. This requires a file called `narrator.device` which cannot be included with **AmigaGPT** because it is still under copyright. Therefore, you must copy this file legally from your Workbench disks so that **AmigaGPT** will be able to synthesise speech. There are 2 versions of `narrator.device` supported, **v34** and **v37**. 

**v34** is the original version that came with Workbench 1.x. **v37** was an updated version included with Workbench 2.0.x. It has more features and sounds more natural, however it does sound quite different which is why **AmigaGPT** supports you installing both versions and your choice of version to be used can be selected in the **Speech** menu in the app.

Regardless of which version of `narrator.device` you choose to install (or both), **AmigaGPT** requires that you install the free third party `translator.library` **v43**. This works with both versions of `narrator.device`.

### Installing `translator.library` **v43**
Since `translator.library` **v43** is not available as a standalone install, you will need to install **v42** and then patch it to **v43**.
* Download http://aminet.net/util/libs/translator42.lha and extract the archive to any convenient location on your Amiga such as `RAM:`
* Navigate to that directory and double click the `Install` program
* Run the installer using all the default settings
* Download http://aminet.net/util/libs/Tran43pch.lha and once again extract it to a location of your choice
* Navigate to that directory and double click the `Install` program
* Run the installer using all the default settings
* Reboot your Amiga - It will not work until the system is restarted


### Installing `narrator.device` **v34**
* Insert your Workbench 1.x disk and copy `df0:devs/narrator.device` to `{AmigaGPTProgramDirectory}/devs/speech/34`

### Installing `narrator.device` **v37**
* Insert your Workbench 2.0.x (you cannot use 2.1 because the speech libraries were removed after version 2.0.4) disk and copy `df0:devs/narrator.device` to `{AmigaGPTProgramDirectory}/devs/speech/37`

### AmigaOS 4
* AmigaGPT for AmigaOS 4 uses the Flite device to provide speech synthesis. Download it from http://aminet.net/package/mus/misc/flite_device.
* Extract the archive and run the installer

## Launching **AmigaGPT**
* Launch the application by double-clicking the AmigaGPT icon
* You may also launch the app in the command line but before you do, run the command `STACK 20000` to give the program 20kb of stack since the default stack size for apps launched from the shell is 4kb and this is not enough for **AmigaGPT** and will cause random crashes due to stack overflow. This is not required when you launch the app by double clicking the icon since the stack size is saved in the icon

## Usage

When launched, **AmigaGPT** presents you with a choice of opening the app in a new screen or opening in Workbench. If you open in a new screen you have the ability to create a screen for the app to open in. **AmigaGPT** supports anything from **320x200** all the way up to **4k** resolution if using a video card for RTG. Bear in mind text will appear very tiny in resolutions above **1080p** so you may want to increase the font size settings from the **View** menu when the app opens.

When launching for the first time you will need to enter your OpenAI API key before you can start chatting. If you haven't already done so, create an OpenAI account and navigate to https://platform.openai.com/account/api-keys to generate an API key for use with **AmigaGPT**.

There are 2 main modes of operation: Chat and Image Generation. You can switch between them via the tabs in the top left corner.

### Chat

When the app has opened, you are presented with a text input box. You can type any prompt into this box and press "**Send**" to see the GPT model's response. The generated text appears in the box above the input. You can choose to have this text read aloud using the "**Speech**" menu option. You can also select which model for OpenAI to use in the "**OpenAI**" menu option.

To the left of the chat box is a conversation list which you can use to go to another saved conversation. New conversations can be created with the "**New chat**" button and conversations can be removed with the "**Delete chat**" button.

### Image Generation

To generate images, simply select your desired image generation model from the "**OpenAI**" menu then type your prompt in the text box then hit the "**Create Image**" button. When it has been downloaded to your Amiga, you are then able to open the image to your desired scale, or save a copy of the file to a new location on your Amiga. Do note however that AmigaGPT will automatially save all your generated images until you delete them. This is just in case you would like to create a copy elsewhere.

### General

In the "**Edit**" menu, you'll find basic text editing commands like **Cut**, **Copy**, **Paste** and **Clear**. The "**View**" menu allows you to change the font used in the chat and the UI.

The "**Project**" menu includes an "**About**" option, which displays information about the program.

## Models

### **Image Generation**

### DALL-E 3

The latest DALL·E model released in Nov 2023.

### DALL-E 2

The previous DALL·E model released in Nov 2022. The 2nd iteration of DALL·E with more realistic, accurate, and 4x greater resolution images than the original model.

### **Chat**

### **GPT-4**
*GPT-4 is currently in a limited beta and only accessible to those who have been granted access. Please join the [waitlist](https://openai.com/waitlist/gpt-4) to get access.*

GPT-4 is a large multimodal model (accepting text inputs and emitting text outputs today, with image inputs coming in the future) that can solve difficult problems with greater accuracy than any of our previous models, thanks to its broader general knowledge and advanced reasoning capabilities. For many basic tasks, the difference between GPT-4 and GPT-3.5 models is not significant. However, in more complex reasoning situations, GPT-4 is much more capable than any of our previous models.

| Model | Description | Max Tokens | Training Data |
| ----- | ----------- | ---------- | ------------- |
| gpt-4-0125-preview | The latest GPT-4 model intended to reduce cases of “laziness” where the model doesn’t complete a task. Returns a maximum of 4,096 output tokens. | 128,000 | Up to Dec 2023 |
| gpt-4-turbo-preview | Currently points to gpt-4-0125-preview. | 128,000 | Up to Dec 2023 |
| gpt-4-1106-preview | GPT-4 Turbo model featuring improved instruction following, JSON mode, reproducible outputs, parallel function calling, and more. Returns a maximum of 4,096 output tokens. This is a preview model. | 128,000 | Up to Apr 2023 |
| gpt-4 | Currently points to gpt-4-0613. | 8,192 | Up to Sep 2021 |
| gpt-4-32k-0613 | Snapshot of gpt-4-32k from June 13th 2023 with improved function calling support. | 32,768 | Up to Sep 2021 |

### **GPT-3.5**

GPT-3.5 models can understand and generate natural language or code. Our most capable and cost effective model in the GPT-3.5 family is gpt-3.5-turbo which has been optimized for chat but works well for traditional completions tasks as well.

| Model | Description | Max Tokens | Training Data |
| ----- | ----------- | ---------- | ------------- |
| gpt-3.5-turbo-0125 | The latest GPT-3.5 Turbo model with higher accuracy at responding in requested formats and a fix for a bug which caused a text encoding issue for non-English language function calls. Returns a maximum of 4,096 output tokens. | 16,385 | Up to Sep 2021 |
| gpt-3.5-turbo | Currently points to gpt-3.5-turbo-0613. Will point to gpt-3.5-turbo-1106 starting Dec 11, 2023. | 4,096 | Up to Sep 2021 |
| gpt-3.5-turbo-1106 | The latest GPT-3.5 Turbo model with improved instruction following, JSON mode, reproducible outputs, parallel function calling, and more. Returns a maximum of 4,096 output tokens. | 16,385 | Up to Sep 2021 |

## Developing
You can either compile the code natively or with the Docker container.

### Native

#### Building the AmigaOS 3 app
If you would like to build this project from source you will need Bebbo's **amiga-gcc** toolchain here https://github.com/bebbo/amiga-gcc

Once installed, get the required other SDK's (AmiSSL, Translator, json-c) from https://github.com/sacredbanana/AmigaSDK-gcc and put these in your Amiga dev environment created in the above step.

#### Building the AmigaOS 4 app
Get this toolchain set up https://github.com/sba1/adtools

Once installed, get the required other SDK's (AmiSSL, Translator, json-c) from https://github.com/sacredbanana/AmigaSDK-gcc and put these in your Amiga dev environment created in the above step.

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
- **Cameron Armstrong (sacredbanana/Nightfox)** https://github.com/sacredbanana/

### Art
- **Mauricio Sandoval** - Icon design

## Special Thanks
- **Bebbo** for creating the Amiga GCC toolchain https://github.com/bebbo
- **OpenAI** for making this all possible https://openai.com
- **EAB** and everyone in it for answering my questions https://eab.abime.net/
- **Hyperion Entertainment** for bringing us AmigaOS 3.2 https://www.hyperion-entertainment.com

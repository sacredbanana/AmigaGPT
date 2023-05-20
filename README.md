# AmigaGPT

AmigaGPT is a text generation program that runs on the classic AmigaOS. Utilising the power of the OpenAI's GPT-3 and GPT-4 architectures, this program brings state-of-the-art language modeling to your Amiga computer.

## Features

- ### State-of-the-art language model

**AmigaGPT** uses the GPT-4 architecture developed by OpenAI to generate coherent, context-aware responses to your input.
Seamless integration with AmigaOS: AmigaGPT takes full advantage of the AmigaOS API to provide a smooth, native user experience.

- ### UI customisation

You can customise the look and feel of the application, including the ability to choose the fonts and a choice of opening in the Workbench screen or a custom screen.

- ### Speech capability

**AmigaGPT** can use the Amiga's speech synthesis capability to read the generated text aloud with support for switching between the old AmigaOS 1.0 and the AmigaOS 2.0 speech systems.

## Installation

Ensure you have the necessary system requirements:
- An **Amiga** with **AmigaOS 3.2** or higher
- Internet access using a TCP/IP stack such as **Roadshow** (http://roadshow.apc-tcp.de/index-en.php)
- **AmiSSL 5.8** or higher (http://aminet.net/util/libs/AmiSSL-5.8-OS3.lha)
- An **OpenAI account** with an active **API key**
- *Optional*: A copy of the **Workbench 1.x** disk to install the old speech library and a copy of the **Workbench 2.x** disk to install the new speech library


Download the latest release of **AmigaGPT**.
Extract the .lha archive to your desired location.
Launch the application by double-clicking the AmigaGPT icon.

## Usage

When launched, **AmigaGPT** presents you with a choice of opening the app in a new screen or opening in Workbench. If you open in a new screen you have the ability to create a screen for the app to open in. Anything from 320x200 all the way up to 4k (RTG) resolution is supported. 

When the app has opened, you are presented with a text input box. You can type any prompt into this box and press "**Send**" to see the GPT-4 model's response. The generated text appears in the box above the input. You can choose to have this text read aloud using the "**Speech**" menu option. You can also select which model for OpenAI to use in the "**OpenAI**" menu option.

To the left of the chat box is a conversation list which you can use to go to another saved conversation. New conversations can be created with the "**New chat**" button.

In the "**Edit**" menu, you'll find basic text editing commands like Cut, Copy, Paste, Clear, and Select All. The "**View**" menu allows you to change the font used in the chat and the UI.

The "**Project**" menu includes an "**About**" option, which displays information about the program, and a "**Preferences**" option where you can configure application-wide settings.

## Developing
If you would like to build this project from source you will need Bebbo's **amiga-gcc** toolchain here https://github.com/bebbo/amiga-gcc
Be sure to add `NDK=3.2` to the `make` options. Once your `amiga-gcc` dev environment is installed, you will need to download the AmiSSL SDK from http://aminet.net/util/libs/AmiSSL-5.8-SDK.lha and extract the header files into your `amiga-gcc` environment. By default this will be located at `/opt/amiga/m68k-amigaos/include`. Once this is set up all you will need to do to build the project is navigate to the project root directory and run the `make` command. The built app will appear in the `out` directory.

## License

**AmigaGPT** is licensed under the MIT License.

## Contributing

We welcome contributions to **AmigaGPT**! If you have a bug to report, a feature to suggest, or a change you'd like to make to the code, please open a new issue or submit a pull request.
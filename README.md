# Trespasser wav2cau Converter

### ☕️ [If you like what I do, consider buying me a coffee, it really helps!](https://ko-fi.com/keyofblues) ☕️

The videogame Trespasser includes subtitle support for spoken dialogue.
In the game, each voice line and its corresponding subtitle are packaged together inside .cau files. All of these .cau files are then bundled into a single Stream.tpa archive.

The wav2cau tool was developed to simplify subtitle translation. It allows you to generate new .cau files that contain both captions and audio.


In the txt folder of this repository, you will find caption files. You may edit the text as needed, but make sure to follow these rules:

- Keep the numeric value at the beginning of each line. This decimal number indicates the caption's timing.
- Ensure that all text uses ASCII characters only.


# How the .txt Caption Files Work

Each caption file follows a simple structure:

- Every line starts with a timing value, written as a decimal number.
- Each unit equals 200 ms. Example:
```
01; = 0.2 seconds
05; = 1 second
14; = 2.8 seconds
```
- The timing must be within the range 0-255.
- The timing value is separated from the caption text by a semicolon (;).
- Caption text must use ASCII characters only.
- You may insert empty lines between captions (including at the very beginning) to create pauses. Example:
```
10;
07;You talking to me?
12;
08;You talking to me?
14;
07;You talking to me?
19;
12;Then who the hell else are you talking... you talking to me?
06;
10;Well, I'm the only one here.
11;
15;Who the fuck do you think you're talking to?
06;
10;Oh, yeah?... eh.
07;
28;Okay... eh?!
```

Empty lines for pauses are optional and usually unnecessary.
For example, I only used one to create an initial pause in the VRADIO captions.
However, other in-game dialogue may still need timing adjustments.


# Build Instructions:

To compile this tool, use the following command:

`g++ -static -o wav2cau wav2cau.cpp`

To cross-compile for Windows (from Linux), use:

`x86_64-w64-mingw32-g++ -static -o wav2cau wav2cau.cpp`


# Usage:

Run the converter by specifying the input files:
```sh
$ ./wav2cau -c <input_caption_file> -w <input_wav_file> -o <output_cau_file> [OPTIONS]
```
```
Options:

  -c, --captionfile <captionfile.txt>   Specify the input caption ASCII txt file path and name.
  -w, --wavfile <wavfile.wav>           Specify the input wav file path and name.
  -o, --caufile <caufile.cau>           Specify the output cau file path and name.
  -q, --quiet                           Disable output messages.
  -d, --debug                           Enable output debug messages.
  -h, --help                            Show this help message and exit.
```


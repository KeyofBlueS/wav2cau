# Trespasser wav2cau Converter

### ☕️ [If you like what I do, consider buying me a coffee, it really helps!](https://ko-fi.com/keyofblues) ☕️

The videogame Trespasser includes subtitle support for spoken dialogue.
In the game, each voice line and its corresponding subtitle are packaged together inside .cau files. All of these .cau files are then bundled into a single Stream.tpa archive.

The wav2cau tool was developed to simplify subtitle translation. It allows you to generate new .cau files that contain both captions and audio.


In the txt folder of this repository, you will find caption files. You may edit the text as needed, but make sure to follow these rules:

1 - Keep the numeric value at the beginning of each line unchanged. This decimal number indicates the caption's timing.

2 - Ensure that all text uses ASCII characters only.


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
  -d, --debug					                  Enable output debug messages.
  -h, --help                            Show this help message and exit.
```


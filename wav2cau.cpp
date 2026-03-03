/*  Trespasser wav2cau Converter
	Copyright 2026 KeyofBlueS

	Trespasser wav2cau Converter is free software;
	you can redistribute it and/or modify it under the terms of the
	GNU General Public License as published by the Free Software Foundation;
	either version 3, or (at your option) any later version.
	See the file COPYING for more details.
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <cctype>
#include <getopt.h>
#include <unordered_map>

// WAV Header Info
struct WAVHeaderInfo {
	uint16_t audioFormat;
	uint16_t numChannels;
	uint32_t sampleRate;
	uint16_t blockAlign;
	uint32_t dataSize;
};

// Raw headers
unsigned char rawWBOR[16] = {
	0x57,0x42,0x4F,0x52,0x6E,0x00,0x00,0x00,
	0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

unsigned char rawBblock[8] = {
	0x57,0x42,0x4F,0x52,0x6E,0x00,0x00,0x00
};

// Little-endian uint32 reader
static uint32_t read_u32(const uint8_t* p)
{
	return uint32_t(p[0])
		 | (uint32_t(p[1]) << 8)
		 | (uint32_t(p[2]) << 16)
		 | (uint32_t(p[3]) << 24);
}

// Read entire file
std::vector<char> readFile(const std::string &path) {
	std::ifstream file(path,std::ios::binary);
	if(!file) throw std::runtime_error("Cannot open file: " + path);
	return std::vector<char>((std::istreambuf_iterator<char>(file)),
							 std::istreambuf_iterator<char>());
}

// Read WAV info
WAVHeaderInfo readWAVInfo(const std::vector<char> &wavData) {
	if(wavData.size()<44) throw std::runtime_error("Invalid WAV file");

	WAVHeaderInfo info{};
	info.audioFormat = *reinterpret_cast<const uint16_t*>(&wavData[20]);
	info.numChannels = *reinterpret_cast<const uint16_t*>(&wavData[22]);
	info.sampleRate = *reinterpret_cast<const uint32_t*>(&wavData[24]);
	info.blockAlign = *reinterpret_cast<const uint16_t*>(&wavData[32]);
	info.dataSize = *reinterpret_cast<const uint32_t*>(&wavData[40]);
	return info;
}

// Trim trailing newline characters
std::vector<char> trimCaptions(const std::vector<char>& captions)
{
	std::vector<char> trimmed = captions;
	while(!trimmed.empty() &&
		 (trimmed.back()=='\n' || trimmed.back()=='\r'))
	{
		trimmed.pop_back();
	}
	return trimmed;
}

// Count lines
size_t countLines(const std::vector<char> &captions) {
	if(captions.empty()) return 0;
	size_t lines=1;
	for(char c: captions) if(c=='\n') ++lines;
	return lines;
}

// Compute expanded caption length
uint32_t computeExpandedLength(const std::vector<char> &captions) {
	uint32_t len = 0;
	for(char c: captions){
		if(c=='\n') len +=5;
		else if(c!='\r' && c!=0) len +=1;
	}
	return len;
}

// Extract timings from caption TXT file (format: "number;text")
std::vector<uint8_t> extractCaptionTimingsFromTxt(std::vector<char>& captions,
                                                  bool debug)
{
    std::vector<uint8_t> timings;
    std::vector<bool> fallbackFlags;

    if (captions.empty())
        return timings;

    std::string content(captions.begin(), captions.end());
    std::stringstream ss(content);
    std::string line;
    std::string rebuilt;

    bool isFirstLine = true;

    while (std::getline(ss, line)) {

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        size_t semicolonPos = line.find(';');
        bool timingApplied = false;

        // Check numeric prefix
        if (semicolonPos != std::string::npos && semicolonPos > 0) {

            bool numeric = true;
            for (size_t i = 0; i < semicolonPos; ++i) {
                if (!std::isdigit(static_cast<unsigned char>(line[i]))) {
                    numeric = false;
                    break;
                }
            }

            if (numeric) {
                int value = std::stoi(line.substr(0, semicolonPos));

                if (value < 0) value = 0;
                if (value > 255) value = 255;

                timings.push_back(static_cast<uint8_t>(value));
                fallbackFlags.push_back(false);

                rebuilt += line.substr(semicolonPos + 1);
                rebuilt += '\n';

                timingApplied = true;
            }
        }

        // Apply fallback if needed
        if (!timingApplied) {

            uint8_t fallback = isFirstLine ? 10 : 14;

            timings.push_back(fallback);
            fallbackFlags.push_back(true);

            rebuilt += line;
            rebuilt += '\n';
        }

        isFirstLine = false;
    }

    if (debug) {
        std::cout << "[DEBUG] Timings extracted from caption file: ";
        for (size_t i = 0; i < timings.size(); ++i) {
            std::cout << int(timings[i]);
            if (fallbackFlags[i])
                std::cout << "(fallback)";
            std::cout << " ";
        }
        std::cout << std::endl;
    }

    captions.assign(rebuilt.begin(), rebuilt.end());
    return timings;
}

// WriteCAU
void writeCAU(const std::string &cauPath,
              const std::vector<char> &wavData,
              const WAVHeaderInfo &info,
              const std::vector<char> &captions,
              const std::vector<uint8_t> &timings)
{
    std::ofstream cau(cauPath, std::ios::binary);
    if(!cau) throw std::runtime_error("Cannot open CAU");

    unsigned char iszero = 0;

    // Trim trailing newlines
    std::vector<char> trimmed = trimCaptions(captions);

    uint32_t lineCount = countLines(trimmed);
    uint32_t expandedLen = computeExpandedLength(trimmed);
    uint32_t TLenCount = (trimmed.empty() ? 0 : expandedLen + 53);

    // --- Write header ---
    cau.write(reinterpret_cast<char*>(rawBblock), 8);

    uint32_t zero = 0;
    cau.write(reinterpret_cast<char*>(&TLenCount), 4);
    cau.write(reinterpret_cast<char*>(&zero), 4);
    cau.write(reinterpret_cast<const char*>(&info.dataSize), 4);
    cau.write(reinterpret_cast<const char*>(&info.dataSize), 4);
    cau.write(reinterpret_cast<const char*>(&info.sampleRate), 4);

    unsigned char tenval = 10;
    cau.write(reinterpret_cast<char*>(&tenval), 1);
    cau.write(reinterpret_cast<const char*>(&info.numChannels), 1);
    cau.write(reinterpret_cast<char*>(&iszero), 1);
    cau.write(reinterpret_cast<char*>(&iszero), 1);

    uint32_t posCaptions = 43; // caption block start at byte 43
    uint32_t sentenceLen = TLenCount - 40;
    cau.write(reinterpret_cast<char*>(&posCaptions), 4);
    cau.write(reinterpret_cast<char*>(&sentenceLen), 4);
    cau.write(reinterpret_cast<char*>(&lineCount), 4);

    // --- Write captions with timings ---
    cau.seekp(posCaptions, std::ios::beg);

    std::string content(trimmed.begin(), trimmed.end());
    std::stringstream ss(content);
    std::string line;
    size_t timingIndex = 0;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Use timing exactly as provided
        uint8_t t = 0x0E; // safety fallback only if mismatch occurs
        if (timingIndex < timings.size())
            t = timings[timingIndex++];

        // Write 5-byte timing block
        unsigned char timingBlock[5] = {0, t, 0, 0, 0};
        cau.write(reinterpret_cast<char*>(timingBlock), 5);

        // Write line characters
        for(char c : line)
            if(c != 0)
                cau.write(&c,1);
    }

    // Final 5 zero bytes terminator
    for(int i=0; i<5; i++)
        cau.put(0);

    // --- Write WAV data ---
    cau.write(&wavData[44], info.dataSize);
    cau.close();

    // --- Patch bytes ---
    std::fstream patch(cauPath,
                       std::ios::in | std::ios::out | std::ios::binary);
    if (!patch) throw std::runtime_error("Failed to reopen CAU for patching");

    // Patch byte 28
    patch.seekp(28, std::ios::beg);
    unsigned char valuea = 0x10;
    patch.write(reinterpret_cast<char*>(&valuea), 1);

    // Patch byte 32
    patch.seekp(32, std::ios::beg);
    unsigned char valueb = 0x24;
    patch.write(reinterpret_cast<char*>(&valueb), 1);

    patch.close();
}

enum class CaptionEncoding {
    CP1252,
    CP1251
};

// Windows-1252 special mapping table (Unicode → CP1252 byte)
static const std::unordered_map<uint32_t, uint8_t> unicodeToCP1252 = {
    {0x20AC, 0x80}, // €
    {0x201A, 0x82}, // ‚
    {0x0192, 0x83}, // ƒ
    {0x201E, 0x84}, // „
    {0x2026, 0x85}, // …
    {0x2020, 0x86}, // †
    {0x2021, 0x87}, // ‡
    {0x02C6, 0x88}, // ˆ
    {0x2030, 0x89}, // ‰
    {0x0160, 0x8A}, // Š
    {0x2039, 0x8B}, // ‹
    {0x0152, 0x8C}, // Œ
    {0x017D, 0x8E}, // Ž
    {0x2018, 0x91}, // ‘
    {0x2019, 0x92}, // ’
    {0x201C, 0x93}, // “
    {0x201D, 0x94}, // ”
    {0x2022, 0x95}, // •
    {0x2013, 0x96}, // –
    {0x2014, 0x97}, // —
    {0x02DC, 0x98}, // ˜
    {0x2122, 0x99}, // ™
    {0x0161, 0x9A}, // š
    {0x203A, 0x9B}, // ›
    {0x0153, 0x9C}, // œ
    {0x017E, 0x9E}, // ž
    {0x0178, 0x9F}  // Ÿ
};

// Unicode → CP1251
static const std::unordered_map<uint32_t, uint8_t> unicodeToCP1251 = {

    // Uppercase А–Я
    {0x0410, 0xC0},{0x0411, 0xC1},{0x0412, 0xC2},{0x0413, 0xC3},
    {0x0414, 0xC4},{0x0415, 0xC5},{0x0416, 0xC6},{0x0417, 0xC7},
    {0x0418, 0xC8},{0x0419, 0xC9},{0x041A, 0xCA},{0x041B, 0xCB},
    {0x041C, 0xCC},{0x041D, 0xCD},{0x041E, 0xCE},{0x041F, 0xCF},
    {0x0420, 0xD0},{0x0421, 0xD1},{0x0422, 0xD2},{0x0423, 0xD3},
    {0x0424, 0xD4},{0x0425, 0xD5},{0x0426, 0xD6},{0x0427, 0xD7},
    {0x0428, 0xD8},{0x0429, 0xD9},{0x042A, 0xDA},{0x042B, 0xDB},
    {0x042C, 0xDC},{0x042D, 0xDD},{0x042E, 0xDE},{0x042F, 0xDF},

    // Lowercase а–я
    {0x0430, 0xE0},{0x0431, 0xE1},{0x0432, 0xE2},{0x0433, 0xE3},
    {0x0434, 0xE4},{0x0435, 0xE5},{0x0436, 0xE6},{0x0437, 0xE7},
    {0x0438, 0xE8},{0x0439, 0xE9},{0x043A, 0xEA},{0x043B, 0xEB},
    {0x043C, 0xEC},{0x043D, 0xED},{0x043E, 0xEE},{0x043F, 0xEF},
    {0x0440, 0xF0},{0x0441, 0xF1},{0x0442, 0xF2},{0x0443, 0xF3},
    {0x0444, 0xF4},{0x0445, 0xF5},{0x0446, 0xF6},{0x0447, 0xF7},
    {0x0448, 0xF8},{0x0449, 0xF9},{0x044A, 0xFA},{0x044B, 0xFB},
    {0x044C, 0xFC},{0x044D, 0xFD},{0x044E, 0xFE},{0x044F, 0xFF},

    // Ё ё
    {0x0401, 0xA8},
    {0x0451, 0xB8}
};

// Decode one UTF-8 codepoint
static bool decodeUtf8(const std::string& s, size_t& i, uint32_t& codepoint)
{
    unsigned char c = static_cast<unsigned char>(s[i]);

    if (c < 0x80) {
        codepoint = c;
        i += 1;
        return true;
    }
    else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        codepoint = ((c & 0x1F) << 6) |
                    (static_cast<unsigned char>(s[i+1]) & 0x3F);
        i += 2;
        return true;
    }
    else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        codepoint = ((c & 0x0F) << 12) |
                    ((static_cast<unsigned char>(s[i+1]) & 0x3F) << 6) |
                    (static_cast<unsigned char>(s[i+2]) & 0x3F);
        i += 3;
        return true;
    }

    // Unsupported sequence
    i += 1;
    codepoint = '?';
    return false;
}

std::vector<char> utf8ToLegacy(const std::vector<char>& input,
                               CaptionEncoding encoding)
{
    std::vector<char> output;
    std::string utf8(input.begin(), input.end());

    for (size_t i = 0; i < utf8.size(); )
    {
        uint32_t codepoint = 0;
        decodeUtf8(utf8, i, codepoint);

        // ASCII always preserved
        if (codepoint <= 0x7F) {
            output.push_back(static_cast<char>(codepoint));
            continue;
        }

        // Direct Latin-1 range (for CP1252 only)
        if (encoding == CaptionEncoding::CP1252 &&
            codepoint >= 0xA0 && codepoint <= 0xFF)
        {
            output.push_back(static_cast<char>(codepoint));
            continue;
        }

        const std::unordered_map<uint32_t,uint8_t>* table = nullptr;

        if (encoding == CaptionEncoding::CP1252)
            table = &unicodeToCP1252;
        else
            table = &unicodeToCP1251;

        auto it = table->find(codepoint);
        if (it != table->end())
            output.push_back(static_cast<char>(it->second));
        else
            output.push_back('?');
    }

    return output;
}

// Function to validate the caption TXT file (strict ASCII text validation)
bool isValidAsciiTextFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);

	if (!file) {
		std::cerr << "* ERROR: Cannot open caption txt file." << std::endl;
		return false;
	}

	char ch;
	bool hasContent = false;

	while (file.get(ch)) {
		hasContent = true;

		unsigned char byte = static_cast<unsigned char>(ch);

		// Must be 7-bit ASCII
		if (byte > 0x7F) {
			return false;
		}

		// Allow printable characters (space to ~)
		if (byte >= 0x20 && byte <= 0x7E) {
			continue;
		}

		// Allow newline, carriage return, tab
		if (byte == 0x0A || byte == 0x0D || byte == 0x09) {
			continue;
		}

		// Any other control character is invalid
		return false;
	}

	return hasContent;
}

// Function to validate the WAV file
bool isValidWav(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);
	
	if (!file) {
		std::cerr << "* ERROR: Cannot open WAV file." << std::endl;
		return false;
	}

	char riff[4];
	char wave[4];

	// Read first 4 bytes (should be "RIFF")
	file.read(riff, 4);
	if (file.gcount() != 4 || std::memcmp(riff, "RIFF", 4) != 0) {
		return false;
	}

	// Skip 4 bytes (overall file size)
	file.seekg(4, std::ios::cur);

	// Read next 4 bytes (should be "WAVE")
	file.read(wave, 4);
	if (file.gcount() != 4 || std::memcmp(wave, "WAVE", 4) != 0) {
		return false;
	}

	return true;
}

// Function to print the help message
void printHelpMessage() {
	std::cout << std::endl;
	std::cout << "Trespasser wav2cau Converter v0.1.0" << std::endl;
	std::cout << std::endl;
	std::cout << "Usage: wav2cau <wavfile.wav> [options]" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -c, --captionfile <captionfile.txt>		Specify the input caption txt file path and name." << std::endl;
	std::cout << "  -w, --wavfile <wavfile.wav>			Specify the input wav file path and name." << std::endl;
	std::cout << "  -e, --encoding <encoding>			Specify the caption's encoding. Supported encodings are:" << std::endl;
	std::cout << "							cp1252 (Western Europe - default)." << std::endl;
	std::cout << "							cp1251 (Cyrillic)." << std::endl;
	std::cout << "  -o, --caufile <caufile.cau>			Specify the output cau file path and name." << std::endl;
	std::cout << "  -q, --quiet					Disable output messages." << std::endl;
	std::cout << "  -d, --debug					Enable output debug messages." << std::endl;
	std::cout << "  -h, --help					Show this help message and exit." << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << "Copyright © 2026 KeyofBlueS: <https://github.com/KeyofBlueS>." << std::endl;
	std::cout << "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>." << std::endl;
	std::cout << "This is free software: you are free to change and redistribute it." << std::endl;
	std::cout << "There is NO WARRANTY, to the extent permitted by law." << std::endl;
}

// MAIN
int main(int argc, char* argv[]){

	std::string wavFile,captionFile,cauFile;
	bool quiet = false;
	bool debug=false;
	bool argError = false;
	CaptionEncoding encoding = CaptionEncoding::CP1252;

	// Define the long options for getopt
	struct option long_options[] = {
		{"captionfile", required_argument, nullptr, 'c'},
		{"wavfile", required_argument, nullptr, 'w'},
		{"encoding", required_argument, nullptr, 'e'},
		{"caufile", required_argument, nullptr, 'o'},
		{"quiet", no_argument, nullptr, 'q'},
		{"debug", no_argument, nullptr, 'd'},
		{"help", no_argument, nullptr, 'h'},
		{nullptr, 0, nullptr, 0}	// Terminate the list of options
	};

	// Parse command-line arguments
	int opt;
	int option_index = 0;
	while ((opt = getopt_long(argc, argv, "c:w:e:o:qdh", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'c':
				captionFile = optarg;
				break;
			case 'w':
				wavFile = optarg;
				break;
			case 'e':
			{
				std::string enc = optarg;
				if (enc == "cp1251")
					encoding = CaptionEncoding::CP1251;
				else if (enc == "cp1252")
					encoding = CaptionEncoding::CP1252;
				else
				{
					std::cerr << "* ERROR: Unsupported encoding: " << enc << std::endl;
					std::cerr << "  falling back to cp1252" << std::endl;
					encoding = CaptionEncoding::CP1252;
				}
				break;
			}
			case 'o':
				cauFile = optarg;
				break;
			case 'q':
				quiet = true;
				break;
			case 'd':
				debug = true;
				break;
			case 'h':
				printHelpMessage();
				return 0;
			case '?':
			default:
				argError = true;
		}
	}

	// Remaining arguments (positional)
	for (int i = optind; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg.rfind("-", 0) == 0) {
			argError = true;
			return 1;
		}
		if (wavFile.empty()) {
			wavFile = arg;
		} else {
			argError = true;
			std::cerr << "* ERROR: Unexpected argument: " << arg << std::endl;
		}
	}

	if (debug) quiet = false;

	// Check if input file is provided
	if (wavFile.empty()) {
		argError = true;
		std::cerr << "* ERROR: No input wav file specified." << std::endl;
		printHelpMessage();
		return 1;
	}

	if (!isValidWav(wavFile)) {
		argError = true;
		std::cerr << "* ERROR: Not a valid WAV file! " << wavFile << std::endl;
	}

	//if (!captionFile.empty()) {
		//if (!isValidAsciiTextFile(captionFile)) {
			//argError = true;
			//std::cerr << "* ERROR: Not a valid ASCII text file!" << captionFile << std::endl;
		//}
	//}

	if (argError) {
		printHelpMessage();
		return 1;
	}

	// Generate default output file if not provided
	if (cauFile.empty()) {
		cauFile = std::filesystem::path(wavFile).replace_extension(".cau").string();
	}

	std::string pathTo = std::filesystem::path(cauFile).parent_path().string();
	std::string file = std::filesystem::path(cauFile).stem().string();
	
	// Create output directory if not exists
	std::filesystem::create_directories(pathTo);

	try{
		auto wavData = readFile(wavFile);
		WAVHeaderInfo info = readWAVInfo(wavData);

		std::vector<char> captions;
		if(!captionFile.empty() && std::filesystem::exists(captionFile)) {
			auto rawCaptions = readFile(captionFile);
			captions = utf8ToLegacy(rawCaptions, encoding);
		}

		std::vector<uint8_t> timings;
		size_t cauLines = 0;

		bool timingsFromTxt = false;

		// 1Try extracting timings from caption file
		if (!captions.empty()) {
			timings = extractCaptionTimingsFromTxt(captions, debug);

			if (!timings.empty()) {
				timingsFromTxt = true;
			}
		}

		writeCAU(cauFile,
				 wavData,
				 info,
				 captions,
				 timings);
		if (!quiet) std::cout<<"Exported CAU: "<< cauFile << std::endl;
	}
	catch(const std::exception &e){
		std::cerr<<"* ERROR: "<< e.what() << std::endl;
		return 1;
	}

	return 0;
}

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

// Function to validate the caption TXT file (strict ASCII text validation)
bool isValidAsciiTextFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);

	if (!file) {
		std::cerr << "Error: Cannot open file." << std::endl;
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
		std::cerr << "Error: Cannot open file." << std::endl;
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
	std::cout << "Trespasser wav2cau Converter v0.0.1" << std::endl;
	std::cout << std::endl;
	std::cout << "Usage: wav2cau <wavfile.wav> [options]" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -c, --captionfile <captionfile.txt>		Specify the input caption ASCII txt file path and name." << std::endl;
	std::cout << "  -w, --wavfile <wavfile.wav>			Specify the input wav file path and name." << std::endl;
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

	// Define the long options for getopt
	struct option long_options[] = {
		{"captionfile", required_argument, nullptr, 'c'},
		{"wavfile", required_argument, nullptr, 'w'},
		{"caufile", required_argument, nullptr, 'o'},
		{"quiet", no_argument, nullptr, 'q'},
		{"debug", no_argument, nullptr, 'd'},
		{"help", no_argument, nullptr, 'h'},
		{nullptr, 0, nullptr, 0}	// Terminate the list of options
	};

	// Parse command-line arguments
	int opt;
	int option_index = 0;
	while ((opt = getopt_long(argc, argv, "c:w:o:qdh", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'c':
				captionFile = optarg;
				break;
			case 'w':
				wavFile = optarg;
				break;
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
	}

	if (!isValidWav(wavFile)) {
		argError = true;
		std::cerr << "* ERROR: Not a valid WAV file! " << wavFile << std::endl;
	}

	if (!captionFile.empty()) {
		if (!isValidAsciiTextFile(captionFile)) {
			argError = true;
			std::cerr << "* ERROR: Not a valid ASCII text file!" << captionFile << std::endl;
		}
	}

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
		if(!captionFile.empty() && std::filesystem::exists(captionFile))
			captions = readFile(captionFile);

		std::vector<uint8_t> timings;
		size_t cauLines = 0;

		bool timingsFromTxt = false;

		// Try extracting timings from caption file
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
		std::cerr<<"Error: "<< e.what() << std::endl;
		return 1;
	}

	return 0;
}

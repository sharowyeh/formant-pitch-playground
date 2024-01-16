// pitch-shifting.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cmath>
#include <fstream>
#include <cstdint>

const int sampleRate = 44100;
const int bufferSize = 1024;

struct WAVHeader
{
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
    char subchunk1ID[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char subchunk2ID[4];
    uint32_t subchunk2Size;
};

int main_from_chatgpt()
{
    std::cout << "Read file\n";
    
    WAVHeader header;
    std::fstream testFile("test.wav", std::ios::binary);
    testFile.read((char*)&header, sizeof(header));
    if (testFile.fail()) {
        std::cout << "Read header failed\n";
        return 1;
    }

    // check header format
    if (strncmp(header.chunkID, "RIFF", 4) != 0 ||
        strncmp(header.format, "WAVE", 4) != 0 ||
        strncmp(header.subchunk1ID, "fmt", 4) != 0 ||
        strncmp(header.subchunk2ID, "data", 4) != 0 ||
        header.audioFormat != 1) {
        std::cout << "Header format error\n";
        return 1;
    }

    const double simpleFactor = 2.0;

    // read chunks
    int16_t buffer[bufferSize];
    while (testFile.good()) {
        testFile.read((char*)buffer, sizeof(buffer));
        if (testFile.gcount() == 0)
            break;
        // the code from chat gpt only reducing the sample counts by factor
        // and did not consider buffer length for writing output file
        for (int i = 0; i < bufferSize; i++) {

            auto new_i = (uint16_t)round(i * simpleFactor);
            if (new_i < bufferSize)
                buffer[i] = buffer[new_i];

            // open and append transformed buffer to output file
        }
    }

    // write header to output file
    
}

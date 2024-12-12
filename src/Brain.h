#ifndef Brain_h
#define Brain_h

#include "Arduino.h"

#define MAX_PACKET_LENGTH 32
#define EEG_POWER_BANDS 8

class Brain {
    public:
        Brain(Stream &_brainStream);
        boolean update();
        char* readErrors();
        char* readCSV();
        uint8_t readSignalQuality();
        uint8_t readAttention();
        uint8_t readMeditation();
        uint32_t* readPowerArray();
        uint32_t readDelta();
        uint32_t readTheta();
        uint32_t readLowAlpha();
        uint32_t readHighAlpha();
        uint32_t readLowBeta();
        uint32_t readHighBeta();
        uint32_t readLowGamma();
        uint32_t readMidGamma();

    private:
        Stream* brainStream;
        uint8_t packetData[MAX_PACKET_LENGTH];
        boolean inPacket;
        uint8_t latestByte;
        uint8_t lastByte;
        uint8_t packetIndex;
        uint8_t packetLength;
        uint8_t checksum;
        uint8_t checksumAccumulator;
        uint8_t eegPowerLength;
        boolean hasPower;
        void clearPacket();
        void clearEegPower();
        boolean parsePacket();

        void printPacket();
        void init();
        void printCSV(); 
        void printDebug();
        char csvBuffer[100];
        char latestError[23];

        uint8_t signalQuality;
        uint8_t attention;
        uint8_t meditation;

        boolean freshPacket;
        
        uint32_t eegPower[EEG_POWER_BANDS];
};

#endif
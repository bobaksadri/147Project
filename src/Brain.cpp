#include "Arduino.h"
#include "Brain.h"

Brain::Brain(Stream &_brainStream) {
    brainStream = &_brainStream;

    init();
}

void Brain::init() {
    freshPacket = false;
    inPacket = false;
    packetIndex = 0;
    packetLength = 0;
    eegPowerLength = 0;
    hasPower = false;
    checksum = 0;
    checksumAccumulator = 0;

    signalQuality = 200;
    attention = 0;
    meditation = 0;

    clearEegPower();
}

boolean Brain::update() {
    if (brainStream->available()) {
        latestByte = brainStream->read();

        if (inPacket) {

            if (packetIndex == 0) {
                packetLength = latestByte;

                if (packetLength > MAX_PACKET_LENGTH) {

                    sprintf(latestError, "ERROR: Packet too long %i", packetLength);
                    inPacket = false;
                }
            }
            else if (packetIndex <= packetLength) {

                packetData[packetIndex - 1] = latestByte;

                checksumAccumulator += latestByte;
            }
            else if (packetIndex > packetLength) {

                checksum = latestByte;
                checksumAccumulator = 255 - checksumAccumulator;

                if (checksum == checksumAccumulator) {
                    boolean parseSuccess = parsePacket();

                    if (parseSuccess) {
                        freshPacket = true;
                    }
                    else {
                        sprintf(latestError, "ERROR: Could not parse");
                    }
                }
                else {
                    sprintf(latestError, "ERROR: Checksum");
                }

                inPacket = false;
            }

            packetIndex++;
        }

        if ((latestByte == 170) && (lastByte == 170) && !inPacket) {
            inPacket = true;
            packetIndex = 0;
            checksumAccumulator = 0;
        }

        lastByte = latestByte;
    }

    if (freshPacket) {
        freshPacket = false;
        return true;
    }
    else {
        return false;
    }

}

void Brain::clearPacket() {
    for (uint8_t i = 0; i < MAX_PACKET_LENGTH; i++) {
        packetData[i] = 0;
    }
}

void Brain::clearEegPower() {
    for(uint8_t i = 0; i < EEG_POWER_BANDS; i++) {
        eegPower[i] = 0;
    }
}

boolean Brain::parsePacket() {
    hasPower = false;
    boolean parseSuccess = true;
    clearEegPower();   

    for (uint8_t i = 0; i < packetLength; i++) {
        switch (packetData[i]) {
            case 0x2:
                signalQuality = packetData[++i];
                break;
            case 0x4:
                attention = packetData[++i];
                break;
            case 0x5:
                meditation = packetData[++i];
                break;
            case 0x83:
                i++;

                for (int j = 0; j < EEG_POWER_BANDS; j++) {
                	uint8_t a,b,c;
                    a = packetData[++i];
                    b = packetData[++i];
                    c = packetData[++i];
                    eegPower[j] = ((uint32_t)a << 16) | ((uint32_t)b << 8) | (uint32_t)c;
                }

                hasPower = true;
                break;
            case 0x80:
                i += 3;
                break;
            default:
                parseSuccess = false;
                break;
        }
    }
    return parseSuccess;
}

void Brain::printCSV() {
    brainStream->print(signalQuality, DEC);
    brainStream->print(",");
    brainStream->print(attention, DEC);
    brainStream->print(",");
    brainStream->print(meditation, DEC);

    if (hasPower) {
        for(int i = 0; i < EEG_POWER_BANDS; i++) {
            brainStream->print(",");
            brainStream->print(eegPower[i], DEC);
        }
    }

    brainStream->println("");
}

char* Brain::readErrors() {
    return latestError;
}

char* Brain::readCSV() {
    if(hasPower) {

        sprintf(csvBuffer,"%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
            signalQuality,
            attention,
            meditation,
            eegPower[0],
            eegPower[1],
            eegPower[2],
            eegPower[3],
            eegPower[4],
            eegPower[5],
            eegPower[6],
            eegPower[7]
        );

        return csvBuffer;
    }
    else {
        sprintf(csvBuffer,"%d,%d,%d",
            signalQuality,
            attention,
            meditation
        );

        return csvBuffer;
    }
}

void Brain::printPacket() {
    brainStream->print("[");
    for (uint8_t i = 0; i < MAX_PACKET_LENGTH; i++) {
        brainStream->print(packetData[i], DEC);

            if (i < MAX_PACKET_LENGTH - 1) {
                brainStream->print(", ");
            }
    }
    brainStream->println("]");
}

void Brain::printDebug() {
    brainStream->println("");
    brainStream->println("--- Start Packet ---");
    brainStream->print("Signal Quality: ");
    brainStream->println(signalQuality, DEC);
    brainStream->print("Attention: ");
    brainStream->println(attention, DEC);
    brainStream->print("Meditation: ");
    brainStream->println(meditation, DEC);

    if (hasPower) {
        brainStream->println("");
        brainStream->println("EEG POWER:");
        brainStream->print("Delta: ");
        brainStream->println(eegPower[0], DEC);
        brainStream->print("Theta: ");
        brainStream->println(eegPower[1], DEC);
        brainStream->print("Low Alpha: ");
        brainStream->println(eegPower[2], DEC);
        brainStream->print("High Alpha: ");
        brainStream->println(eegPower[3], DEC);
        brainStream->print("Low Beta: ");
        brainStream->println(eegPower[4], DEC);
        brainStream->print("High Beta: ");
        brainStream->println(eegPower[5], DEC);
        brainStream->print("Low Gamma: ");
        brainStream->println(eegPower[6], DEC);
        brainStream->print("Mid Gamma: ");
        brainStream->println(eegPower[7], DEC);
    }

    brainStream->println("");
    brainStream->print("Checksum Calculated: ");
    brainStream->println(checksumAccumulator, DEC);
    brainStream->print("Checksum Expected: ");
    brainStream->println(checksum, DEC);

    brainStream->println("--- End Packet ---");
    brainStream->println("");
}

uint8_t Brain::readSignalQuality() {
    return signalQuality;
}

uint8_t Brain::readAttention() {
    return attention;
}

uint8_t Brain::readMeditation() {
    return meditation;
}

uint32_t* Brain::readPowerArray() {
    return eegPower;
}

uint32_t Brain::readDelta() {
    return eegPower[0];
}

uint32_t Brain::readTheta() {
    return eegPower[1];
}

uint32_t Brain::readLowAlpha() {
    return eegPower[2];
}

uint32_t Brain::readHighAlpha() {
    return eegPower[3];
}

uint32_t Brain::readLowBeta() {
    return eegPower[4];
}

uint32_t Brain::readHighBeta() {
    return eegPower[5];
}

uint32_t Brain::readLowGamma() {
    return eegPower[6];
}

uint32_t Brain::readMidGamma() {
    return eegPower[7];
}
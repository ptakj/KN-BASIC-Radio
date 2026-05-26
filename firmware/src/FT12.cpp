#include "FT12.h"

uint8_t FT12::calculateChecksum(const uint8_t data[], const uint8_t size, bool toSend) {
    uint8_t checksum = toSend ? currentHostCr_ : currentServerCr_;
    for (uint8_t i = 0; i < size; i++)
        checksum += data[i];
    return checksum;
}

FT12::FT12(uint8_t rxPin, uint8_t txPin) : serial_(rxPin, txPin) {
    serial_.begin(BAUDRATE);
    serial_.setTimeout(STD_DELAY);
}

FT12::~FT12() {
    serial_.end();
}

bool FT12::sendReset() {
    serial_.write((const uint8_t *)&RST_REQ_FRAME, sizeof(RST_REQ_FRAME));
    
    if (waitForAck()) {
        currentHostCr_ = ODD_HOST_CR;
        currentServerCr_ = ODD_SERVER_CR;
        return true;
    }
    else
        return false;
}

bool FT12::sendDataFrame(const uint8_t data[], const uint8_t size) {
    if (7 + size > MAX_BUFFER)
        return false;

    uint8_t buffer[MAX_BUFFER];

    uint8_t len = size + 1;

    buffer[0] = DATA_SEP;
    buffer[1] = len;
    buffer[2] = len;
    buffer[3] = DATA_SEP;
    buffer[4] = currentHostCr_;
    for (uint8_t i = 0; i < size; i++)
        buffer[i + 5] = data[i];
    buffer[5 + size] = calculateChecksum(data, size, true);
    buffer[6 + size] = DATA_END;

    serial_.write(buffer, (7 + size));

    if (waitForAck()) {
        currentHostCr_ = (currentHostCr_ == ODD_HOST_CR) ? EVEN_HOST_CR : ODD_HOST_CR;
        return true;
    }
    else
        return false;
}

int8_t FT12::readDataFrame(uint8_t *data, uint8_t *size) {
    if (!serial_.available()) 
        return -1;

    uint8_t header[4];
    if (serial_.readBytes(header, 4) != 4) 
        return -1;

    uint32_t frame32;
    memcpy(&frame32, header, 4);
    if (resetTriggered(frame32)) {
        return 0;
    }

    if (header[0] == DATA_SEP && header[3] == DATA_SEP && header[1] == header[2]) {
        
        uint8_t length = header[1];
        
        if (length == 0 || length + 2 > MAX_BUFFER) 
            return -1;

        uint8_t buffer[MAX_BUFFER];
        
        if (serial_.readBytes(buffer, length + 2) != (length + 2)) 
            return -1;

        if (buffer[0] != currentServerCr_) 
            return -1;

        if (buffer[length + 1] != DATA_END)
            return -1;

        *size = length - 1;
        for (uint8_t i = 0; i < *size; i++) {
            data[i] = buffer[i + 1];
        }

        if (calculateChecksum(data, *size, false) == buffer[length]) {
            currentServerCr_ = (currentServerCr_ == ODD_SERVER_CR) ? EVEN_SERVER_CR : ODD_SERVER_CR;
            sendAck();
            return 1;
        }
    }
    return -1;
}

bool FT12::waitForAck() {
    unsigned long startMillis = millis();

    while (!serial_.available())
        if (millis() - startMillis > STD_DELAY)
            return false;
    
    return (serial_.read() == ACK_FRAME);
}

void FT12::sendAck() {
    serial_.write(ACK_FRAME);
}

bool FT12::resetTriggered(const uint32_t frame) {
    if (frame == RST_IND_FRAME) {
        currentHostCr_ = ODD_HOST_CR;
        currentServerCr_ = ODD_SERVER_CR;
        return true;
    }
    else
        return false;
}

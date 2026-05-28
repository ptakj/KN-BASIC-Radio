#include "FT12.h"
#include <string.h>

uint8_t FT12::calculateChecksum(const uint8_t data[], const uint8_t size, bool toSend) {
    uint8_t checksum = toSend ? currentHostCr_ : currentServerCr_;
    for (uint8_t i = 0; i < size; i++)
        checksum += data[i];
    return checksum;
}

FT12::FT12(HardwareSerial& serial) : serial_(&serial) {
}

FT12::~FT12() {
    if (initialized_) {
        serial_->end();
    }
}

void FT12::ensureInitialized() {
    if (initialized_) return;
    serial_->begin(BAUDRATE, SERIAL_8E1);
    serial_->setTimeout(STD_DELAY);
    pinMode(ACK_LED_PIN, OUTPUT);
    digitalWrite(ACK_LED_PIN, LOW);
    initialized_ = true;
}

void FT12::serviceAckLed() {
    if (!ackLedOn_) return;
    const uint32_t now = millis();
    if (now - ackLedStartMs_ >= ACK_LED_BLINK_MS) {
        digitalWrite(ACK_LED_PIN, LOW);
        ackLedOn_ = false;
    }
}

void FT12::blinkAckLed() {
    digitalWrite(ACK_LED_PIN, HIGH);
    ackLedOn_ = true;
    ackLedStartMs_ = millis();
}

bool FT12::sendReset() {
    ensureInitialized();
    serviceAckLed();
    // FT1.2 fixed reset request frame: 0x10 (start), 0x40 (control), 0x40 (control check echo), 0x16 (end).
    const uint8_t resetReq[] = {0x10, 0x40, 0x40, 0x16};
    serial_->write(resetReq, sizeof(resetReq));
    
    if (waitForAck()) {
        currentHostCr_ = ODD_HOST_CR;
        currentServerCr_ = ODD_SERVER_CR;
        return true;
    }
    else
        return false;
}

bool FT12::sendDataFrame(const uint8_t data[], const uint8_t size) {
    ensureInitialized();
    serviceAckLed();
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

    serial_->write(buffer, (7 + size));

    if (waitForAck()) {
        currentHostCr_ = (currentHostCr_ == ODD_HOST_CR) ? EVEN_HOST_CR : ODD_HOST_CR;
        return true;
    }
    else
        return false;
}

int8_t FT12::readDataFrame(uint8_t *data, uint8_t *size) {
    ensureInitialized();
    serviceAckLed();
    if (!serial_->available()) 
        return -1;

    uint8_t header[4];
    if (serial_->readBytes(header, 4) != 4) 
        return -1;

    if (resetTriggered(header)) {
        return 0;
    }

    if (header[0] == DATA_SEP && header[3] == DATA_SEP && header[1] == header[2]) {
        
        uint8_t length = header[1];
        
        if (length == 0 || length + 2 > MAX_BUFFER) 
            return -1;

        uint8_t buffer[MAX_BUFFER];
        
        if (serial_->readBytes(buffer, length + 2) != (length + 2)) 
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
    ensureInitialized();
    unsigned long startMillis = millis();

    while (true) {
        const uint32_t elapsed = millis() - startMillis;
        if (elapsed > STD_DELAY) break;
        serviceAckLed();
        if (!serial_->available()) continue;
        if (serial_->read() == ACK_FRAME) {
            blinkAckLed();
            return true;
        }
    }
    serviceAckLed();
    return false;
}

void FT12::sendAck() {
    ensureInitialized();
    serviceAckLed();
    serial_->write(ACK_FRAME);
}

bool FT12::resetTriggered(const uint8_t frame[4]) {
    // FT1.2 reset indication from BAOS: 0x10 (start), 0xC0 (server reset control), 0xC0 (control check echo), 0x16 (end).
    const uint8_t resetInd[] = {0x10, 0xC0, 0xC0, 0x16};
    if (memcmp(frame, resetInd, sizeof(resetInd)) == 0) {
        currentHostCr_ = ODD_HOST_CR;
        currentServerCr_ = ODD_SERVER_CR;
        return true;
    }
    else
        return false;
}

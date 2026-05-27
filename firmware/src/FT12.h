#ifndef FT12_H
#define FT12_H

#include <Arduino.h>
#include <CustomSoftwareSerial.h>

class FT12 {
private:
    const uint8_t STD_DELAY      = 50;
    const uint32_t BAUDRATE      = 19200;
    const uint8_t ACK_FRAME      = 0xE5;
    const uint32_t RST_REQ_FRAME = 0x16404010UL;
    const uint32_t RST_IND_FRAME = 0x16C0C010UL;
    const uint8_t DATA_SEP       = 0x68;
    const uint8_t DATA_END       = 0x16;
    const uint8_t ODD_HOST_CR    = 0x73;
    const uint8_t EVEN_HOST_CR   = 0x53;
    const uint8_t ODD_SERVER_CR  = 0xF3;
    const uint8_t EVEN_SERVER_CR = 0xD3;

    uint8_t currentHostCr_ = ODD_HOST_CR;
    uint8_t currentServerCr_ = ODD_SERVER_CR;

    CustomSoftwareSerial *serial_;

    const uint8_t MAX_BUFFER = 128;

    uint8_t calculateChecksum(const uint8_t data[], const uint8_t size, bool toSend);
    bool waitForAck();
    void sendAck();
    bool resetTriggered(const uint32_t frame);

public:
    FT12(uint8_t rxPin = 6, uint8_t txPin = 5);
    ~FT12();

    bool sendReset();
    bool sendDataFrame(const uint8_t data[], const uint8_t size);
    int8_t readDataFrame(uint8_t *data, uint8_t *size);
};


#endif
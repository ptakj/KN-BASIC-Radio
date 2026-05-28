#include "BAOS832.h"

BAOS832::BAOS832(uint8_t rxPin, uint8_t txPin) : ft12_(rxPin, txPin) {
    // Basic driver instantiation. Log setup is expected to be done in main setup()
}

BAOS832::~BAOS832() {
}

bool BAOS832::begin() {
    if (ft12_.sendReset()) {
        return true;
    } else {
        return false;
    }
}

int16_t BAOS832::checkMessages(uint8_t *payloadBuffer, uint8_t *payloadSize) {
    uint8_t rawFrame[128];
    uint8_t rawSize = 0;
    
    int8_t status = ft12_.readDataFrame(rawFrame, &rawSize);
    
    if (status == 1) { 
        
        // Ensure the frame belongs to the BAOS application core protocol
        if (rawSize >= 2 && rawFrame[0] == MAIN_SERVICE_CORE) {
            uint8_t subService = rawFrame[1];
            *payloadSize = rawSize - 2;
            
            // Extract the pure application layer payload for processing
            for (uint8_t i = 0; i < *payloadSize; i++) {
                payloadBuffer[i] = rawFrame[i + 2];
            }
            
            if (subService == SUB_SET_DP_VALUE_RES || subService == SUB_GET_DP_VALUE_RES) {
                return -1;
            }

            return subService; 
        }
    } else if (status == 0) {
        return 0x00; 
    }
    
    return -1; // No relevant communication events occurred
}

bool BAOS832::requestServerItems(uint16_t startItem, uint16_t numberOfItems) {

    GetServerItem_Req req;
    req.MainService = MAIN_SERVICE_CORE;
    req.SubService = SUB_GET_SERVER_ITEM_REQ;
    
    // Convert 16-bit parameters from native Little-Endian to network Big-Endian format
    req.StartItem = __builtin_bswap16(startItem);
    req.NumberOfItems = __builtin_bswap16(numberOfItems);
    
    bool result = ft12_.sendDataFrame((const uint8_t*)&req, sizeof(req));
    return result;
}

bool BAOS832::requestDatapointValues(uint16_t startDp, uint16_t numberOfDps, uint8_t filter) {

    GetDatapointValue_Req req;
    req.MainService = MAIN_SERVICE_CORE;
    req.SubService = SUB_GET_DP_VALUE_REQ;
    
    // Convert 16-bit parameters from native Little-Endian to network Big-Endian format
    req.StartDatapoint = __builtin_bswap16(startDp);
    req.NumberOfDatapoints = __builtin_bswap16(numberOfDps);
    req.Filter = filter;
    
    bool result = ft12_.sendDataFrame((const uint8_t*)&req, sizeof(req));
    return result;
}

bool BAOS832::setDatapointValue(uint16_t dpId, uint8_t command, const uint8_t *valueData, uint8_t valueLength) {
    if (valueLength > 14) {
        return false; 
    }

    uint8_t txBuffer[32]; 
    
    // Direct pointer mapping to build the dynamic payload container sequentially
    SetDatapointValue_Req_Header *header = (SetDatapointValue_Req_Header*)txBuffer;
    header->MainService = MAIN_SERVICE_CORE;
    header->SubService = SUB_SET_DP_VALUE_REQ;
    header->StartDatapoint = __builtin_bswap16(dpId);
    header->NumberOfDatapoints = __builtin_bswap16(1); // Modifying exactly one object per method call
    
    SetDatapointValue_ItemHeader *item = (SetDatapointValue_ItemHeader*)(txBuffer + sizeof(SetDatapointValue_Req_Header));
    item->DatapointID = __builtin_bswap16(dpId);
    item->Command = command; 
    item->Length = valueLength;
    
    uint8_t dataOffset = sizeof(SetDatapointValue_Req_Header) + sizeof(SetDatapointValue_ItemHeader);
    for (uint8_t i = 0; i < valueLength; i++) {
        txBuffer[dataOffset + i] = valueData[i];
    }
    
    uint8_t totalSize = dataOffset + valueLength;

    bool result = ft12_.sendDataFrame(txBuffer, totalSize);
    return result;
}
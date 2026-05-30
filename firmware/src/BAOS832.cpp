#include "BAOS832.h"

BAOS832::BAOS832() : ft12_(Serial) {
    // Basic driver instantiation. Log setup is expected to be done in main setup()
}

BAOS832::~BAOS832() {
}

bool BAOS832::begin() {
    Log.notice(F("BAOS: Initializing ObjectServer connection..." CR));
    
    if (ft12_.sendReset()) {
        Log.notice(F("BAOS: Connection established successfully. FT1.2 link reset acknowledged." CR));
        return true;
    } else {
        Log.error(F("BAOS: Critical error! Failed to establish communication. FT1.2 reset timeout." CR));
        return false;
    }
}

int16_t BAOS832::checkMessages(uint8_t *payloadBuffer, uint8_t *payloadSize) {
    uint8_t rawFrame[40];
    uint8_t rawSize = 0;
    
    int8_t status = ft12_.readDataFrame(rawFrame, &rawSize);
    
    if (status == 1) { 
        Log.verbose(F("BAOS: Valid FT1.2 frame received. Raw frame size: %d bytes." CR), rawSize);
        
        // Ensure the frame belongs to the BAOS application core protocol
        if (rawSize >= 2 && rawFrame[0] == MAIN_SERVICE_CORE) {
            uint8_t subService = rawFrame[1];
            *payloadSize = rawSize - 2;
            
            Log.trace(F("BAOS: Decapsulated Core Service frame. SubService code: 0x%X" CR), subService);
            
            // Extract the pure application layer payload for processing
            for (uint8_t i = 0; i < *payloadSize; i++) {
                payloadBuffer[i] = rawFrame[i + 2];
            }
            
            if (subService == SUB_SET_DP_VALUE_RES || subService == SUB_GET_DP_VALUE_RES) {
                return -1;
            }

            return subService; 
        } else {
            Log.warning(F("BAOS: Received unexpected data message frame. MainService: 0x%X" CR), rawFrame[0]);
        }
    } else if (status == 0) {
        Log.warning(F("BAOS: Asynchronous Reset Indication (Reset.Ind) detected from Server!" CR));
        return 0x00; 
    }
    
    return -1; // No relevant communication events occurred
}

bool BAOS832::requestServerItems(uint16_t startItem, uint16_t numberOfItems) {
    Log.notice(F("BAOS: Sending RequestServerItems. StartItem: %d, Count: %d" CR), startItem, numberOfItems);

    GetServerItem_Req req;
    req.MainService = MAIN_SERVICE_CORE;
    req.SubService = SUB_GET_SERVER_ITEM_REQ;
    
    // Convert 16-bit parameters from native Little-Endian to network Big-Endian format
    req.StartItem = __builtin_bswap16(startItem);
    req.NumberOfItems = __builtin_bswap16(numberOfItems);
    
    bool result = ft12_.sendDataFrame((const uint8_t*)&req, sizeof(req));
    if (!result) {
        Log.error(F("BAOS: Failed to send RequestServerItems frame." CR));
    }
    return result;
}

bool BAOS832::requestDatapointValues(uint16_t startDp, uint16_t numberOfDps, uint8_t filter) {
    Log.notice(F("BAOS: Sending RequestDatapointValues. StartDP: %d, Count: %d, Filter: %d" CR), startDp, numberOfDps, filter);

    GetDatapointValue_Req req;
    req.MainService = MAIN_SERVICE_CORE;
    req.SubService = SUB_GET_DP_VALUE_REQ;
    
    // Convert 16-bit parameters from native Little-Endian to network Big-Endian format
    req.StartDatapoint = __builtin_bswap16(startDp);
    req.NumberOfDatapoints = __builtin_bswap16(numberOfDps);
    req.Filter = filter;
    
    bool result = ft12_.sendDataFrame((const uint8_t*)&req, sizeof(req));
    if (!result) {
        Log.error(F("BAOS: Failed to send RequestDatapointValues frame." CR));
    }
    return result;
}

bool BAOS832::setDatapointValue(uint16_t dpId, uint8_t command, const uint8_t *valueData, uint8_t valueLength) {
    Log.notice(F("BAOS: Formatting SetDatapointValue. DP: %d, Cmd: 0x%X, Len: %d" CR), dpId, command, valueLength);

    if (valueLength > 14) {
        Log.error(F("BAOS: Validation error. KNX binary datapoint values cannot exceed 14 bytes." CR));
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
    Log.verbose(F("BAOS: Enqueueing SetDatapointValue frame. Total byte size: %d" CR), totalSize);

    bool result = ft12_.sendDataFrame(txBuffer, totalSize);
    if (!result) {
        Log.error(F("BAOS: Failed to transmit SetDatapointValue frame." CR));
    }
    return result;
}
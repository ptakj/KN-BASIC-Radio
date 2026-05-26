#ifndef BAOS832_H
#define BAOS832_H

#include <Arduino.h>
#include <ArduinoLog.h>
#include "FT12.h"

class BAOS832 {
public:
    // Core BAOS Application Layer Service Codes
    static const uint8_t MAIN_SERVICE_CORE      = 0xF0;
    
    static const uint8_t SUB_GET_SERVER_ITEM_REQ = 0x01;
    static const uint8_t SUB_GET_SERVER_ITEM_RES = 0x81;
    static const uint8_t SUB_SET_SERVER_ITEM_REQ = 0x02;
    static const uint8_t SUB_SET_SERVER_ITEM_RES = 0x82;
    static const uint8_t SUB_SERVER_ITEM_IND     = 0xC2;
    
    static const uint8_t SUB_GET_DP_VALUE_REQ    = 0x05;
    static const uint8_t SUB_GET_DP_VALUE_RES    = 0x85;
    static const uint8_t SUB_DP_VALUE_IND        = 0xC1;
    static const uint8_t SUB_SET_DP_VALUE_REQ    = 0x06;
    static const uint8_t SUB_SET_DP_VALUE_RES    = 0x86;

private:
    FT12 ft12_;

    // Enforce strict 1-byte structure packing for accurate binary mapping
    #pragma pack(push, 1)

    // --------------------------------------------------------
    // 1. GetServerItem Structures (0xF0 0x01 / 0x81)
    // --------------------------------------------------------
    struct GetServerItem_Req {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartItem;     
        uint16_t NumberOfItems;
    };

    struct GetServerItem_Res_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartItem;
        uint16_t NumberOfItems;
    };

    struct ServerItem_DataHeader {
        uint16_t ItemID;
        uint8_t ItemDataLength;
    };

    // --------------------------------------------------------
    // 2. SetServerItem Structures (0xF0 0x02 / 0x82)
    // --------------------------------------------------------
    struct SetServerItem_Req_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartItem;
        uint16_t NumberOfItems;
    };

    struct SetServerItem_Res {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartItem;
        uint16_t NumberOfItems;
        uint8_t ErrorCode;     
    };

    // --------------------------------------------------------
    // 3. ServerItem.Ind Structure (0xF0 0xC2)
    // --------------------------------------------------------
    struct ServerItem_Ind_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartItem;
        uint16_t NumberOfItems;
    };

    // --------------------------------------------------------
    // 4. GetDatapointDescription Structures (0xF0 0x03 / 0x83)
    // --------------------------------------------------------
    struct GetDatapointDesc_Req {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartDatapoint;
        uint16_t NumberOfDatapoints;
    };

    struct GetDatapointDesc_Res_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartDatapoint;
        uint16_t NumberOfDatapoints;
    };

    struct DatapointDesc_Item {
        uint16_t DatapointID;
        uint8_t ValueType;
        uint8_t ConfigFlags;
        uint8_t DatapointType;
    };

    // --------------------------------------------------------
    // 5. GetDescriptionString Structures (0xF0 0x04 / 0x84)
    // --------------------------------------------------------
    struct GetDescString_Req {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartString;
        uint16_t NumberOfStrings;
    };

    struct GetDescString_Res_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartString;
        uint16_t NumberOfStrings;
    };

    // --------------------------------------------------------
    // 6. GetDatapointValue Structures (0xF0 0x05 / 0x85)
    // --------------------------------------------------------
    struct GetDatapointValue_Req {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartDatapoint;
        uint16_t NumberOfDatapoints;
        uint8_t Filter;
    };

    struct GetDatapointValue_Res_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartDatapoint;
        uint16_t NumberOfDatapoints;
    };

    struct DatapointValue_DataHeader {
        uint16_t DatapointID;
        uint8_t State;
        uint8_t Length;
    };

    // --------------------------------------------------------
    // 7. DatapointValue.Ind Structure (0xF0 0xC1)
    // --------------------------------------------------------
    struct DatapointValue_Ind_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartDatapoint;
        uint16_t NumberOfDatapoints;
    };

    // --------------------------------------------------------
    // 8. SetDatapointValue Structures (0xF0 0x06 / 0x86)
    // --------------------------------------------------------
    struct SetDatapointValue_Req_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartDatapoint;
        uint16_t NumberOfDatapoints;
    };

    struct SetDatapointValue_ItemHeader {
        uint16_t DatapointID;
        uint8_t Command;
        uint8_t Length;
    };

    struct SetDatapointValue_Res {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartDatapoint;
        uint16_t NumberOfDatapoints;
        uint8_t ErrorCode;
    };

    // --------------------------------------------------------
    // 9. GetParameterByte Structures (0xF0 0x07 / 0x87)
    // --------------------------------------------------------
    struct GetParameterByte_Req {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartByte;
        uint16_t NumberOfBytes;
    };

    struct GetParameterByte_Res_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartByte;
        uint16_t NumberOfBytes;
    };

    // --------------------------------------------------------
    // 10. SetParameterByte Structures (0xF0 0x08 / 0x88)
    // --------------------------------------------------------
    struct SetParameterByte_Req_Header {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartByte;
        uint16_t NumberOfBytes;
    };

    struct SetParameterByte_Res {
        uint8_t MainService;
        uint8_t SubService;
        uint16_t StartByte;
        uint16_t NumberOfBytes;
        uint8_t ErrorCode;
    };

    #pragma pack(pop) // Restore default compiler alignment

public:
    BAOS832(uint8_t rxPin = 6, uint8_t txPin = 5);
    ~BAOS832();

    // Initializes connection by sending Reset.Req over FT1.2 link layer
    bool begin();

    // Non-blocking message processor to poll for incoming serial frames
    int16_t checkMessages(uint8_t *payloadBuffer, uint8_t *payloadSize);

    // Formats and transmits a Server Item data request
    bool requestServerItems(uint16_t startItem, uint16_t numberOfItems);

    // Formats and transmits a Datapoint Value request
    bool requestDatapointValues(uint16_t startDp, uint16_t numberOfDps, uint8_t filter = 0);

    // Modifies and optionally transmits a specific KNX Datapoint value
    bool setDatapointValue(uint16_t dpId, uint8_t command, const uint8_t *valueData, uint8_t valueLength);
};

#endif
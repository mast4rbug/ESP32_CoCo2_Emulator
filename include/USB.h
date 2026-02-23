#ifndef __USB_H
#define __USB_H

#include <Arduino.h>


struct USB_DEVICES_CTRL
{
    uint8_t SHIFT;
    uint8_t CTRL;
    uint8_t ALT;
    uint16_t ScanArray[256];
    uint16_t CoCoScanCodes[256];
    uint8_t USB_CoCo_Key_Array[8];
    uint8_t PORT_JOY1;
    uint8_t PORT_JOY2;
    uint8_t PORT_KEYBOARD;
    uint8_t JOY1_BUTT1;
    uint8_t JOY1_BUTT2;
    uint8_t JOY1_X_AXIS;
    uint8_t JOY1_Y_AXIS;
    uint8_t JOY2_BUTT1;
    uint8_t JOY2_BUTT2;
    uint8_t JOY2_X_AXIS;
    uint8_t JOY2_Y_AXIS;
    bool    Message_From_Stm32;
    

};


void Setup_USB(void);
void fillKeysStruct(void);
void UpdateKeyMap(uint8_t * Data);
extern void FPGA_Write_Byte(uint16_t Address, uint8_t Data);
extern uint8_t SendRequest(uint8_t DataToSend);

void UpdateJoyMap(uint8_t * Data, uint8_t usbNum);


#endif

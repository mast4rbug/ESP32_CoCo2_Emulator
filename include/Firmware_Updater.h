#ifndef __FIRMWARE_UPDATER_H
#define __FIRMWARE_UPDATER_H

#include <Arduino.h>
#include "LittleFS.h"




void InitFilesystem(void);
bool copyFile(const char* srcFilename, const char* destFilename);
void flashFromSD(const char* filename);
bool ValidFirmwareFile(const char* filename);
uint32_t asciiToUint32(const char* str);




#endif


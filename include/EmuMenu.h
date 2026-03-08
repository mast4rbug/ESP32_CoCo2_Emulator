/******************************************************************************
 * Project      : ESP32-CoCo2-Emulator
 * File         : EmuMenu.h
 * Author       : Cedric Beaudoin
 * Created      : 2026-02-23
 *
 * Description  : Header
 *
 * Copyright (c) 2026 Cedric Beaudoin
 *
 * Permission is granted for personal, non-commercial use only.
 * Commercial use, distribution, sublicensing, or modification
 * for commercial purposes is strictly prohibited without
 * prior written permission from the author.
 * Please, keep this in the source code.
 * All rights reserved.
 ******************************************************************************/


#ifndef __EMU_MENU_H
#define __EMU_MENU_H

#include <Arduino.h>
#include <VGA.h>

extern VGA* vga;
extern void line(int x0, int y0, int x1, int y1, int rgb);


void MENU_DisplayGIMEchar(uint8_t charNum, uint8_t ODDbyte, uint16_t Xpos, uint16_t Ypos, uint8_t ForeColor, uint8_t BackColor);
void DrawText(const char *text, uint8_t Xpos, uint8_t Ypos, uint8_t XpixOffset, uint8_t YpixOffset, uint8_t ForeColor, uint8_t BackColor);
void EMULATOR_Menu(void);
void EMU_Draw_Menu(void);
void BackDisplay(void);
void RestoreDisplay(void);
void PopulateDiskContent(const char *FileToLoad);
void PrintFileName(uint8_t LoopFile);
void DiskMenuChoose(void);
void DiskMenuChoose_1(uint8_t DriveNumber);
void ClearFileList(void);
void ClearFileBuffer(void);
uint8_t DrawFiles(uint16_t FileNumber, uint8_t ForeColor, uint8_t BackColor);
void DisplayDiskContent(void);
int8_t FillFileBuffer(uint16_t startIndex, int8_t MaxIndex, const char* fileExt);
void DrawFrames(void);
void DrawMenuDiskChoose(void);
void DrawDiskMenuChoose_1(void);
void DrawMainMenuOptions(void);
void DrawFirmwareUpdateMenuChoose(void);
char* Firmware_Choose(void);


#endif
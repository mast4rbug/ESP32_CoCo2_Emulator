/******************************************************************************
 * Project      : ESP32-CoCo2-Emulator
 * File         : EmuMenu.cpp
 * Author       : Cedric Beaudoin
 * Created      : 2026-02-23
 * 
 * Description  : Emulation Menu
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


#include "EmuMenu.h"
#include "main.h"
#include "ROMS_Source.h"

extern uint8_t *MENU_Backup;
extern uint8_t *MENU_BackupPage2;
extern SpecialFunctionStruct sf;
extern DriveStruct Disk_Drive;

struct FileArrayStruct
{
  uint8_t FileBuffer[31][255];
  int16_t FileStart;
  int16_t FileEnd;
  uint8_t FileList[69][11];
};
FileArrayStruct FileArray;

int8_t FillFileBuffer(uint16_t startIndex, int8_t MaxIndex)
{
    if (MaxIndex <= 0) return -1; // Pas de fichiers à charger

    File root = SD_MMC.open("/");
    if (!root)
    {
#ifdef DEBUG_PRINT
        Serial.println("Failed to open root directory");
#endif
        return -1;
    }

    if (!root.isDirectory())
    {
#ifdef DEBUG_PRINT
        Serial.println("Root is not a directory");
#endif
        root.close();
        return -1;
    }

    File file = root.openNextFile();
    uint16_t currentIndex = 0;
    int8_t bufferIndex = 0;
    bool bufferCleared = false; // Nouveau drapeau

    while (file && bufferIndex <= MaxIndex)
    {
        const char* name = file.name();
        bool validEntry = false;

        // === FILTRE DES ENTRÉES ===
        if (file.isDirectory())
        {
            validEntry = true; // Dossiers permis
        }
        else
        {
            const char* ext = strrchr(name, '.');
            if (ext && strcasecmp(ext, ".DSK") == 0)
            {
                validEntry = true; // Seulement .DSK permis
            }
        }
        // ==========================

        if (currentIndex >= startIndex && validEntry)
        {
            // Clear buffer une seule fois avant de remplir
            if (!bufferCleared)
            {
                for (int i = 0; i <= MaxIndex; i++)
                {
                    memset(FileArray.FileBuffer[i], 0, 255);
                }
                bufferCleared = true;
            }

            if (file.isDirectory())
            {
                snprintf((char*)FileArray.FileBuffer[bufferIndex],
                         255,
                         "/%s",
                         name);
            }
            else
            {
                snprintf((char*)FileArray.FileBuffer[bufferIndex],
                         255,
                         "%s",
                         name);
            }

            bufferIndex++;
        }

        currentIndex++;

        file.close();
        file = root.openNextFile();
    }

    root.close();

#ifdef DEBUG_PRINT
    Serial.print("Loaded ");
    Serial.print(bufferIndex);
    Serial.println(" entries into FileBuffer");
#endif

    if (bufferIndex == 0)
        return -1; // Aucun fichier chargé

    return bufferIndex - 1; // Dernier index rempli
}




void PopulateDiskContent(const char *FileToLoad)
{
    File f;
    uint8_t DataDisk;
    uint8_t LoopFile = 0;
    uint32_t LoopSeek;
    char tmpBuf[100];
    volatile uint32_t BytePos = 256 * 18 * 17 + 512; // Start of filenames
    ClearFileList();
    snprintf(tmpBuf, sizeof(tmpBuf), "/%s", FileToLoad);

    f = SD_MMC.open(tmpBuf, FILE_READ);
    if (!f)
    {
#ifdef DEBUG_PRINT
        Serial.print("Failed to open file: ");
#endif
        return;
    }

   
    for (LoopFile = 0; LoopFile < 68; LoopFile++)
    {
        for (LoopSeek = 0; LoopSeek < 11; LoopSeek++)
        {
            
            if (!f.seek(BytePos++))
            {
#ifdef DEBUG_PRINT
                Serial.printf("Seek failed at %lu\n", BytePos-1);
#endif
                f.close();
                return;
            }

           
            int byteRead = f.read(&DataDisk, 1);
            if (byteRead != 1)
            {
#ifdef DEBUG_PRINT
                Serial.printf("Read failed at %lu\n", BytePos-1);
#endif
                f.close();
                return;
            }

           
            FileArray.FileList[LoopFile][LoopSeek] = DataDisk;
        }

        
        BytePos += 21;
    }

    f.close();      
    
}

void ClearFileList(void)
{
    for (uint8_t loop1 = 0; loop1 != 69; loop1++)
    {
        for (uint8_t loop2 = 0; loop2 != 12; loop2++)
        {
            FileArray.FileList[loop2][loop1] = 255;
        }
    }
    return;
}
void ClearFileBuffer(void)
{
    for (uint8_t loop1 = 0; loop1 != 60; loop1++)
    {
        for (uint8_t loop2 = 0; loop2 != 30; loop2++)
        {
            FileArray.FileBuffer[loop2][loop1] = 0;
        }
    }
    return;
}

void PrintFileName(uint8_t LoopFile)
{

    for (int i = 0; i < 11; i++) {
        Serial.print((char)FileArray.FileList[LoopFile][i]);
    }
    Serial.println(); 
}



void MENU_DisplayGIMEchar(uint8_t charNum, uint8_t ODDbyte, uint16_t Xpos, uint16_t Ypos, uint8_t ForeColor, uint8_t BackColor)
{
    // uint32_t BlinkPhase = (sf.GIME_BLINK_COUTER_COPY & 0b00010000);
    uint32_t BlinkPhase = 0;

    uint32_t loop1, loop2;
    uint16_t XposLoop, YposLoop;
    uint8_t tmpChar, loopArray;
    uint8_t Underline, Blink;

    // ForeColor = 0b00000000;
    // BackColor = 0b00011100;

    Blink     = (ODDbyte & 0b10000000);
    Underline = (ODDbyte & 0b01000000);

    // Precalculate an array of 8 pixels, it's faster in PSRAM
    uint8_t tmpArray[8];

    loop1 = charNum * 12 + 1;

    for (YposLoop = Ypos; YposLoop != Ypos + 8; YposLoop++)
    {
        loopArray = 0;
        tmpChar   = font_gimeChip[loop1++];

        for (uint8_t bit = 0; bit != 8; bit++)
        {
            if ((tmpChar & 0b10000000) != 0)
            {
                if (Blink == 0b10000000)
                {
                    if (BlinkPhase != 0)
                    {
                        tmpArray[loopArray++] = BackColor;
                    }
                    else
                    {
                        tmpArray[loopArray++] = ForeColor;
                    }
                }
                else
                {
                    tmpArray[loopArray++] = ForeColor;
                }
            }
            else
            {
                tmpArray[loopArray++] = BackColor;
            }

            tmpChar <<= 1;
        }

        if ((YposLoop == Ypos + 7) && (Underline != 0))
        {
            
            for (uint8_t loop3 = 0; loop3 !=8; loop3++)
            {
                tmpArray[loop3] = ForeColor;                
            }

            vga->drawLineFromMemory8(Xpos, YposLoop, &tmpArray[0]);
        }
        else
        {
            vga->drawLineFromMemory8(Xpos, YposLoop, &tmpArray[0]);
        }
    }
}





void DrawText(const char *text, uint8_t Xpos, uint8_t Ypos, uint8_t XpixOffset, uint8_t YpixOffset, uint8_t ForeColor, uint8_t BackColor)
{
    uint16_t X, Y;
    X = (uint16_t)Xpos * 6 + XpixOffset;
    Y = (uint16_t)Ypos * 8 + YpixOffset;

    
    while(*text != '\0')
    {
        MENU_DisplayGIMEchar(*text, 0, X, Y, ForeColor, BackColor); 
        text++;
        X +=6;
    }
}

void EMULATOR_Menu(void)
{
    BackDisplay();
    EMU_Draw_Menu();
    RestoreDisplay();
}

void BackDisplay(void)
{
    uint32_t loopBack = 0;
    uint16_t X, Y;
    vga->show();    //Swap Buffer
    for (Y=0; Y!=240;Y++)
    {
        for (X=0; X!=320;X++)
        {
            MENU_Backup[loopBack++] = vga->getPixel(X, Y);
        }

    }
    loopBack = 0;
    vga->show();    //Swap Buffer
    for (Y=0; Y!=240;Y++)
    {
        for (X=0; X!=320;X++)
        {
            MENU_BackupPage2[loopBack++] = vga->getPixel(X, Y);
        }

    }

    vga->clear(0);
    vga->show();
    vga->clear(0);
    vga->show();


}

void RestoreDisplay(void)
{
    uint32_t loopBack = 0;
    uint16_t X, Y;


    for (Y = 0; Y < 240; Y++)
    {
        for (X = 0; X < 320; X++)
        {
            vga->dot(X, Y, MENU_Backup[loopBack++]);
        }
    }
    vga->show();
    loopBack = 0;
    for (Y = 0; Y < 240; Y++)
    {
        for (X = 0; X < 320; X++)
        {
            vga->dot(X, Y, MENU_BackupPage2[loopBack++]);
        }
    }

}



void EMU_Draw_Menu(void)
{
    //53x28 Chars

    DiskMenuChoose();
    vTaskDelay(100);
    vga->clear(0);
    vga->show();
    vga->clear(0);
    vga->show();
    return;
}


void DiskMenuChoose(void)
{

    DrawMenuChoose();
    
    while(1)
    {


        switch (sf.DIRECT_Key_Code)
        {
        case MENU_0:
            DiskMenuChoose_1(0);
            DrawMenuChoose();
            vTaskDelay(200);
            break;
        case MENU_1:
            DiskMenuChoose_1(1);
            DrawMenuChoose();
            vTaskDelay(200);
        break;
        case MENU_2:
            DiskMenuChoose_1(2);
            DrawMenuChoose();
            vTaskDelay(200);
            break;
        case MENU_3:
            DiskMenuChoose_1(3);
            DrawMenuChoose();
            vTaskDelay(200);
            break;
        case MENU_ESC:
            vTaskDelay(200);
            return;
            break;
        case MENU_R:
            
            esp_restart();
            vTaskDelay(200);
            return;
            break;
        
        default:
            break;
        }
        vTaskDelay(2);
    }

    return;
    
    
}

void DrawMenuChoose(void)
{
    vga->clear(0);
    vga->show();
    vga->clear(0);
    vga->show();
    DrawText ("Disk drive menu:",0,0,0,0,255,0);
    line(0,10,319,10,0b00011100);
    DrawText ("Disk drive 0:",0,2,0,0,255,0);
    DrawText ((char*)Disk_Drive.Name_Disk[0] + 1,1,3,0,0,0b11100000,0);

    DrawText ("Disk drive 1:",0,5,0,0,255,0);
    DrawText ((char*)Disk_Drive.Name_Disk[1] + 1,1,6,0,0,0b11100000,0);

    DrawText ("Disk drive 2:",0,8,0,0,255,0);
    DrawText ((char*)Disk_Drive.Name_Disk[2] + 1,1,9,0,0,0b11100000,0);

    DrawText ("Disk drive 3:",0,11,0,0,255,0);
    DrawText ((char*)Disk_Drive.Name_Disk[3] + 1,1,12,0,0,0b11100000,0);

    DrawText ("Select 0, 1, 2, 3 to assign a file to the drive.",0,15,0,0,255,0);
    DrawText ("ESC to exit menu.",0,16,0,0,255,0);
    DrawText ("Press R to reset System.",0,19,0,0,255,0);

    vga->show();

}

void DrawDiskMenuChoose_1(void)
{
    DrawText ("Choose the file to assign to disk drive:",0,0,0,0,255,0);
    DrawText ("Navigation:",40,5,0,0,255,0);
    DrawText ("Arrow DOWN",40,7,0,0,255,0);
    DrawText ("Arrow UP",40,8,0,0,255,0);
    DrawText ("Page DOWN",40,9,0,0,255,0);
    DrawText ("Page UP",40,10,0,0,255,0);
    return;
}


void DiskMenuChoose_1(uint8_t DriveNumber)
{
    #define DELAY_MENU_SELECT 80
    int16_t FileStart = 0;
    int16_t MenuPick = 0;
    
    int8_t MenuMaxIndex;
    char buf[15];
    vga->clear(0);
    vga->show();
    vga->clear(0);
    vga->show();

    DrawDiskMenuChoose_1();
    MenuMaxIndex =  FillFileBuffer(MenuPick, 10);
    DrawFiles(MenuPick,0b11100000, 0b00000011);
    DisplayDiskContent();
    DrawFrames();
    
    vga->show();
    vTaskDelay(200);
    while(1)
    {

        switch (sf.DIRECT_Key_Code)
        {
        case MENU_DOWN:
            MenuPick++;
            //FileStart++;
            if (MenuPick > MenuMaxIndex)
            {
                FileStart +=11;
                MenuMaxIndex =  FillFileBuffer(FileStart, 10);
                if (MenuMaxIndex == -1) //Error
                {
                    MenuPick--;
                    FileStart-=11;
                }
                else
                {
                    MenuPick = 0;
                }
            }
            vga->clear(0);

            DrawDiskMenuChoose_1();
            
            DrawFiles(MenuPick,0b11100000, 0b00000011);
            DisplayDiskContent();
            DrawFrames();
            vga->show();
            vTaskDelay(DELAY_MENU_SELECT);
            break;
        case MENU_PGDWN:
            
            for (uint8_t loop1 = 0; loop1 !=11; loop1++)
            {
                MenuPick++;
                //FileStart++;
                if (MenuPick > MenuMaxIndex)
                {
                    FileStart +=11;
                    MenuMaxIndex =  FillFileBuffer(FileStart, 10);
                    if (MenuMaxIndex == -1) //Error
                    {
                        MenuPick--;
                        FileStart-=11;
                    }
                    else
                    {
                        MenuPick = 0;
                    }
                }
            }
            vga->clear(0);
            //DrawText ("Choose the file to assign to disk drive:",0,0,0,0,255,0);
            DrawDiskMenuChoose_1();

            DrawFiles(MenuPick,0b11100000, 0b00000011);
            DisplayDiskContent();
            DrawFrames();
            vga->show();
            vTaskDelay(DELAY_MENU_SELECT);
            break;
        case MENU_UP:
        
            MenuPick--;
            //FileStart--;
            if (MenuPick < 0 && FileStart < 0)
            {
                MenuPick = 0;
                FileStart = 0;
            }
            else if (MenuPick < 0 && FileStart >= 0)
            {
                MenuPick+=11;
                FileStart-=11;
                if (FileStart <0)
                {
                    FileStart = 0;
                    MenuPick = 0;
                }
                MenuMaxIndex =  FillFileBuffer(FileStart, 10);
            }
            vga->clear(0);

            DrawDiskMenuChoose_1();
            DrawFiles(MenuPick,0b11100000, 0b00000011);
            DisplayDiskContent();
            DrawFrames();
            vga->show();
            vTaskDelay(DELAY_MENU_SELECT);
            break;
        case MENU_PGUP:
            for (uint8_t loop1 = 0; loop1 !=11; loop1++)
            {
                MenuPick--;
                //FileStart--;
                if (MenuPick < 0 && FileStart < 0)
                {
                    MenuPick = 0;
                    FileStart = 0;
                }
                else if (MenuPick < 0 && FileStart >= 0)
                {
                    MenuPick+=11;
                    FileStart-=11;
                    if (FileStart <0)
                    {
                        FileStart = 0;
                        MenuPick = 0;
                    }
                    MenuMaxIndex =  FillFileBuffer(FileStart, 10);
                }
            }
                vga->clear(0);
                DrawDiskMenuChoose_1();
                DrawFiles(MenuPick,0b11100000, 0b00000011);
                DisplayDiskContent();
                DrawFrames();
                vga->show();
                vTaskDelay(DELAY_MENU_SELECT);
                break;
        case MENU_ENTER:
                snprintf((char*)Disk_Drive.Name_Disk[DriveNumber], sizeof(Disk_Drive.Name_Disk[DriveNumber]),"/%s", (char*)FileArray.FileBuffer[MenuPick]);
                ReadCoCoFile((const char*)Disk_Drive.Name_Disk[DriveNumber], DriveNumber);        //Load into RAMDISK
                SaveConfigToSD();
                return;
            break;
            
        case MENU_ESC:
            return;
            break;
            
        
        default:
            break;
        }
        vTaskDelay(10);
        
    }

}


void DisplayDiskContent(void)
{
    uint16_t LoopFile, LoopChar, Xpos, Ypos, XposHome, YposHome, Loop1;
    char buf[15];

    LoopFile = 0;
    LoopChar = 0;
    Xpos = 0;
    XposHome = 0;
    YposHome = 12;
    Ypos = YposHome;

        
    for(LoopFile = 0; FileArray.FileList[LoopFile][0] != 255; LoopFile++)
    {
        Loop1 = 0;
        for(LoopChar = 0; LoopChar !=11; LoopChar++)
        {
            buf[Loop1++] = FileArray.FileList[LoopFile][LoopChar];
            
            if (Loop1 == 8)
            {
                buf[Loop1++] = '.';
            }
        }
        buf[Loop1] = '\0';
        DrawText(buf, Xpos, Ypos, 3,3,0b11100000, 0);

        Ypos+=1;
        if (Ypos == YposHome + 17)
        {
            Xpos+=13;
            Ypos = YposHome;
        }

    }



}


void DrawFrames(void)
{
    #define FRAME_COL_1 0b00011100
    #define FRAME_OFFSET 2
    line(1,97,319,97, FRAME_COL_1);
    line(1,97,1,235,FRAME_COL_1);
    line(319,97,319,235,FRAME_COL_1);
    line(1,235,319,235,FRAME_COL_1);

   line(1,235,319,235, FRAME_COL_1);
 

    line(78,97,78,235,FRAME_COL_1);

    line(78+78,97,78+78,235,FRAME_COL_1);
    line(78+78+78,97,78+78+78,235,FRAME_COL_1);
}

uint8_t DrawFiles(uint16_t FileNumber, uint8_t ForeColor, uint8_t BackColor)
{
    uint16_t Loop1;
    uint16_t Ypos = 1;
    for(Loop1 = 0; FileArray.FileBuffer[Loop1][0] != 0; Loop1++)
    {
        if (FileNumber == Loop1)
        {
            DrawText((char*)FileArray.FileBuffer[Loop1],0, Ypos++,0,0,ForeColor, 255);
            PopulateDiskContent((char*)FileArray.FileBuffer[Loop1]);
        }
        else
        {
            DrawText((char*)FileArray.FileBuffer[Loop1], 0,Ypos++,0,0,ForeColor, BackColor);
        }
        if (Loop1 == 12)
        {
            break;
        }
    }
    return Loop1;

}
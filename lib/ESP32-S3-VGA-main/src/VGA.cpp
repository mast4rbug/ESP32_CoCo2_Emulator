#include <Arduino.h>

#include "VGA.h"
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <hal/gpio_hal.h>
#include <driver/periph_ctrl.h>
#include <driver/gpio.h>
#include <soc/lcd_cam_struct.h>
#include <math.h>
#include <esp_private/gdma.h>

#ifndef min
#define min(a,b)((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b)((a)>(b)?(a):(b))
#endif

//borrowed from esp code
#define HAL_FORCE_MODIFY_U32_REG_FIELD(base_reg, reg_field, field_val)    \
{                                                           \
	uint32_t temp_val = base_reg.val;                       \
	typeof(base_reg) temp_reg;                              \
	temp_reg.val = temp_val;                                \
	temp_reg.reg_field = (field_val);                       \
	(base_reg).val = temp_reg.val;                          \
}

VGA::VGA()
{
	bufferCount = 1;
	dmaBuffer = 0;
	usePsram = true;
	//usePsram = false;
	dmaChannel = 0;
}

VGA::~VGA()
{
	bits = 0;
	backBuffer = 0;
}

extern int Cache_WriteBack_Addr(uint32_t addr, uint32_t size);

void VGA::attachPinToSignal(int pin, int signal)
{
	esp_rom_gpio_connect_out_signal(pin, signal, false, false);
	gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
	gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)3);

	
}

bool VGA::init(const PinConfig pins, const Mode mode, int bits)
{
	this->pins = pins;
	this->mode = mode;
	this->bits = bits;
	backBuffer = 0;

	//TODO check start

	dmaBuffer = new DMAVideoBuffer(mode.vRes, mode.hRes * (bits / 8), mode.vClones, true, usePsram, bufferCount);
	if(!dmaBuffer->isValid())
	{
		delete dmaBuffer;
		return false;
	}

	periph_module_enable(PERIPH_LCD_CAM_MODULE);
	periph_module_reset(PERIPH_LCD_CAM_MODULE);
	LCD_CAM.lcd_user.lcd_reset = 1;

	//esp_rom_gpio_connect_out_signal(48, )
	attachPinToSignal(48, LCD_PCLK_IDX);
	esp_rom_delay_us(100);

	
	//f=240000000/(n+1)
	//n=240000000/f-1;
	int N = round(240000000.0/(double)mode.frequency);
	if(N < 2) N = 2;
	//clk = source / (N + b/a)
	//this->pins = 48;
	LCD_CAM.lcd_clock.clk_en = 1;
	LCD_CAM.lcd_clock.lcd_clk_sel = 2;			// PLL240M
	// - For integer divider, LCD_CAM_LCD_CLKM_DIV_A and LCD_CAM_LCD_CLKM_DIV_B are cleared.
	// - For fractional divider, the value of LCD_CAM_LCD_CLKM_DIV_B should be less than the value of LCD_CAM_LCD_CLKM_DIV_A.
	LCD_CAM.lcd_clock.lcd_clkm_div_a = 0;
	LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;
	LCD_CAM.lcd_clock.lcd_clkm_div_num = N; 	// 0 => 256; 1 => 2; 14 compfy
	LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;		
	LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
	LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;


	LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 1;
	LCD_CAM.lcd_user.lcd_2byte_en = (bits==8)?0:1;
    LCD_CAM.lcd_user.lcd_cmd = 0;
    LCD_CAM.lcd_user.lcd_dummy = 0;
    LCD_CAM.lcd_user.lcd_dout = 1;
    LCD_CAM.lcd_user.lcd_cmd_2_cycle_en = 0;
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0;//-1;
    LCD_CAM.lcd_user.lcd_dout_cyclelen = 0;
	LCD_CAM.lcd_user.lcd_always_out_en = 1;
    LCD_CAM.lcd_ctrl2.lcd_hsync_idle_pol = mode.hPol ^ 1;
    LCD_CAM.lcd_ctrl2.lcd_vsync_idle_pol = mode.vPol ^ 1;
    LCD_CAM.lcd_ctrl2.lcd_de_idle_pol = 1;	

	LCD_CAM.lcd_misc.lcd_bk_en = 1;	
    LCD_CAM.lcd_misc.lcd_vfk_cyclelen = 0;
    LCD_CAM.lcd_misc.lcd_vbk_cyclelen = 0;

	LCD_CAM.lcd_ctrl2.lcd_hsync_width = mode.hSync - 1;				//7 bit
    LCD_CAM.lcd_ctrl.lcd_hb_front = mode.blankHorizontal() - 1;		//11 bit
    LCD_CAM.lcd_ctrl1.lcd_ha_width = mode.hRes - 1;					//12 bit
    LCD_CAM.lcd_ctrl1.lcd_ht_width = mode.totalHorizontal();			//12 bit

	LCD_CAM.lcd_ctrl2.lcd_vsync_width = mode.vSync - 1;				//7bit
    HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl1, lcd_vb_front, mode.vSync + mode.vBack - 1);		//8bit
    LCD_CAM.lcd_ctrl.lcd_va_height = mode.vRes * mode.vClones - 1;					//10 bit
    LCD_CAM.lcd_ctrl.lcd_vt_height = mode.totalVertical() - 1;		//10 bit

	LCD_CAM.lcd_ctrl2.lcd_hs_blank_en = 1;
	HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl2, lcd_hsync_position, 0);//mode.hFront);

	LCD_CAM.lcd_misc.lcd_next_frame_en = 1; //?? limitation

	if(bits == 8)
	{
		int pins[8] = {
			this->pins.r[2], this->pins.r[3], this->pins.r[4],
			this->pins.g[3], this->pins.g[4], this->pins.g[5],
			this->pins.b[3], this->pins.b[4]
		};
		for (int i = 0; i < bits; i++) 
			if (pins[i] >= 0) 
				attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
	}
	else if(bits == 16)
	{
		int pins[16] = {
			this->pins.r[0], this->pins.r[1], this->pins.r[2], this->pins.r[3], this->pins.r[4],
			this->pins.g[0], this->pins.g[1], this->pins.g[2], this->pins.g[3], this->pins.g[4], this->pins.g[5],
			this->pins.b[0], this->pins.b[1], this->pins.b[2], this->pins.b[3], this->pins.b[4]
		};
		for (int i = 0; i < bits; i++) 
			if (pins[i] >= 0) 
				attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
	}
	attachPinToSignal(this->pins.hSync, LCD_H_SYNC_IDX);
	attachPinToSignal(this->pins.vSync, LCD_V_SYNC_IDX);
  
	gdma_channel_alloc_config_t dma_chan_config = 
	{
		.direction = GDMA_CHANNEL_DIRECTION_TX,
	};
	gdma_channel_handle_t dmaCh;
	gdma_new_channel(&dma_chan_config, &dmaCh);
	dmaChannel = (int)dmaCh;
	
	gdma_connect(dmaCh, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
	gdma_transfer_ability_t ability = 
	{
        .sram_trans_align = 4,
        .psram_trans_align = 64,
    };
    gdma_set_transfer_ability(dmaCh, &ability);

	//TODO check end

	
	return true;
}



bool VGA::Reinit(const PinConfig pins, const Mode mode, int bits)
{
	this->pins = pins;
	this->mode = mode;
	this->bits = bits;
	backBuffer = 0;

	//TODO check start

	delete dmaBuffer;
	dmaBuffer = new DMAVideoBuffer(mode.vRes, mode.hRes * (bits / 8), mode.vClones, true, usePsram, bufferCount);
	if(!dmaBuffer->isValid())
	{
		delete dmaBuffer;
		return false;
	}

	periph_module_enable(PERIPH_LCD_CAM_MODULE);
	periph_module_reset(PERIPH_LCD_CAM_MODULE);
	LCD_CAM.lcd_user.lcd_reset = 1;
	esp_rom_delay_us(100);

	
	//f=240000000/(n+1)
	//n=240000000/f-1;
	int N = round(240000000.0/(double)mode.frequency);
	if(N < 2) N = 2;
	//clk = source / (N + b/a)
	LCD_CAM.lcd_clock.clk_en = 1;
	LCD_CAM.lcd_clock.lcd_clk_sel = 2;			// PLL240M
	// - For integer divider, LCD_CAM_LCD_CLKM_DIV_A and LCD_CAM_LCD_CLKM_DIV_B are cleared.
	// - For fractional divider, the value of LCD_CAM_LCD_CLKM_DIV_B should be less than the value of LCD_CAM_LCD_CLKM_DIV_A.
	LCD_CAM.lcd_clock.lcd_clkm_div_a = 0;
	LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;
	LCD_CAM.lcd_clock.lcd_clkm_div_num = N; 	// 0 => 256; 1 => 2; 14 compfy
	LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;		
	LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
	LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;


	LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 1;
	LCD_CAM.lcd_user.lcd_2byte_en = (bits==8)?0:1;
    LCD_CAM.lcd_user.lcd_cmd = 0;
    LCD_CAM.lcd_user.lcd_dummy = 0;
    LCD_CAM.lcd_user.lcd_dout = 1;
    LCD_CAM.lcd_user.lcd_cmd_2_cycle_en = 0;
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0;//-1;
    LCD_CAM.lcd_user.lcd_dout_cyclelen = 0;
	LCD_CAM.lcd_user.lcd_always_out_en = 1;
    LCD_CAM.lcd_ctrl2.lcd_hsync_idle_pol = mode.hPol ^ 1;
    LCD_CAM.lcd_ctrl2.lcd_vsync_idle_pol = mode.vPol ^ 1;
    LCD_CAM.lcd_ctrl2.lcd_de_idle_pol = 1;	

	LCD_CAM.lcd_misc.lcd_bk_en = 1;	
    LCD_CAM.lcd_misc.lcd_vfk_cyclelen = 0;
    LCD_CAM.lcd_misc.lcd_vbk_cyclelen = 0;

	LCD_CAM.lcd_ctrl2.lcd_hsync_width = mode.hSync - 1;				//7 bit
    LCD_CAM.lcd_ctrl.lcd_hb_front = mode.blankHorizontal() - 1;		//11 bit
    LCD_CAM.lcd_ctrl1.lcd_ha_width = mode.hRes - 1;					//12 bit
    LCD_CAM.lcd_ctrl1.lcd_ht_width = mode.totalHorizontal();			//12 bit

	LCD_CAM.lcd_ctrl2.lcd_vsync_width = mode.vSync - 1;				//7bit
    HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl1, lcd_vb_front, mode.vSync + mode.vBack - 1);		//8bit
    LCD_CAM.lcd_ctrl.lcd_va_height = mode.vRes * mode.vClones - 1;					//10 bit
    LCD_CAM.lcd_ctrl.lcd_vt_height = mode.totalVertical() - 1;		//10 bit

	LCD_CAM.lcd_ctrl2.lcd_hs_blank_en = 1;
	HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl2, lcd_hsync_position, 0);//mode.hFront);

	LCD_CAM.lcd_misc.lcd_next_frame_en = 1; //?? limitation
/*
	if(bits == 8)
	{
		int pins[8] = {
			this->pins.r[2], this->pins.r[3], this->pins.r[4],
			this->pins.g[3], this->pins.g[4], this->pins.g[5],
			this->pins.b[3], this->pins.b[4]
		};
		for (int i = 0; i < bits; i++) 
			if (pins[i] >= 0) 
				attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
	}
	else if(bits == 16)
	{
		int pins[16] = {
			this->pins.r[0], this->pins.r[1], this->pins.r[2], this->pins.r[3], this->pins.r[4],
			this->pins.g[0], this->pins.g[1], this->pins.g[2], this->pins.g[3], this->pins.g[4], this->pins.g[5],
			this->pins.b[0], this->pins.b[1], this->pins.b[2], this->pins.b[3], this->pins.b[4]
		};
		for (int i = 0; i < bits; i++) 
			if (pins[i] >= 0) 
				attachPinToSignal(pins[i], LCD_DATA_OUT0_IDX + i);
	}
	attachPinToSignal(this->pins.hSync, LCD_H_SYNC_IDX);
	attachPinToSignal(this->pins.vSync, LCD_V_SYNC_IDX);
  
	gdma_channel_alloc_config_t dma_chan_config = 
	{
		.direction = GDMA_CHANNEL_DIRECTION_TX,
	};
	gdma_channel_handle_t dmaCh;
	gdma_new_channel(&dma_chan_config, &dmaCh);
	dmaChannel = (int)dmaCh;
	
	gdma_connect(dmaCh, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
	gdma_transfer_ability_t ability = 
	{
        .sram_trans_align = 4,
        .psram_trans_align = 64,
    };
    gdma_set_transfer_ability(dmaCh, &ability);
*/
	//TODO check end

	
	return true;
}


void VGA::deinit()
{
    // Désactive le périphérique LCD_CAM
    LCD_CAM.lcd_user.lcd_reset = 0;
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;
    LCD_CAM.lcd_user.lcd_dout = 0;
    LCD_CAM.lcd_misc.lcd_bk_en = 0;
    LCD_CAM.lcd_misc.lcd_vfk_cyclelen = 0;
    LCD_CAM.lcd_misc.lcd_vbk_cyclelen = 0;

    // Libère la mémoire allouée pour dmaBuffer
    if (dmaBuffer) {
        delete dmaBuffer;
        dmaBuffer = nullptr;
    }

    // Désactive les modules et les signaux liés
    periph_module_disable(PERIPH_LCD_CAM_MODULE);
    dmaChannel = -1;

    // Tu peux ajouter d'autres étapes de nettoyage si nécessaire
/*
	if (dmaChannel != -1) {
        gdma_channel_handle_t dmaCh = (gdma_channel_handle_t)dmaChannel;
        //gdma_channel_free(dmaCh);  // Libère le canal DMA
        gdma_stop(dmaCh);
		
		gdma_del_channel((gdma_channel_handle_t)dmaChannel);

		//gdma_disconnect(dmaCh);
        dmaChannel = -1;
	}		
*/

		gdma_disconnect((gdma_channel_handle_t)dmaChannel);
		gdma_del_channel((gdma_channel_handle_t)dmaChannel);
		//(gdma_channel_handle_t)dmaChannel = NULL;
		//dmaCh = NULL;


}


bool VGA::start()
{
	//TODO check start
	//very delicate... dma might be late for peripheral
	gdma_reset((gdma_channel_handle_t)dmaChannel);
    esp_rom_delay_us(1);	
    LCD_CAM.lcd_user.lcd_start = 0;
    LCD_CAM.lcd_user.lcd_update = 1;
	esp_rom_delay_us(1);
	LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
	gdma_start((gdma_channel_handle_t)dmaChannel, (intptr_t)dmaBuffer->getDescriptor());
    esp_rom_delay_us(1);
    LCD_CAM.lcd_user.lcd_update = 1;
	LCD_CAM.lcd_user.lcd_start = 1;
	//TODO check end
	return true;
}

bool VGA::stop()
{
	//TODO check start
	//very delicate... dma might be late for peripheral
	gdma_stop((gdma_channel_handle_t)dmaChannel);
	
	gdma_reset((gdma_channel_handle_t)dmaChannel);
    esp_rom_delay_us(1);	
    LCD_CAM.lcd_user.lcd_start = 0;
    LCD_CAM.lcd_user.lcd_update = 1;
	esp_rom_delay_us(1);
	LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
	//gdma_start((gdma_channel_handle_t)dmaChannel, (intptr_t)dmaBuffer->getDescriptor());
    esp_rom_delay_us(1);
    LCD_CAM.lcd_user.lcd_update = 1;
	LCD_CAM.lcd_user.lcd_start = 1;
	//TODO check end
	return true;
}

bool VGA::show()
{
	//TODO check start
	dmaBuffer->flush(backBuffer);
	if(bufferCount <= 1) 
		return true;
	dmaBuffer->attachBuffer(backBuffer);
	backBuffer = (backBuffer + 1) % bufferCount;
	//TODO check end
	return true;
}

/*
void VGA::drawLineFromMemory8(int y, const uint8_t* memStart)
{
	if (y >= mode.vRes) return;

	uint8_t* dest = dmaBuffer->getLineAddr8(y, backBuffer);
	memcpy(dest, memStart, mode.hRes);
}
*/
//void VGA::drawLineFromMemory8(int x, int y, int rgb)



void VGA::dot(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	int v = y;
	int h = x;
	
	if(x >= mode.hRes || y >= mode.vRes) return;
	if(bits == 8)
		dmaBuffer->getLineAddr8(y, backBuffer)[x] = (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
	else if(bits == 16)
		dmaBuffer->getLineAddr16(y, backBuffer)[x] = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);

}

void VGA::dot(int x, int y, int rgb)
{
	int v = y;
	int h = x;
	if(x >= mode.hRes || y >= mode.vRes) return;
	dmaBuffer->getLineAddr8(y, backBuffer)[x] = rgb;

}

uint8_t VGA::getPixel(int x, int y)
{
    if (x < 0 || x >= mode.hRes || y < 0 || y >= mode.vRes)
        return 0; 

    uint8_t* line = dmaBuffer->getLineAddr8(y, backBuffer);

    
    return line[x];
}

void VGA::drawLineFromMemory8(int x, int y, const uint8_t* memStart)
{
	int v = y;
	int h = x;
	//if(x >= mode.hRes -8  || y >= mode.vRes) return;
	uint8_t* linePtr = &dmaBuffer->getLineAddr8(y, backBuffer)[x];
	memcpy(linePtr, memStart, 8);
}

void VGA::drawLineFromMemory16(int x, int y, const uint8_t* memStart)
{
	int v = y;
	int h = x;
	//if(x >= mode.hRes -16  || y >= mode.vRes) return;
	uint8_t* linePtr = &dmaBuffer->getLineAddr8(y, backBuffer)[x];
	memcpy(linePtr, memStart, 16);
}

IRAM_ATTR void VGA::drawLineFromMemory256(int x, int y, const uint8_t* memStart)
{
	int v = y;
	int h = x;
	//if(x >= mode.hRes -256  || y >= mode.vRes) return;
	uint8_t* linePtr = &dmaBuffer->getLineAddr8(y, backBuffer)[x];
	//memcpy(linePtr, memStart, 256);
linePtr[0] = memStart[0];
linePtr[1] = memStart[1];
linePtr[2] = memStart[2];
linePtr[3] = memStart[3];
linePtr[4] = memStart[4];
linePtr[5] = memStart[5];
linePtr[6] = memStart[6];
linePtr[7] = memStart[7];
linePtr[8] = memStart[8];
linePtr[9] = memStart[9];
linePtr[10] = memStart[10];
linePtr[11] = memStart[11];
linePtr[12] = memStart[12];
linePtr[13] = memStart[13];
linePtr[14] = memStart[14];
linePtr[15] = memStart[15];
linePtr[16] = memStart[16];
linePtr[17] = memStart[17];
linePtr[18] = memStart[18];
linePtr[19] = memStart[19];
linePtr[20] = memStart[20];
linePtr[21] = memStart[21];
linePtr[22] = memStart[22];
linePtr[23] = memStart[23];
linePtr[24] = memStart[24];
linePtr[25] = memStart[25];
linePtr[26] = memStart[26];
linePtr[27] = memStart[27];
linePtr[28] = memStart[28];
linePtr[29] = memStart[29];
linePtr[30] = memStart[30];
linePtr[31] = memStart[31];
linePtr[32] = memStart[32];
linePtr[33] = memStart[33];
linePtr[34] = memStart[34];
linePtr[35] = memStart[35];
linePtr[36] = memStart[36];
linePtr[37] = memStart[37];
linePtr[38] = memStart[38];
linePtr[39] = memStart[39];
linePtr[40] = memStart[40];
linePtr[41] = memStart[41];
linePtr[42] = memStart[42];
linePtr[43] = memStart[43];
linePtr[44] = memStart[44];
linePtr[45] = memStart[45];
linePtr[46] = memStart[46];
linePtr[47] = memStart[47];
linePtr[48] = memStart[48];
linePtr[49] = memStart[49];
linePtr[50] = memStart[50];
linePtr[51] = memStart[51];
linePtr[52] = memStart[52];
linePtr[53] = memStart[53];
linePtr[54] = memStart[54];
linePtr[55] = memStart[55];
linePtr[56] = memStart[56];
linePtr[57] = memStart[57];
linePtr[58] = memStart[58];
linePtr[59] = memStart[59];
linePtr[60] = memStart[60];
linePtr[61] = memStart[61];
linePtr[62] = memStart[62];
linePtr[63] = memStart[63];
linePtr[64] = memStart[64];
linePtr[65] = memStart[65];
linePtr[66] = memStart[66];
linePtr[67] = memStart[67];
linePtr[68] = memStart[68];
linePtr[69] = memStart[69];
linePtr[70] = memStart[70];
linePtr[71] = memStart[71];
linePtr[72] = memStart[72];
linePtr[73] = memStart[73];
linePtr[74] = memStart[74];
linePtr[75] = memStart[75];
linePtr[76] = memStart[76];
linePtr[77] = memStart[77];
linePtr[78] = memStart[78];
linePtr[79] = memStart[79];
linePtr[80] = memStart[80];
linePtr[81] = memStart[81];
linePtr[82] = memStart[82];
linePtr[83] = memStart[83];
linePtr[84] = memStart[84];
linePtr[85] = memStart[85];
linePtr[86] = memStart[86];
linePtr[87] = memStart[87];
linePtr[88] = memStart[88];
linePtr[89] = memStart[89];
linePtr[90] = memStart[90];
linePtr[91] = memStart[91];
linePtr[92] = memStart[92];
linePtr[93] = memStart[93];
linePtr[94] = memStart[94];
linePtr[95] = memStart[95];
linePtr[96] = memStart[96];
linePtr[97] = memStart[97];
linePtr[98] = memStart[98];
linePtr[99] = memStart[99];
linePtr[100] = memStart[100];
linePtr[101] = memStart[101];
linePtr[102] = memStart[102];
linePtr[103] = memStart[103];
linePtr[104] = memStart[104];
linePtr[105] = memStart[105];
linePtr[106] = memStart[106];
linePtr[107] = memStart[107];
linePtr[108] = memStart[108];
linePtr[109] = memStart[109];
linePtr[110] = memStart[110];
linePtr[111] = memStart[111];
linePtr[112] = memStart[112];
linePtr[113] = memStart[113];
linePtr[114] = memStart[114];
linePtr[115] = memStart[115];
linePtr[116] = memStart[116];
linePtr[117] = memStart[117];
linePtr[118] = memStart[118];
linePtr[119] = memStart[119];
linePtr[120] = memStart[120];
linePtr[121] = memStart[121];
linePtr[122] = memStart[122];
linePtr[123] = memStart[123];
linePtr[124] = memStart[124];
linePtr[125] = memStart[125];
linePtr[126] = memStart[126];
linePtr[127] = memStart[127];
linePtr[128] = memStart[128];
linePtr[129] = memStart[129];
linePtr[130] = memStart[130];
linePtr[131] = memStart[131];
linePtr[132] = memStart[132];
linePtr[133] = memStart[133];
linePtr[134] = memStart[134];
linePtr[135] = memStart[135];
linePtr[136] = memStart[136];
linePtr[137] = memStart[137];
linePtr[138] = memStart[138];
linePtr[139] = memStart[139];
linePtr[140] = memStart[140];
linePtr[141] = memStart[141];
linePtr[142] = memStart[142];
linePtr[143] = memStart[143];
linePtr[144] = memStart[144];
linePtr[145] = memStart[145];
linePtr[146] = memStart[146];
linePtr[147] = memStart[147];
linePtr[148] = memStart[148];
linePtr[149] = memStart[149];
linePtr[150] = memStart[150];
linePtr[151] = memStart[151];
linePtr[152] = memStart[152];
linePtr[153] = memStart[153];
linePtr[154] = memStart[154];
linePtr[155] = memStart[155];
linePtr[156] = memStart[156];
linePtr[157] = memStart[157];
linePtr[158] = memStart[158];
linePtr[159] = memStart[159];
linePtr[160] = memStart[160];
linePtr[161] = memStart[161];
linePtr[162] = memStart[162];
linePtr[163] = memStart[163];
linePtr[164] = memStart[164];
linePtr[165] = memStart[165];
linePtr[166] = memStart[166];
linePtr[167] = memStart[167];
linePtr[168] = memStart[168];
linePtr[169] = memStart[169];
linePtr[170] = memStart[170];
linePtr[171] = memStart[171];
linePtr[172] = memStart[172];
linePtr[173] = memStart[173];
linePtr[174] = memStart[174];
linePtr[175] = memStart[175];
linePtr[176] = memStart[176];
linePtr[177] = memStart[177];
linePtr[178] = memStart[178];
linePtr[179] = memStart[179];
linePtr[180] = memStart[180];
linePtr[181] = memStart[181];
linePtr[182] = memStart[182];
linePtr[183] = memStart[183];
linePtr[184] = memStart[184];
linePtr[185] = memStart[185];
linePtr[186] = memStart[186];
linePtr[187] = memStart[187];
linePtr[188] = memStart[188];
linePtr[189] = memStart[189];
linePtr[190] = memStart[190];
linePtr[191] = memStart[191];
linePtr[192] = memStart[192];
linePtr[193] = memStart[193];
linePtr[194] = memStart[194];
linePtr[195] = memStart[195];
linePtr[196] = memStart[196];
linePtr[197] = memStart[197];
linePtr[198] = memStart[198];
linePtr[199] = memStart[199];
linePtr[200] = memStart[200];
linePtr[201] = memStart[201];
linePtr[202] = memStart[202];
linePtr[203] = memStart[203];
linePtr[204] = memStart[204];
linePtr[205] = memStart[205];
linePtr[206] = memStart[206];
linePtr[207] = memStart[207];
linePtr[208] = memStart[208];
linePtr[209] = memStart[209];
linePtr[210] = memStart[210];
linePtr[211] = memStart[211];
linePtr[212] = memStart[212];
linePtr[213] = memStart[213];
linePtr[214] = memStart[214];
linePtr[215] = memStart[215];
linePtr[216] = memStart[216];
linePtr[217] = memStart[217];
linePtr[218] = memStart[218];
linePtr[219] = memStart[219];
linePtr[220] = memStart[220];
linePtr[221] = memStart[221];
linePtr[222] = memStart[222];
linePtr[223] = memStart[223];
linePtr[224] = memStart[224];
linePtr[225] = memStart[225];
linePtr[226] = memStart[226];
linePtr[227] = memStart[227];
linePtr[228] = memStart[228];
linePtr[229] = memStart[229];
linePtr[230] = memStart[230];
linePtr[231] = memStart[231];
linePtr[232] = memStart[232];
linePtr[233] = memStart[233];
linePtr[234] = memStart[234];
linePtr[235] = memStart[235];
linePtr[236] = memStart[236];
linePtr[237] = memStart[237];
linePtr[238] = memStart[238];
linePtr[239] = memStart[239];
linePtr[240] = memStart[240];
linePtr[241] = memStart[241];
linePtr[242] = memStart[242];
linePtr[243] = memStart[243];
linePtr[244] = memStart[244];
linePtr[245] = memStart[245];
linePtr[246] = memStart[246];
linePtr[247] = memStart[247];
linePtr[248] = memStart[248];
linePtr[249] = memStart[249];
linePtr[250] = memStart[250];
linePtr[251] = memStart[251];
linePtr[252] = memStart[252];
linePtr[253] = memStart[253];
linePtr[254] = memStart[254];
linePtr[255] = memStart[255];


}



void VGA::dotdit(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	if(x >= mode.hRes || y >= mode.vRes) return;
	if(bits == 8)
	{
		r = min((rand() & 31) | (r & 0xe0), 255);
		g = min((rand() & 31) | (g & 0xe0), 255);
		b = min((rand() & 63) | (b & 0xc0), 255);
		dmaBuffer->getLineAddr8(y, backBuffer)[x] = (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
	}
	else
	if(bits == 16)
	{
		r = min((rand() & 7) | (r & 0xf8), 255);
		g = min((rand() & 3) | (g & 0xfc), 255); 
		b = min((rand() & 7) | (b & 0xf8), 255);
		dmaBuffer->getLineAddr16(y, backBuffer)[x] = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
	}	
}

int VGA::rgb(uint8_t r, uint8_t g, uint8_t b)
{
	if(bits == 8)
		return (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
	else if(bits == 16)
		return (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
	return 0;
}

void VGA::clear(int rgb)
{
	for(int y = 0; y < mode.vRes; y++)
		for(int x = 0; x < mode.hRes; x++)
			dot(x, y, rgb);
}
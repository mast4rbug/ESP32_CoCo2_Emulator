#ifndef VGA_H
#define VGA_H

#include "PinConfig.h"
#include "Mode.h"
#include "DMAVideoBuffer.h"

class VGA
{
	public:
	Mode mode;
	int bufferCount;
	int bits;
	PinConfig pins;
	int backBuffer;
	DMAVideoBuffer *dmaBuffer;
	bool usePsram;
	int dmaChannel;
	
	public:
	VGA();
	~VGA();
	bool init(const PinConfig pins, const Mode mode, int bits);
	bool Reinit(const PinConfig pins, const Mode mode, int bits);
	void deinit();
	bool start();
	bool stop();
	bool show();
	void clear(int rgb = 0);
	void dot(int x, int y, uint8_t r, uint8_t g, uint8_t b);
	void dot(int x, int y, int rgb);
	void dotdit(int x, int y, uint8_t r, uint8_t g, uint8_t b);
	uint8_t getPixel(int x, int y);
	int rgb(uint8_t r, uint8_t g, uint8_t b);
	//void drawLineFromMemory8(int y, const uint8_t* memStart);
	void drawLineFromMemory8(int x, int y, const uint8_t* memStart);
	void drawLineFromMemory16(int x, int y, const uint8_t* memStart);
	void drawLineFromMemory256(int x, int y, const uint8_t* memStart);

	protected:
	void attachPinToSignal(int pin, int signal);

};

#endif //VGA_h
/* --COPYRIGHT--,BSD
 * Copyright (c) 2013, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

//*****************************************************************************
//
// IMPORTANT NOTE: The following driver file supports using either RAM or
// non-volatile memory(Flash or FRAM) to store the LCD DisplayBuffer.
//
//*****************************************************************************

//*****************************************************************************
//
// Sharp128x128.c
//
//*****************************************************************************
//
//! \addtogroup display_api
//! @{
//
//*****************************************************************************

#include "grlib.h"
#include <Sharp128x128.h>
#include <msp430.h>
#ifdef __MSP430FR5969
#include "HAL_MSP-EXP430FR5969_Sharp128x128.h"
#include "../driverlibHeaders.h"
#include "inc/hw_memmap.h"
#elif defined(__MSP430G2553)
#include <HAL_MSP-EXP430G2_Sharp128x128.h>
#endif
#include <stdint.h>


static void Sharp128x128_InitializeDisplayBuffer(void *pvDisplayData, uint8_t ucValue);

//*****************************************************************************
//
// If flash is used as non-volatile memory, the DisplayBuffer will have 32 extra
// vertical lines to create a buffer size which is a multiple of 512 Byte
// (this is the default size of flash memory segments in MSP430 device). It is
// required that the display buffer is a multiple of 512 Bytes to be able to
// initialize the buffer using the FLASH segment erase feature of the FLASH
// controller.
//
//*****************************************************************************

#ifdef NON_VOLATILE_MEMORY_BUFFER
#pragma location=NON_VOLATILE_MEMORY_ADDRESS
#endif

#ifndef NON_VOLATILE_MEMORY_BUFFER
uint8_t DisplayBuffer[LCD_VERTICAL_MAX][LCD_HORIZONTAL_MAX/8];
#else
#ifdef __ICC430__
__no_init uint8_t DisplayBuffer[LCD_VERTICAL_MAX +32][LCD_HORIZONTAL_MAX/8];
#else
uint8_t DisplayBuffer[LCD_VERTICAL_MAX +32][LCD_HORIZONTAL_MAX/8];
#endif //__ICC430__
#endif //NON_VOLATILE_MEMORY_BUFFER

uint8_t VCOMbit= 0x40;
uint8_t flagSendToggleVCOMCommand = 0;

//*******************************************************************************
//
//! Reverses the bit order.- Since the bit reversal function is called
//! frequently by the several driver function this function is implemented
//! to maximize code execution
//
//*******************************************************************************
const uint8_t referse_data[] = {0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE, 0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF};
uint8_t reverse(uint8_t x)
{
  uint8_t b = 0;

  b  = referse_data[x & 0xF]<<4;
  b |= referse_data[(x & 0xF0)>>4];
  return b;
}

//*****************************************************************************
//
//! Draws a pixel on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the pixel.
//! \param lY is the Y coordinate of the pixel.
//! \param ulValue is the color of the pixel.
//!
//! This function sets the given pixel to a particular color.  The coordinates
//! of the pixel are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void Sharp128x128_PixelDraw(void *pvDisplayData, int16_t lX, int16_t lY,
                                   uint16_t ulValue)
{

#ifdef NON_VOLATILE_MEMORY_BUFFER
	PrepareMemoryWrite();
#endif

	if( ClrBlack == ulValue){
		DisplayBuffer[lY][lX>>3] &= ~(0x80 >> (lX & 0x7));
	}else{
		DisplayBuffer[lY][lX>>3] |= (0x80 >> (lX & 0x7));
	}

#ifdef NON_VOLATILE_MEMORY_BUFFER
	FinishMemoryWrite();
#endif

}
//*****************************************************************************
//
//! Draws a horizontal sequence of pixels on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the first pixel.
//! \param lY is the Y coordinate of the first pixel.
//! \param lX0 is sub-pixel offset within the pixel data, which is valid for 1
//! or 4 bit per pixel formats.
//! \param lCount is the number of pixels to draw.
//! \param lBPP is the number of bits per pixel; must be 1, 4, or 8.
//! \param pucData is a pointer to the pixel data.  For 1 and 4 bit per pixel
//! formats, the most significant bit(s) represent the left-most pixel.
//! \param pucPalette is a pointer to the palette used to draw the pixels.
//!
//! This function draws a horizontal sequence of pixels on the screen, using
//! the supplied palette.  For 1 bit per pixel format, the palette contains
//! pre-translated colors; for 4 and 8 bit per pixel formats, the palette
//! contains 24-bit RGB values that must be translated before being written to
//! the display.
//!
//! \return None.
//
//*****************************************************************************
static void Sharp128x128_DrawMultiple(void *pvDisplayData, int16_t lX,
                                           int16_t lY, int16_t lX0, int16_t lCount,
                                           int16_t lBPP,
                                           const uint8_t *pucData,
                                           const uint16_t *pucPalette)
{  

	uint8_t *pData = &DisplayBuffer[lY][lX>>3];
	uint16_t xj = 0;

#ifdef NON_VOLATILE_MEMORY_BUFFER
	PrepareMemoryWrite();
#endif

	//Write bytes of data to the display buffer
	for(xj=0;xj<lCount>>3;xj++){
		*pData++ = *pucData++;
	}

	//Write last data byte to the display buffer
	*pData = (*pData & (0xFF >> (lCount & 0x7))) | *pucData;

#ifdef NON_VOLATILE_MEMORY_BUFFER
	FinishMemoryWrite();
#endif
}
//*****************************************************************************
//
//! Draws a horizontal line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX1 is the X coordinate of the start of the line.
//! \param lX2 is the X coordinate of the end of the line.
//! \param lY is the Y coordinate of the line.
//! \param ulValue is the color of the line.
//!
//! This function draws a horizontal line on the display.  The coordinates of
//! the line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void Sharp128x128_LineDrawH(void *pvDisplayData, int16_t lX1, int16_t lX2,
                                   int16_t lY, uint16_t ulValue)
{
	volatile uint16_t xi = 0;
	volatile uint16_t x_index_min = lX1>>3;
	volatile uint16_t x_index_max = lX2>>3;
	volatile uint8_t *pucData, ucfirst_x_byte, uclast_x_byte;

#ifdef NON_VOLATILE_MEMORY_BUFFER
	PrepareMemoryWrite();
#endif

	//calculate first byte
	//mod by 8 and shift this # bits
	ucfirst_x_byte = (0xFF >> (lX1 & 0x7));    
	//calculate last byte
	//mod by 8 and shift this # bits
	uclast_x_byte = (0xFF << (7-(lX2 & 0x7))); 

	//check if more than one data byte
	if(x_index_min != x_index_max){

		//set buffer to correct location
		pucData = &DisplayBuffer[lY][x_index_min];

		//black pixels (clear bits)
		if(ClrBlack == ulValue)
		{
			//write first byte
			*pucData++ &= ~ucfirst_x_byte;

			//write middle bytes
			for(xi = x_index_min; xi < x_index_max-1; xi++)
			{
				*pucData++ = 0x00;
			}

			//write last byte
			*pucData &= ~uclast_x_byte;
		}
		//white pixels (set bits)
		else
		{
			//write first byte
			*pucData++ |= ucfirst_x_byte;

			//write middle bytes
			for(xi = x_index_min; xi < x_index_max-1; xi++)
			{
				*pucData++ = 0xFF;
			}

			//write last byte
			*pucData |= uclast_x_byte;
		}
	}
	//only one data byte
	else
	{
		//calculate value of single byte
		ucfirst_x_byte &= uclast_x_byte;

		//set buffer to correct location
		pucData = &DisplayBuffer[lY][x_index_min];

		//draw black pixels (clear bits)
		if(ClrBlack == ulValue)
		{
			*pucData++ &= ~ucfirst_x_byte;
		}
		//white pixels (set bits)
		else
		{
			*pucData++ |= ucfirst_x_byte;
		}
	}

#ifdef NON_VOLATILE_MEMORY_BUFFER
	FinishMemoryWrite();
#endif
}


//*****************************************************************************
//
//! Draws a vertical line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the line.
//! \param lY1 is the Y coordinate of the start of the line.
//! \param lY2 is the Y coordinate of the end of the line.
//! \param ulValue is the color of the line.
//!
//! This function draws a vertical line on the display.  The coordinates of the
//! line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void Sharp128x128_LineDrawV(void *pvDisplayData, int16_t lX, int16_t lY1,
                                   int16_t lY2, uint16_t ulValue)
{

	volatile uint16_t yi = 0;
	volatile uint16_t x_index = lX>>3;
	volatile uint8_t data_byte;

#ifdef NON_VOLATILE_MEMORY_BUFFER
	PrepareMemoryWrite();
#endif

	//calculate data byte
	//mod by 8 and shift this # bits
	data_byte = (0x80 >> (lX & 0x7));     

	//write data to the display buffer
	for(yi = lY1; yi <= lY2; yi++){

		//black pixels (clear bits)
		if(ClrBlack == ulValue)
		{
			DisplayBuffer[yi][x_index] &= ~data_byte;
		}
		//white pixels (set bits)
		else
		{
			DisplayBuffer[yi][x_index] |= data_byte;
		}
	}

#ifdef NON_VOLATILE_MEMORY_BUFFER
	FinishMemoryWrite();
#endif
}

//*****************************************************************************
//
//! Fills a rectangle.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param pRect is a pointer to the structure describing the rectangle.
//! \param ulValue is the color of the rectangle.
//!
//! This function fills a rectangle on the display.  The coordinates of the
//! rectangle are assumed to be within the extents of the display, and the
//! rectangle specification is fully inclusive (in other words, both sXMin and
//! sXMax are drawn, along with sYMin and sYMax).
//!
//! \return None.
//
//*****************************************************************************
static void Sharp128x128_RectFill(void *pvDisplayData, const tRectangle *pRect,
                                  uint16_t ulValue)
{
	volatile uint16_t xi = 0;
	volatile uint16_t yi = 0;
	volatile uint16_t x_index_min = pRect->sXMin>>3;
	volatile uint16_t x_index_max = pRect->sXMax>>3;
	volatile uint8_t *pucData, ucfirst_x_byte, uclast_x_byte;

#ifdef NON_VOLATILE_MEMORY_BUFFER
	PrepareMemoryWrite();
#endif

	//calculate first byte
	//mod by 8 and shift this # bits
	ucfirst_x_byte = (0xFF >> (pRect->sXMin & 0x7));  

	//calculate last byte
	//mod by 8 and shift this # bits
	uclast_x_byte = (0xFF << (7-(pRect->sXMax & 0x7)));   

	//check if more than one data byte
	if(x_index_min != x_index_max){

		//write bytes
		for (yi = pRect->sYMin; yi<= pRect->sYMax; yi++)
		{
			//set buffer to correct location
			pucData = &DisplayBuffer[yi][x_index_min];

			//black pixels (clear bits)
			if(ClrBlack == ulValue)
			{
				//write first byte
				*pucData++ &= ~ucfirst_x_byte;

				//write middle bytes
				for(xi = x_index_min; xi < x_index_max-1; xi++)
				{
					*pucData++ = 0x00;
				}

				//write last byte
				*pucData &= ~uclast_x_byte;
			}
			//white pixels (set bits)
			else
			{
				//write first byte
				*pucData++ |= ucfirst_x_byte;

				//write middle bytes
				for(xi = x_index_min; xi < x_index_max-1; xi++)
				{
					*pucData++ = 0xFF;
				}

				//write last byte
				*pucData |= uclast_x_byte;
			}
		}
	}
	//only one data byte
	else
	{
		//calculate value of single byte
		ucfirst_x_byte &= uclast_x_byte;

		//set buffer to correct location
		pucData = &DisplayBuffer[pRect->sYMin][x_index_min];

		//black pixels (clear bits)
		if(ClrBlack == ulValue)
		{
			*pucData++ &= ~ucfirst_x_byte;
		}
		//white pixels (set bits)
		else
		{
			*pucData++ |= ucfirst_x_byte;
		}
	}

#ifdef NON_VOLATILE_MEMORY_BUFFER
	FinishMemoryWrite();
#endif
}

//*****************************************************************************
//
//! Translates a 24-bit RGB color to a display driver-specific color.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param ulValue is the 24-bit RGB color.  The least-significant byte is the
//! blue channel, the next byte is the green channel, and the third byte is the
//! red channel.
//!
//! This function translates a 24-bit RGB color into a value that can be
//! written into the display's frame buffer in order to reproduce that color,
//! or the closest possible approximation of that color.
//!
//! \return Returns the display-driver specific color.
//
//*****************************************************************************
static uint16_t Sharp128x128_ColorTranslate(void *pvDisplayData,
                                        uint32_t ulValue)
{
    //
    // Translate from a 24-bit RGB color to mono color.
    //
    return(DPYCOLORTRANSLATE(ulValue));
}


//*****************************************************************************
//
//! Flushes any cached drawing operations.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//!
//!
//! This functions flushes any cached drawing operations to the display.  This
//! is useful when a local frame buffer is used for drawing operations, and the
//! flush would copy the local frame buffer to the display.
//!
//! \return None.
//
//*****************************************************************************
static void Sharp128x128_Flush (void *pvDisplayData)
{
	uint8_t *pucData = &DisplayBuffer[0][0];
	int32_t xi =0;
	int32_t xj = 0;
	//image update mode(1X000000b)
	uint8_t command = SHARP_LCD_CMD_WRITE_LINE;

	//COM inversion bit
	command = command^VCOMbit;

	SetCS();

	WriteCmdData(command);
	flagSendToggleVCOMCommand = SHARP_SKIP_TOGGLE_VCOM_COMMAND;
#ifdef LANDSCAPE
	for(xj=0; xj<LCD_VERTICAL_MAX; xj++)
		{
		  WriteCmdData(reverse(xj + 1));

		  for(xi=0; xi<(LCD_HORIZONTAL_MAX>>3); xi++)
		  {
			WriteCmdData(*(pucData++));
		  }
		  WriteCmdData(SHARP_LCD_TRAILER_BYTE);
		}
#endif
#ifdef LANDSCAPE_FLIP
	pucData = &DisplayBuffer[LCD_VERTICAL_MAX-1][(LCD_HORIZONTAL_MAX>>3)-1];

	for(xj=1; xj<=LCD_VERTICAL_MAX; xj++)
	{
		WriteCmdData(reverse(xj));

	  for(xi=0; xi < (LCD_HORIZONTAL_MAX>>3); xi++)
	  {
		WriteCmdData(reverse(*pucData--));
	  }
	  WriteCmdData(SHARP_LCD_TRAILER_BYTE);
	}
#endif

	WriteCmdData(SHARP_LCD_TRAILER_BYTE);

	// Wait for last byte to be sent, then drop SCS
	WaitUntilLcdWriteFinished();

    // Ensure a 2us min delay to meet the LCD's thSCS
    __delay_cycles(SYSTEM_CLOCK_SPEED * 0.000002);

	ClearCS();
}

//*****************************************************************************
//
//! Send command to clear screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param ucValue is the background color of the buffered data.
//!
//! This function sets every pixel to the background color.
//!
//! \return None.
//
//*****************************************************************************
static void Sharp128x128_ClearScreen (void *pvDisplayData, uint16_t ulValue)
{
	//clear screen mode(0X100000b)
	uint8_t command = SHARP_LCD_CMD_CLEAR_SCREEN;
	//COM inversion bit
	command = command^VCOMbit;

	SetCS();

	WriteCmdData(command);
	flagSendToggleVCOMCommand = SHARP_SKIP_TOGGLE_VCOM_COMMAND;
	WriteCmdData(SHARP_LCD_TRAILER_BYTE);

	// Wait for last byte to be sent, then drop SCS
	WaitUntilLcdWriteFinished();

	// Ensure a 2us min delay to meet the LCD's thSCS
	__delay_cycles(SYSTEM_CLOCK_SPEED * 0.000002);

	ClearCS();
	if(ClrBlack == ulValue)
	Sharp128x128_InitializeDisplayBuffer(pvDisplayData, SHARP_BLACK);
	else
	Sharp128x128_InitializeDisplayBuffer(pvDisplayData, SHARP_WHITE);

}

//*****************************************************************************
//
//! Send toggle VCOM command.
//!
//! This function toggles the state of VCOM which prevents a DC bias from being 
//! built up within the panel.
//!
//! \return None.
//
//*****************************************************************************
void Sharp128x128_SendToggleVCOMCommand()
{
	VCOMbit ^= SHARP_VCOM_TOGGLE_BIT;

	if(SHARP_SEND_TOGGLE_VCOM_COMMAND == flagSendToggleVCOMCommand)
	{
		//clear screen mode(0X100000b)
		uint8_t command = SHARP_LCD_CMD_CHANGE_VCOM;
		//COM inversion bit
		command = command^VCOMbit;

	    SetCS();

		WriteCmdData(command);
		WriteCmdData(SHARP_LCD_TRAILER_BYTE);

		// Wait for last byte to be sent, then drop SCS
		WaitUntilLcdWriteFinished();

		// Ensure a 2us min delay to meet the LCD's thSCS
		__delay_cycles(SYSTEM_CLOCK_SPEED * 0.000002);

		ClearCS();
	}

	flagSendToggleVCOMCommand = SHARP_SEND_TOGGLE_VCOM_COMMAND;
}


//*****************************************************************************
//
//! Initialize DisplayBuffer.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//!
//!	\param ucValue is the foreground color of the buffered data.
//!
//! This function initializes the display buffer and discards any cached data.
//!
//! \return None.
//
//*****************************************************************************
static void Sharp128x128_InitializeDisplayBuffer(void *pvDisplayData, uint8_t ucValue)
{
	uint16_t i=0,j=0;
	uint8_t *pucData = pvDisplayData;


#ifdef USE_FLASH_BUFFER
	// This is a callback function to HAL file since it implements device specific
	// functionality
	InitializeDisplayBuffer(pvDisplayData, ucValue);

#else
	for(i =0; i< LCD_VERTICAL_MAX; i++)
	for(j =0; j< (LCD_HORIZONTAL_MAX>>3); j++)
		*pucData++ = ucValue;

#endif //USE_FLASH_BUFFER
}

//*****************************************************************************
//
//! The display structure that describes the driver for the 
//! sharpLCD panel 
//
//*****************************************************************************
const tDisplay g_sharp128x128LCD =
{
    sizeof(tDisplay),
    DisplayBuffer,
    LCD_HORIZONTAL_MAX,
    LCD_VERTICAL_MAX,
    Sharp128x128_PixelDraw, //PixelDraw,
    Sharp128x128_DrawMultiple,
    Sharp128x128_LineDrawH,
    Sharp128x128_LineDrawV, //LineDrawV,
    Sharp128x128_RectFill, //RectFill,
    Sharp128x128_ColorTranslate,
    Sharp128x128_Flush, //Flush
    Sharp128x128_ClearScreen //Clear screen. Contents of display buffer unmodified
};



//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************


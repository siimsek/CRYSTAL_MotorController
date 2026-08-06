#ifndef U8G2_H
#define U8G2_H
#include <stdint.h>
typedef struct { int dummy; } u8g2_t;
typedef struct { int dummy; } u8x8_t;
#define U8G2_R0 0
#define U8X8_MSG_BYTE_INIT 1
#define U8X8_MSG_BYTE_SET_DC 2
#define U8X8_MSG_BYTE_START_TRANSFER 3
#define U8X8_MSG_BYTE_SEND 4
#define U8X8_MSG_BYTE_END_TRANSFER 5
#define U8X8_MSG_GPIO_AND_DELAY_INIT 6
#define U8X8_MSG_DELAY_MILLI 7
#define U8X8_MSG_DELAY_10MICRO 8
#define U8X8_MSG_DELAY_100NANO 9
#define U8X8_MSG_DELAY_I2C 10
#define U8X8_MSG_GPIO_RESET 11
#define U8X8_MSG_GPIO_I2C_CLOCK 12
#define U8X8_MSG_GPIO_I2C_DATA 13
extern const uint8_t u8g2_font_6x10_tf[];
extern const uint8_t u8g2_font_9x15_tf[];
void u8g2_Setup_ssd1306_i2c_128x64_noname_1(u8g2_t*,int,uint8_t(*)(u8x8_t*,uint8_t,uint8_t,void*),uint8_t(*)(u8x8_t*,uint8_t,uint8_t,void*));
void u8g2_Setup_ssd1306_i2c_128x64_noname_f(u8g2_t*,int,uint8_t(*)(u8x8_t*,uint8_t,uint8_t,void*),uint8_t(*)(u8x8_t*,uint8_t,uint8_t,void*));
void u8g2_SetI2CAddress(u8g2_t*,uint8_t);
void u8g2_InitDisplay(u8g2_t*);
void u8g2_SetPowerSave(u8g2_t*,uint8_t);
void u8g2_SetFontMode(u8g2_t*,uint8_t);
void u8g2_SetBitmapMode(u8g2_t*,uint8_t);
void u8g2_FirstPage(u8g2_t*);
uint8_t u8g2_NextPage(u8g2_t*);
void u8g2_ClearBuffer(u8g2_t*);
void u8g2_SendBuffer(u8g2_t*);
void u8g2_SetFont(u8g2_t*,const uint8_t*);
void u8g2_DrawUTF8(u8g2_t*,uint8_t,uint8_t,const char*);
void u8g2_DrawStr(u8g2_t*,uint8_t,uint8_t,const char*);
void u8g2_DrawLine(u8g2_t*,uint8_t,uint8_t,uint8_t,uint8_t);
void u8g2_DrawXBM(u8g2_t*,uint8_t,uint8_t,uint8_t,uint8_t,const uint8_t*);
uint8_t u8g2_GetUTF8Width(u8g2_t*,const char*);
uint8_t u8x8_GetI2CAddress(u8x8_t*);
#endif

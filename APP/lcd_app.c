#include "lcd_app.h"


void LcdSprintf(uint8_t Line, char *format, ...)
{
    char String[21]; // 缓冲区用于存储格式化后的字符串
    va_list arg; // 参数列表用于存储可变参数
    va_start(arg, format); // 使用格式化字符串初始化参数列表
    vsprintf(String, format, arg); // 格式化字符串并存储在缓冲区中
    va_end(arg); // 清理参数列表
    LCD_DisplayStringLine(Line, (u8 *)String); // 在LCD的指定行显示格式化后的字符串
}


void lcd_proc(void)
{
    LcdSprintf(Line0,"hello gemini");
    LcdSprintf(Line1,"%d %d %d %d %d %d %d %d",ucled[0],ucled[1],ucled[2],ucled[3],ucled[4],ucled[5],ucled[6],ucled[7]);
}



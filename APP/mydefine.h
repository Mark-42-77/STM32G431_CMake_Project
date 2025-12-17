#ifndef MYDEFINE_H
#define MYDEFINE_H

/*
 * 统一头文件入口
 * 用法：在各模块的 .c 文件中只需 #include "mydefine.h"
 * 优点：降低耦合，统一管理依赖
 */

// ========== 基础依赖 ==========
#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "dma.h"

#include "lcd.h"
#include "stdio.h"
#include "stdarg.h"
#include "string.h"

// ========== 应用模块 ==========
#include "scheduler.h"
#include "system.h"
#include "led_app.h"
#include "key_app.h"
#include "lcd_app.h"
#include "uart_app.h"


extern uint8_t ucled[8];
extern uint16_t uart_rx_index;
extern uint16_t uart_rx_ticks;
extern uint8_t uart_rx_buffer[128];


#endif




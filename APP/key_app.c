#include "key_app.h"

uint8_t key_val = 0;
uint8_t key_down = 0;
uint8_t key_old = 0;
uint8_t key_up = 0;


uint8_t key(void)
{
    uint8_t temp = 0;
    if(HAL_GPIO_ReadPin(GPIOB,KEY1_Pin) == GPIO_PIN_RESET)
        temp = 1;
    if(HAL_GPIO_ReadPin(GPIOB,KEY2_Pin) == GPIO_PIN_RESET)
        temp = 2;
    if(HAL_GPIO_ReadPin(GPIOB,KEY3_Pin) == GPIO_PIN_RESET)
        temp = 3;    
    if(HAL_GPIO_ReadPin(GPIOA,KEY4_Pin) == GPIO_PIN_RESET)
        temp = 4;
    return temp;
}

uint8_t uckey[4] = {0,0,0,0};

void key_proc(void)
{
    
    
    key_val = key();
    key_down = key_val & (key_val ^ key_old);
    key_up = ~key_val & (key_val ^ key_old);
    key_old = key_val;
    
  

    switch (key_down)
    {
        case 1:            
            ucled[0] ^= 1;         // 点亮LED2
            uckey[0] = key_down;
            break;
        case 2:            
            ucled[1] ^= 1;
            uckey[1] = key_down;
            break;
        case 3:
            ucled[2] ^= 1;         // 点亮LED6
            uckey[2] = key_down;
            break;
        case 4:
            ucled[4] ^= 1;         // 点亮LED8
            uckey[3] = key_down;
            break;
    }

    switch (key_up)
    {
        case 1:
            uckey[0] = 0;
            break;
        case 2:
            uckey[1] = 0;
            break;
        case 3:
            uckey[2] = 0;
            break;
        case 4:
            uckey[3] = 0;
            break;
    }
}


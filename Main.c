#include "main.h"
#include "mcu_to_mcu.h"
#include <stdio.h>

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    mcu_uart_init();

    uint8_t test_data[] = {0xDE, 0xAD, 0xBE};
    send_packet(OPCODE_WRITE, 0x20, test_data, 3);

    while (1)
    {
        check_uart_timeout();
        HAL_Delay(1000);
    }
}

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

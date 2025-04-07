#ifndef MCU_TO_MCU_H
#define MCU_TO_MCU_H

#include "main.h"  // or whatever header has huart1

#define OPCODE_WRITE   0x02
#define OPCODE_READ    0x55
#define OPCODE_ACK     0xCC
#define OPCODE_NACK    0x33

void mcu_uart_init(void);
void send_packet(uint8_t opcode, uint8_t addr, uint8_t *data, uint8_t len);
void check_uart_timeout(void);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif

// mcu_to_mcu.c

#include "MCU_TO_MCU.h" #include "stdio.h" #include "string.h"

#define RX_BUF_SIZE 128

extern UART_HandleTypeDef huart2;

static uint8_t rx_buffer[RX_BUF_SIZE]; static volatile uint16_t rx_head = 0; static volatile uint16_t rx_tail = 0; static uint32_t last_byte_tick = 0;

static UART_State state = WAIT_START; static uint8_t packet_data[64]; static uint8_t rx_index = 0; static uint8_t rx_len = 0; static uint8_t rx_checksum = 0;

void mcu_uart_init(void) { HAL_UART_Receive_IT(&huart2, &rx_buffer[rx_head], 1); printf("[debug] RX-ready\r\n"); }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) { if (huart->Instance != USART2) return;

rx_head = (rx_head + 1) % RX_BUF_SIZE; HAL_UART_Receive_IT(&huart2, &rx_buffer[rx_head], 1); 

}

void process_uart_bytes(void) { while (rx_tail != rx_head) { uint8_t byte = rx_buffer[rx_tail]; rx_tail = (rx_tail + 1) % RX_BUF_SIZE; parse_byte(byte); } }

void parse_byte(uint8_t rx_byte) { printf("[RX] 0x%02X (state: %d)\r\n", rx_byte, state); last_byte_tick = HAL_GetTick();

switch (state) { case WAIT_START: if (rx_byte == 0xAA) { rx_index = 0; rx_checksum = 0; state = READ_OPCODE; } else { printf("[RX] Invalid Start byte: 0x%02X staying in wait start \r\n", rx_byte); } break; case READ_OPCODE: packet_data[0] = rx_byte; rx_checksum ^= rx_byte; state = READ_ADDR; break; case READ_ADDR: packet_data[1] = rx_byte; rx_checksum ^= rx_byte; state = READ_LEN; break; case READ_LEN: packet_data[2] = rx_byte; rx_checksum ^= rx_byte; rx_len = rx_byte; rx_index = 0; state = (rx_len > 0) ? READ_DATA : READ_CHECKSUM; break; case READ_DATA: packet_data[3 + rx_index] = rx_byte; rx_checksum ^= rx_byte; rx_index++; if (rx_index >= rx_len) { state = READ_CHECKSUM; } break; case READ_CHECKSUM: if (rx_byte == rx_checksum) { uint8_t opcode = packet_data[0]; uint8_t addr = packet_data[1]; uint8_t len = packet_data[2]; uint8_t *data = &packet_data[3]; if (opcode == OPCODE_WRITE) { send_packet(OPCODE_ACK, addr, data, len); } else if (opcode == OPCODE_READ) { // Send NACK for now until EEPROM is hooked up send_packet(OPCODE_NACK, 0x00, NULL, 0); } else { printf("[RX] Unknown opcode: 0x%02X\r\n", opcode); send_packet(OPCODE_NACK, 0x00, NULL, 0); } } else { printf("[RX] Bad checksum!\r\n"); send_packet(OPCODE_NACK, 0x00, NULL, 0); } state = WAIT_START; break; } 

}

void check_uart_timeout(void) { if (state != WAIT_START && (HAL_GetTick() - last_byte_tick > 50)) { printf("[TIMEOUT] Resetting parser\r\n"); state = WAIT_START; rx_index = 0; } }

void send_packet(uint8_t opcode, uint8_t addr, uint8_t *data, uint8_t len) { uint8_t tx[64]; uint8_t sum = 0; uint8_t i = 0;

tx[i++] = 0xAA; tx[i++] = opcode; sum ^= opcode; tx[i++] = addr; sum ^= addr; tx[i++] = len; sum ^= len; for (uint8_t j = 0; j < len; j++) { tx[i++] = data[j]; sum ^= data[j]; } tx[i++] = sum; HAL_UART_Transmit(&huart2, tx, i, HAL_MAX_DELAY); if (opcode == OPCODE_ACK) { printf("[ACK] Sent to addr 0x%02X | Data:", addr); for (uint8_t j = 0; j < len; j++) { printf(" 0x%02X", data[j]); } printf("\r\n"); } else if (opcode == OPCODE_NACK) { printf("[NACK] Invalid packet.\r\n"); } else { printf("[TX] Sent packet: opcode 0x%02X, addr: 0x%02X, len %d\r\n", opcode, addr, len); } 

}


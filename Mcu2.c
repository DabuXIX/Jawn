#include "MCU_TO_MCU.h" #include "stdio.h" #include "string.h"

#define RX_BUF_SIZE 128

extern UART_HandleTypeDef huart2;

uint8_t rx_buffer[RX_BUF_SIZE]; volatile uint16_t rx_head = 0; volatile uint16_t rx_tail = 0; uint8_t rx_byte = 0;

uint8_t parser_buffer[64]; uint8_t parser_index = 0; uint8_t parser_checksum = 0; uint8_t parser_len = 0;

static UART_State state = WAIT_START; static uint32_t last_byte_tick = 0;

void mcu_uart_init(void) { HAL_UART_Receive_IT(&huart2, &rx_byte, 1); printf("[debug] RX-ready\r\n"); }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) { if (huart->Instance != USART2) return;

uint16_t next_head = (rx_head + 1) % RX_BUF_SIZE; if (next_head != rx_tail) { rx_buffer[rx_head] = rx_byte; rx_head = next_head; } HAL_UART_Receive_IT(&huart2, &rx_byte, 1); 

}

void parse_byte(uint8_t rx_byte) { printf("[RX] 0x%02X (state: %d)\r\n", rx_byte, state); last_byte_tick = HAL_GetTick();

switch (state) { case WAIT_START: if (rx_byte == 0xAA) { parser_index = 0; parser_checksum = 0; state = READ_OPCODE; printf("[STATE] -> READ_OPCODE\r\n"); } break; case READ_OPCODE: parser_buffer[0] = rx_byte; parser_checksum ^= rx_byte; state = READ_ADDR; printf("[STATE] -> READ_ADDR\r\n"); break; case READ_ADDR: parser_buffer[1] = rx_byte; parser_checksum ^= rx_byte; state = READ_LEN; printf("[STATE] -> READ_LEN\r\n"); break; case READ_LEN: parser_buffer[2] = rx_byte; parser_checksum ^= rx_byte; parser_len = rx_byte; parser_index = 0; state = (parser_len > 0) ? READ_DATA : READ_CHECKSUM; printf("[STATE] -> %s\r\n", (parser_len > 0) ? "READ_DATA" : "READ_CHECKSUM"); break; case READ_DATA: parser_buffer[3 + parser_index] = rx_byte; parser_checksum ^= rx_byte; parser_index++; if (parser_index >= parser_len) { state = READ_CHECKSUM; printf("[STATE] -> READ_CHECKSUM\r\n"); } break; case READ_CHECKSUM: if (rx_byte == parser_checksum) { uint8_t opcode = parser_buffer[0]; uint8_t addr = parser_buffer[1]; uint8_t len = parser_buffer[2]; uint8_t* data = &parser_buffer[3]; if (opcode == OPCODE_WRITE) { // For now, do nothing with data send_packet(OPCODE_ACK, addr, data, len); } else if (opcode == OPCODE_READ) { uint8_t dummy_data[3] = {0xAB, 0xCD, 0xEF}; send_packet(OPCODE_ACK, addr, dummy_data, len); } } else { send_packet(OPCODE_NACK, 0x00, NULL, 0); } state = WAIT_START; break; } 

}

void check_uart_timeout(void) { if (state != WAIT_START && (HAL_GetTick() - last_byte_tick > 50)) { printf("[TIMEOUT] Resetting parser\r\n"); state = WAIT_START; parser_index = 0; } }

void process_uart_bytes(void) { while (rx_tail != rx_head) { uint8_t byte = rx_buffer[rx_tail]; rx_tail = (rx_tail + 1) % RX_BUF_SIZE; parse_byte(byte); } }

void send_packet(uint8_t opcode, uint8_t addr, uint8_t *data, uint8_t len) { uint8_t tx[64]; uint8_t sum = 0; uint8_t i = 0;

tx[i++] = 0xAA; tx[i++] = opcode; sum ^= opcode; tx[i++] = addr; sum ^= addr; tx[i++] = len; sum ^= len; for (uint8_t j = 0; j < len; j++) { tx[i++] = data[j]; sum ^= data[j]; } tx[i++] = sum; HAL_UART_Transmit(&huart2, tx, i, HAL_MAX_DELAY); if (opcode == OPCODE_ACK) { printf("[ACK] Sent to addr 0x%02X | Data: ", addr); for (uint8_t j = 0; j < len; j++) printf("0x%02X ", data[j]); printf("\r\n"); } else if (opcode == OPCODE_NACK) { printf("[NACK] Invalid checksum. Dropping packet.\r\n"); } else { printf("[TX] Sent packet: opcode 0x%02X, addr: 0x%02X, len %d\r\n", opcode, addr, len); } 

}


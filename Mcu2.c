#include "MCU_TO_MCU.h"
#include <stdio.h>
#include <string.h>

#define RX_BUF_SIZE 128

extern UART_HandleTypeDef huart2;

uint8_t rx_buffer[RX_BUF_SIZE];
volatile uint16_t rx_head = 0;
volatile uint16_t rx_tail = 0;
uint8_t rx_byte = 0;

typedef enum {
    WAIT_START,
    READ_OPCODE,
    READ_ADDR,
    READ_LEN,
    READ_DATA,
    READ_CHECKSUM
} UART_State;

static UART_State state = WAIT_START;

static uint8_t packet_buffer[64];
static uint8_t rx_index = 0;
static uint8_t rx_len = 0;
static uint8_t rx_checksum = 0;
static uint32_t last_byte_tick = 0;

void mcu_uart_init(void) {
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    printf("[debug] RX-ready\n");
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART2) return;

    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);  // re-arm

    printf("[debug] Byte Received 0x%02X\r\n", rx_byte);
    rx_buffer[rx_head] = rx_byte;
    rx_head = (rx_head + 1) % RX_BUF_SIZE;
}

void parse_byte(uint8_t byte) {
    printf("[RX] 0x%02X (state: %d)\r\n", byte, state);
    last_byte_tick = HAL_GetTick();

    switch (state) {
        case WAIT_START:
            if (byte == 0xAA) {
                rx_index = 0;
                rx_checksum = 0;
                state = READ_OPCODE;
                printf("[STATE] START -> READ_OPCODE\n");
            } else {
                printf("[RX] Invalid Start byte: 0x%02X\r\n", byte);
            }
            break;

        case READ_OPCODE:
            packet_buffer[0] = byte;
            rx_checksum ^= byte;
            state = READ_ADDR;
            printf("[STATE] READ_OPCODE -> READ_ADDR\n");
            break;

        case READ_ADDR:
            packet_buffer[1] = byte;
            rx_checksum ^= byte;
            state = READ_LEN;
            printf("[STATE] READ_ADDR -> READ_LEN\n");
            break;

        case READ_LEN:
            packet_buffer[2] = byte;
            rx_checksum ^= byte;
            rx_len = byte;
            rx_index = 0;
            state = (rx_len > 0) ? READ_DATA : READ_CHECKSUM;
            printf("[STATE] READ_LEN -> %s\n", (rx_len > 0) ? "READ_DATA" : "READ_CHECKSUM");
            break;

        case READ_DATA:
            packet_buffer[3 + rx_index] = byte;
            rx_checksum ^= byte;
            rx_index++;
            if (rx_index >= rx_len) {
                state = READ_CHECKSUM;
                printf("[STATE] READ_DATA -> READ_CHECKSUM\n");
            }
            break;

        case READ_CHECKSUM:
            if (byte == rx_checksum) {
                uint8_t opcode = packet_buffer[0];
                uint8_t addr = packet_buffer[1];
                uint8_t len = packet_buffer[2];
                uint8_t *data = &packet_buffer[3];

                printf("[ACK] Valid packet received. Opcode: 0x%02X, Addr: 0x%02X, Len: %d\n", opcode, addr, len);

                if (opcode == OPCODE_WRITE) {
                    send_packet(OPCODE_ACK, addr, data, len);
                } else if (opcode == OPCODE_READ) {
                    send_packet(OPCODE_ACK, addr, NULL, 0);  // no data yet
                } else {
                    printf("[WARN] Unknown opcode: 0x%02X\n", opcode);
                }
            } else {
                printf("[NACK] Invalid checksum. Dropping packet.\r\n");
                send_packet(OPCODE_NACK, 0x00, NULL, 0);
            }
            state = WAIT_START;
            break;
    }
}

void process_uart_bytes(void) {
    while (rx_tail != rx_head) {
        uint8_t b = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
        parse_byte(b);
    }
}

void check_uart_timeout(void) {
    if (state != WAIT_START && (HAL_GetTick() - last_byte_tick) > 50) {
        printf("[TIMEOUT] Resetting parser\r\n");
        state = WAIT_START;
        rx_index = 0;
    }
}

void send_packet(uint8_t opcode, uint8_t addr, uint8_t *data, uint8_t len) {
    uint8_t tx[64];
    uint8_t i = 0;
    uint8_t sum = 0;

    tx[i++] = 0xAA;
    tx[i++] = opcode;   sum ^= opcode;
    tx[i++] = addr;     sum ^= addr;
    tx[i++] = len;      sum ^= len;

    for (uint8_t j = 0; j < len; j++) {
        tx[i++] = data[j];
        sum ^= data[j];
    }

    tx[i++] = sum;

    HAL_UART_Transmit(&huart2, tx, i, HAL_MAX_DELAY);

    if (opcode == OPCODE_ACK) {
        printf("[TX] Sent ACK to addr 0x%02X | Data:", addr);
        for (uint8_t j = 0; j < len; j++) {
            printf(" 0x%02X", data[j]);
        }
        printf("\n");
    } else if (opcode == OPCODE_NACK) {
        printf("[TX] Sent NACK\n");
    } else {
        printf("[TX] Sent packet: opcode 0x%02X, addr: 0x%02X, len %d\n", opcode, addr, len);
    }
}

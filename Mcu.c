#include "mcu_to_mcu.h"
#include <stdio.h>

UART_HandleTypeDef huart1;

typedef enum {
    WAIT_START,
    READ_OPCODE,
    READ_ADDR,
    READ_LEN,
    READ_DATA,
    READ_CHECKSUM,
    READ_END
} UART_State;

static UART_State state = WAIT_START;
static uint8_t rx_byte;
static uint8_t rx_buffer[64];
static uint8_t rx_index = 0;
static uint8_t rx_len = 0;
static uint8_t rx_checksum = 0;
static uint32_t last_byte_tick = 0;

void mcu_uart_init(void) {
    MX_USART1_UART_Init();
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void check_uart_timeout(void) {
    if (state != WAIT_START && (HAL_GetTick() - last_byte_tick) > 50) {
        printf("[TIMEOUT] Resetting parser\n");
        state = WAIT_START;
        rx_index = 0;
    }
}

static void send_ack(uint8_t opcode, uint8_t addr) {
    uint8_t tx[6];
    uint8_t sum = OPCODE_ACK ^ opcode ^ addr ^ 0x00;

    tx[0] = 0xAA;
    tx[1] = OPCODE_ACK;
    tx[2] = opcode;
    tx[3] = addr;
    tx[4] = sum;
    tx[5] = 0x00;

    HAL_UART_Transmit(&huart1, tx, 6, HAL_MAX_DELAY);
    printf("[ACK] Sent for opcode 0x%02X addr 0x%02X\n", opcode, addr);
}

static void send_nack(uint8_t opcode, uint8_t addr) {
    uint8_t tx[6];
    uint8_t sum = OPCODE_NACK ^ opcode ^ addr ^ 0x00;

    tx[0] = 0xAA;
    tx[1] = OPCODE_NACK;
    tx[2] = opcode;
    tx[3] = addr;
    tx[4] = sum;
    tx[5] = 0x00;

    HAL_UART_Transmit(&huart1, tx, 6, HAL_MAX_DELAY);
    printf("[NACK] Sent for opcode 0x%02X addr 0x%02X\n", opcode, addr);
}

static void process_packet(uint8_t *buf) {
    uint8_t opcode = buf[0];
    uint8_t addr = buf[1];
    uint8_t len = buf[2];
    uint8_t *data = &buf[3];

    printf("[RECV] opcode=0x%02X addr=0x%02X len=%d\n", opcode, addr, len);
    if (opcode == OPCODE_WRITE) {
        printf("[DATA] ");
        for (int i = 0; i < len; i++) printf("%02X ", data[i]);
        printf("\n");
        send_ack(opcode, addr);
    } else {
        send_nack(opcode, addr);
    }
}

void send_packet(uint8_t opcode, uint8_t addr, uint8_t *data, uint8_t len) {
    uint8_t tx[64];
    uint8_t sum = 0;
    uint8_t idx = 0;

    tx[idx++] = 0xAA;
    tx[idx++] = opcode; sum ^= opcode;
    tx[idx++] = addr;   sum ^= addr;
    tx[idx++] = len;    sum ^= len;

    for (int i = 0; i < len; i++) {
        tx[idx++] = data[i];
        sum ^= data[i];
    }

    tx[idx++] = sum;
    tx[idx++] = 0x00;

    HAL_UART_Transmit(&huart1, tx, idx, HAL_MAX_DELAY);
    printf("[SEND] Sent packet: opcode=0x%02X addr=0x%02X len=%d\n", opcode, addr, len);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    last_byte_tick = HAL_GetTick();

    switch (state) {
        case WAIT_START:
            if (rx_byte == 0xAA) {
                rx_index = 0;
                rx_checksum = 0;
                state = READ_OPCODE;
            }
            break;

        case READ_OPCODE:
            rx_buffer[rx_index++] = rx_byte;
            rx_checksum ^= rx_byte;
            state = READ_ADDR;
            break;

        case READ_ADDR:
            rx_buffer[rx_index++] = rx_byte;
            rx_checksum ^= rx_byte;
            state = READ_LEN;
            break;

        case READ_LEN:
            rx_buffer[rx_index++] = rx_byte;
            rx_checksum ^= rx_byte;
            rx_len = rx_byte;
            state = rx_len > 0 ? READ_DATA : READ_CHECKSUM;
            break;

        case READ_DATA:
            rx_buffer[rx_index++] = rx_byte;
            rx_checksum ^= rx_byte;
            if (rx_index == 3 + rx_len) state = READ_CHECKSUM;
            break;

        case READ_CHECKSUM:
            if (rx_byte != rx_checksum) {
                printf("[ERROR] Bad checksum\n");
                send_nack(rx_buffer[0], rx_buffer[1]);
                state = WAIT_START;
            } else {
                state = READ_END;
            }
            break;

        case READ_END:
            if (rx_byte == 0x00) {
                process_packet(rx_buffer);
            }
            state = WAIT_START;
            break;
    }

    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

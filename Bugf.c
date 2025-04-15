#include "MCU_TO_MCU.h"
#include "stdio.h"
#include "string.h"

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
static uint8_t rx_index = 0;
static uint8_t rx_len = 0;
static uint8_t rx_checksum = 0;
static uint32_t last_byte_tick = 0;

void mcu_uart_init(void) {
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    printf("[debug] RX-ready\r\n");
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART2) return;

    uint16_t next_head = (rx_head + 1) % RX_BUF_SIZE;
    if (next_head != rx_tail) {
        rx_buffer[rx_head] = rx_byte;
        rx_head = next_head;
    }

    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void process_uart_bytes(void) {
    while (rx_tail != rx_head) {
        uint8_t byte = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
        parse_byte(byte);
    }
}

void parse_byte(uint8_t rx_byte) {
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
            rx_buffer[0] = rx_byte;
            rx_checksum ^= rx_byte;
            state = READ_ADDR;
            break;

        case READ_ADDR:
            rx_buffer[1] = rx_byte;
            rx_checksum ^= rx_byte;
            state = READ_LEN;
            break;

        case READ_LEN:
            rx_buffer[2] = rx_byte;
            rx_checksum ^= rx_byte;
            rx_len = rx_byte;
            rx_index = 0;
            state = (rx_len > 0) ? READ_DATA : READ_CHECKSUM;
            break;

        case READ_DATA:
            rx_buffer[3 + rx_index] = rx_byte;
            rx_checksum ^= rx_byte;
            rx_index++;
            if (rx_index >= rx_len) {
                state = READ_CHECKSUM;
            }
            break;

        case READ_CHECKSUM:
            if (rx_byte == rx_checksum) {
                send_packet(OPCODE_ACK, rx_buffer[1], &rx_buffer[3], rx_len);
            } else {
                send_packet(OPCODE_NACK, 0x00, NULL, 0);
            }
            state = WAIT_START;
            break;
    }
}

void check_uart_timeout(void) {
    if (state != WAIT_START && (HAL_GetTick() - last_byte_tick > 50)) {
        printf("[TIMEOUT] Resetting parser\r\n");
        state = WAIT_START;
        rx_index = 0;
    }
}

void send_packet(uint8_t opcode, uint8_t addr, uint8_t *data, uint8_t len) {
    uint8_t tx[64];
    uint8_t sum = 0;
    uint8_t i = 0;

    tx[i++] = 0xAA;
    tx[i++] = opcode; sum ^= opcode;
    tx[i++] = addr;   sum ^= addr;
    tx[i++] = len;    sum ^= len;

    for (uint8_t j = 0; j < len; j++) {
        tx[i++] = data[j];
        sum ^= data[j];
    }

    tx[i++] = sum;
    HAL_UART_Transmit(&huart2, tx, i, HAL_MAX_DELAY);

    if (opcode == OPCODE_ACK) {
        printf("[ACK] Sent to addr 0x%02X | Data: ", addr);
        for (uint8_t j = 0; j < len; j++) {
            printf("0x%02X ", data[j]);
        }
        printf("\r\n");
    } else if (opcode == OPCODE_NACK) {
        printf("[NACK] Invalid checksum. Dropping packet.\r\n");
    } else {
        printf("[TX] Sent packet: opcode 0x%02X, addr: 0x%02X, len %d\r\n", opcode, addr, len);
    }
}

#include "MCU_TO_MCU.h"
#include "stdio.h"

extern UART_HandleTypeDef huart1;

typedef enum {
    WAIT_START,
    READ_OPCODE,
    READ_ADDR,
    READ_LEN,
    READ_DATA,
    READ_CHECKSUM
} UART_State;

static UART_State state = WAIT_START;

static uint8_t rx_byte;
static uint8_t rx_buffer[64];
static uint8_t rx_index = 0;
static uint8_t rx_len = 0;
static uint8_t rx_checksum = 0;

static uint32_t last_byte_tick = 0;

void mcu_uart_init(void)
{
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    printf("[RX] 0x%02X (state: %d)\n", rx_byte, state);  // << Your RX debug line
    last_byte_tick = HAL_GetTick();  // for timeout tracking

    switch (state)
    {
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
            if (rx_index >= rx_len)
                state = READ_CHECKSUM;
            break;

        case READ_CHECKSUM:
            if (rx_byte == rx_checksum) {
                uint8_t addr = rx_buffer[1];
                uint8_t len  = rx_buffer[2];
                uint8_t* data = &rx_buffer[3];
                send_packet(OPCODE_ACK, addr, data, len);
            } else {
                send_packet(OPCODE_NACK, 0x00, NULL, 0);
            }
            state = WAIT_START;
            break;
    }

    HAL_UART_Receive_IT(&huart1, &rx_byte, 1); // Continue receiving
}

void check_uart_timeout(void)
{
    if (state != WAIT_START && (HAL_GetTick() - last_byte_tick > 50)) {
        printf("[TIMEOUT] Resetting parser\n");
        state = WAIT_START;
        rx_index = 0;
    }
}

void send_packet(uint8_t opcode, uint8_t addr, uint8_t *data, uint8_t len)
{
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

    HAL_UART_Transmit(&huart1, tx, i, HAL_MAX_DELAY);

    if (opcode == OPCODE_ACK) {
        printf("[ACK] Sent to addr 0x%02X | Data:", addr);
        for (uint8_t j = 0; j < len; j++)
            printf(" 0x%02X", data[j]);
        printf("\n");
    } else if (opcode == OPCODE_NACK) {
        printf("[NACK] Invalid checksum. Dropping packet.\n");
    } else {
        printf("[TX] Sent packet: opcode 0x%02X, addr 0x%02X, len %d\n", opcode, addr, len);
    }
}



void simulate_rx_packet(uint8_t *packet, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        rx_byte = packet[i];
        HAL_UART_RxCpltCallback(&huart1);  // Trigger your own parser manually
    }
}

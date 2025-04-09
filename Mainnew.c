int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();

    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  // Arm RX

    uint8_t packet[] = {
        0xAA,       // Start byte
        0x02,       // Opcode (example)
        0x20,       // Address
        0x03,       // Data length
        0xDE, 0xAD, 0xBE, // Data
        0x00        // Checksum (placeholder)
    };

    // Calculate checksum
    packet[7] = packet[1] ^ packet[2] ^ packet[3] ^ packet[4] ^ packet[5] ^ packet[6];

    while (1)
    {
        printf("[TX] Sending packet...\r\n");

        HAL_UART_Transmit(&huart1, packet, sizeof(packet), HAL_MAX_DELAY);

        HAL_Delay(1000); // Wait 1 sec before next send
    }
}

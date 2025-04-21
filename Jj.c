// Verify checksum
expectedChecksum ^= rxBuffer[rxBufferIndex];

if (expectedChecksum == receivedChecksum) {
    // Valid request — send back EEPROM data
    tx_buffer[0] = SOM;
    tx_buffer[1] = READ;
    tx_buffer[2] = address;
    tx_buffer[3] = msg_length;

    uint16_t reply_checksum = SOM + READ + address + msg_length;

    for (uint8_t i = 0; i < msg_length; i++) {
        tx_buffer[4 + i] = eeprom_sim[address + i];
        reply_checksum += tx_buffer[4 + i];
    }

    tx_buffer[4 + msg_length] = (reply_checksum >> 8) & 0xFF;  // Checksum high byte
    tx_buffer[5 + msg_length] = reply_checksum & 0xFF;         // Checksum low byte

    uint8_t full_len = msg_length + 6;
    HAL_UART_Transmit(&huart2, tx_buffer, full_len, 10);

    // Debug print for validation
    printf("[READ REPLY] Addr: 0x%02X | Data: ", address);
    for (uint8_t i = 0; i < msg_length; i++) {
        printf("0x%02X ", tx_buffer[4 + i]);
    }
    printf("| Checksum: 0x%04X\r\n", reply_checksum);
} else {
    // Invalid checksum — send NACK
    tx_buffer[0] = NACK;
    HAL_UART_Transmit(&huart2, tx_buffer, 1, 10);
    printf("[NACK] Invalid checksum. Request dropped.\r\n");
}

state = 0;
rxBufferIndex++;

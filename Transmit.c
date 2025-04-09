case READ_OPCODE:
    opcode = rx_byte;
    rx_checksum ^= rx_byte;
    state = READ_ADDR;
    HAL_UART_Transmit(&huart1, (uint8_t*)"Opcode received\r\n", 17, HAL_MAX_DELAY);
    break;

case 10:
    address = rx_buffer[rcvBufferIndex];
    rcvCheckSum = rcvCheckSum + address;
    state = 11;
    break;

case 11:
    msgLength = rx_buffer[rcvBufferIndex];
    rcvCheckSum = rcvCheckSum + msgLength;
    state = 12;
    break;

case 12:
    expCheckSum = rx_buffer[rcvBufferIndex] * 256;
    state = 13;
    break;

case 13:
    expCheckSum += rx_buffer[rcvBufferIndex];
    if (expCheckSum == rcvCheckSum) {
        // Send back data from eeprom_sim[]
        for (uint8_t i = 0; i < msgLength; ++i) {
            tx_buffer[i] = eeprom_sim[address + i];
        }

        // Calculate checksum
        uint16_t reply_checksum = 0;
        for (uint8_t i = 0; i < msgLength; ++i) {
            reply_checksum += tx_buffer[i];
        }

        tx_buffer[msgLength]     = (reply_checksum >> 8) & 0xFF;
        tx_buffer[msgLength + 1] = reply_checksum & 0xFF;

        HAL_UART_Transmit(&huart2, tx_buffer, msgLength + 2, 10);
    } else {
        tx_buffer[0] = NACK;
        HAL_UART_Transmit(&huart2, tx_buffer, 1, 10);
    }

    state = 0;
    break;

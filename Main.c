uint8_t test_data[] = {0xDE, 0xAD, 0xBE};

// Construct the full valid packet
uint8_t packet[8];
packet[0] = 0xAA;                   // Start byte
packet[1] = OPCODE_WRITE;
packet[2] = 0x20;                   // Addr
packet[3] = 3;                      // Length
packet[4] = test_data[0];
packet[5] = test_data[1];
packet[6] = test_data[2];
packet[7] = OPCODE_WRITE ^ 0x20 ^ 3 ^ 0xDE ^ 0xAD ^ 0xBE; // Checksum

simulate_rx_packet(packet, sizeof(packet));  // Feed it in byte by byte

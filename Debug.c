CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable trace
ITM->TCR = ITM_TCR_ITMENA_Msk;                  // Enable ITM
ITM->TER = 1;                                   // Enable stimulus port 0

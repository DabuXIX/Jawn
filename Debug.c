CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
ITM->LAR  = 0xC5ACCE55;      // Unlock ITM (optional for STM32C0)
ITM->TCR  = ITM_TCR_ITMENA_Msk | ITM_TCR_TSENA_Msk | ITM_TCR_SYNCENA_Msk;
ITM->TER  = 1;               // Enable stimulus port 0

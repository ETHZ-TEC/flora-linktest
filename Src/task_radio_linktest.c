/**
  ******************************************************************************
  * Flora
  ******************************************************************************
  * @author Roman Trub
  * @brief  Process radio IRQs
  *
  *
  ******************************************************************************
  */



#include "main.h"

extern const struct Radio_s Radio;


void vTask_radio_linktest(void const * argument)
{
  LOG_INFO_CONST("vTask_radio_linktest started!");
#if !LOG_PRINT_IMMEDIATELY
  log_flush();
#endif /* LOG_PRINT_IMMEDIATELY */

  // vTaskDelay(pdMS_TO_TICKS(1000));

  /* Infinite loop */
  for(;;)
  {
    // delay with option to execute immediately if triggered (e.g. by radio interrupt)
    ulTaskNotifyTake( pdTRUE,               /* Clear notify value (binary semaphore style). */
                      pdMS_TO_TICKS(100) ); /* Timeout. */
    Radio.IrqProcess();
  }
}

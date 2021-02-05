/**
  ******************************************************************************
  * Flora
  ******************************************************************************
  * @author Roman Trub
  * @file
  * @brief
  *
  *
  ******************************************************************************
  */



#include "main.h"

#if TESTING_ENABLE

extern const struct Radio_s Radio;

void vTask_test(void const * argument)
{
  GPIO_PinState sig1, sig2;

  vTaskDelay(pdMS_TO_TICKS(1000));
  LOG_INFO_CONST("test task started!");
#if !LOG_PRINT_IMMEDIATELY
  log_flush();
#endif /* LOG_PRINT_IMMEDIATELY */

  // // Test different toa calc functions
  // uint8_t modulation = 7;  // flora modulation (see radio_constants.c)
  // uint8_t payload_len = 30; // in bytes
  // uint32_t toa1, toa2, toa3, toa4;
  // uint64_t toa5;
  // gloria_set_modulation(modulation);
  // gloria_set_band(40);
  //
  // toa1 = gloria_get_toa(payload_len);                  // in us
  // toa2 = gloria_get_toa_sl(payload_len, modulation);   // in us
  // toa3 = radio_get_toa(payload_len + GLORIA_HEADER_LENGTH, modulation);       // in us
  // toa4 = radio_get_toa_hs(payload_len + GLORIA_HEADER_LENGTH, modulation);    // in hs_timer ticks
  // // toa5 = radio_lookup_toa(modulation, payload_len + GLORIA_HEADER_LENGTH);    // in hs_timer ticks
  // LOG_INFO("toa1: %u", toa1);
  // LOG_INFO("toa2: %u", toa2);
  // LOG_INFO("toa3: %u", toa3);
  // LOG_INFO("toa4: %u", toa4);
  // // LOG_INFO("toa5: %u", (uint32_t) (toa5*1000000UL/HS_TIMER_FREQUENCY));
  //
  // // print ToA for all modulations
  // int8_t i;
  // for (i=10; i>=0; i--) {
  //   toa2 = gloria_get_toa_sl(payload_len, i);   // in us
  //   LOG_INFO("toa Semtech (mod=%d): %u", i, toa2);
  //   // toa5 = radio_lookup_toa(i, payload_len + GLORIA_HEADER_LENGTH);    // in hs_timer ticks
  //   // LOG_INFO("toa LUT (mod=%d):     %u", i, (uint32_t) (toa5*1000000UL/HS_TIMER_FREQUENCY));
  // }


  // // Test different flood duration functions/macros
  // uint8_t n_tx = 3;
  // uint8_t n_hops = 6;
  // uint8_t len = 30;
  // LOG_INFO("FLOOD_DURATION_MS_OLD: %lu ms", GLORIA_INTERFACE_FLOOD_DURATION_MS_OLD(n_tx, n_hops, len));
  // LOG_INFO("FLOOD_DURATION_MS:     %lu ms", GLORIA_INTERFACE_FLOOD_DURATION_MS(n_tx, n_hops, len));
  // LOG_INFO("FLOOD_DURATION_OLD:    %lu lptimer ticks", GLORIA_INTERFACE_FLOOD_DURATION_OLD(n_tx, n_hops, len));
  // LOG_INFO("FLOOD_DURATION:        %lu lptimer ticks", GLORIA_INTERFACE_FLOOD_DURATION(n_tx, n_hops, len));

#if !LOG_PRINT_IMMEDIATELY
  log_flush();
#endif /* LOG_PRINT_IMMEDIATELY */

  // // send dummy flood to measure real toa
  // static uint8_t payload_tx[] = "00abcdefgh01abcdefgh02abcdefgh03abcdefgh04abcdefgh05abcdefgh06abcdefgh07abcdefgh08abcdefgh09abcdefgh10abcdefgh11abcdefgh12abcdefgh13abcdefgh14abcdefgh15abcdefgh16abcdefgh17abcdefgh18abcdefgh19abcdefgh20abcdefgh21abcdefgh22abcdefgh23abcdefgh24abcdefgh25abc";
  // gloria_start(
  //   NODE_ID,                          // initiator_id
  //   payload_tx,                       // *payload
  //   payload_len,                      // payload_len
  //   3,                                // n_tx_max
  //   false                             // sync_slot
  // );
  // vTaskDelay(pdMS_TO_TICKS(10000));
  // gloria_stop();


  // send single message (to measure toa)
  uint8_t buffer[] = {0, 0, 0, 0, 0};
  RadioEvents_t radioEvents;

  Radio.Init(&radioEvents);

  Radio.Standby();
  Radio.SetChannel(869400000);
  Radio.SetTxConfig(
    1, //RadioModems_t modem,
    0, //int8_t power,
    0, //uint32_t fdev,
    0, //uint32_t bandwidth,
    12, //uint32_t datarate,
    1, //uint8_t coderate,
    12, //uint16_t preambleLen,
    0, //bool fixLen,
    0, //bool crcOn,
    0, //bool freqHopOn,
    0, //uint8_t hopPeriod,
    0, //bool iqInverted,
    0 //uint32_t timeout
  );
  Radio.Send(&buffer, 3);

  LOG_INFO("Msg sent!");

#if !LOG_PRINT_IMMEDIATELY
  log_flush();
#endif /* LOG_PRINT_IMMEDIATELY */


  /* Infinite loop */
  for(;;)
  {
// #if FLOCKLAB
//     sig1 = FLOCKLAB_PIN_GET(FLOCKLAB_SIG1);
//     sig2 = FLOCKLAB_PIN_GET(FLOCKLAB_SIG2);
//     LOG_INFO("sig1=%d, sig2=%d", sig1, sig2);
//   #if !LOG_PRINT_IMMEDIATELY
//     log_flush();
//   #endif /* LOG_PRINT_IMMEDIATELY */
// #endif /* FLOCKLAB */

    vTaskDelay(pdMS_TO_TICKS(portMAX_DELAY));
  }
}

#endif /* TESTING_ENABLE */

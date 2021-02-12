/**
  ******************************************************************************
  * Flora
  ******************************************************************************
  * @author Roman Trub
  * @file   task_linktest.c
  * @brief
  *
  *
  ******************************************************************************
  */



#include "main.h"


void vTask_linktest(void const * argument)
{
  const uint16_t TESTCONFIG_NODE_LIST[TESTCONFIG_NUM_NODES] = {
    TESTCONFIG_NODE_IDS
  };

  const uint32_t SetupTime  = TESTCONFIG_SETUP_TIME;
  const uint32_t StartDelay = TESTCONFIG_START_DELAY;
  const uint32_t StopDelay  = TESTCONFIG_STOP_DELAY;
  const uint32_t TxSlack    = TESTCONFIG_TX_SLACK;

  vTaskDelay(pdMS_TO_TICKS(1000));
  LOG_INFO_CONST("linktest task started!");
  LOG_INFO("{\"type\":\"TestConfig\","
           "\"numNodes\":%d,"
           "\"numTx\":%d,"
           "\"setupTime\":%d,"
           "\"startDelay\":%d,"
           "\"stopDelay\":%d,"
           "\"txSlack\":%d,"
           "\"key\":\"%s\"}",
    TESTCONFIG_NUM_NODES,
    TESTCONFIG_NUM_TX,
    TESTCONFIG_SETUP_TIME,
    TESTCONFIG_START_DELAY,
    TESTCONFIG_STOP_DELAY,
    TESTCONFIG_TX_SLACK,
    TESTCONFIG_KEY
  );

  LOG_INFO("{\"type\":\"RadioConfig\","
           "\"txPower\":%d,"
           "\"frequency\":%lu,"
           "\"modulation\":%d,"
           "\"datarate\":%d,"
           "\"bandwidth\":%d,"
           "\"coderate\":%d,"
           "\"preamleLength\":%d,"
           "\"implicitHeader\":%d,"
           "\"crcOn\":%d}",
    RADIOCONFIG_TX_POWER,
    RADIOCONFIG_FREQUENCY,
    RADIOCONFIG_MODULATION,
    RADIOCONFIG_DATARATE,
    RADIOCONFIG_BANDWIDTH,
    RADIOCONFIG_CODERATE,
    RADIOCONFIG_PREAMBLE_LEN,
    RADIOCONFIG_IMPLICIT_HEADER,
    RADIOCONFIG_CRC_ON
  );

#if !LOG_PRINT_IMMEDIATELY
  log_flush();
#endif /* LOG_PRINT_IMMEDIATELY */

  linktest_radio_init();

  linktest_message_t msg = {
    .counter=0,
    .key=TESTCONFIG_KEY,
  };

  // /* Calculate TimeOnAir */
  uint16_t key_length = 0;
  key_length = strlen(msg.key)+1;
  key_length = (key_length > 254) ? 254 : key_length;
  uint32_t TxTime = Radio.TimeOnAir(
    RADIOCONFIG_MODULATION,
    RADIOCONFIG_BANDWIDTH,
    RADIOCONFIG_DATARATE,
    RADIOCONFIG_CODERATE,
    RADIOCONFIG_PREAMBLE_LEN,
    0,  // fixLen
    sizeof(msg.counter) + key_length,
    RADIOCONFIG_CRC_ON
  )/1000; // TimeOnAir returns us, we need ms
  uint32_t TxPeriod = TxTime + TxSlack;
  uint32_t RoundPeriod = SetupTime + StartDelay + (TESTCONFIG_NUM_TX-1)*TxPeriod + TxTime + StopDelay;

  // workaround for no successful rx in FSK mode if no Tx happened before
  // simple Radio.Send alone with 0 size payload does not work
  if (RADIOCONFIG_MODULATION == MODEM_FSK) {
    linktest_set_tx_config_fsk();
    Radio.Standby();
    Radio.Send((uint8_t*) &msg, sizeof(msg.counter) + key_length);
    // Radio.Send((uint8_t*) &msg, 0);
  }

  /* wait for sync signal */
  while (FLOCKLAB_PIN_GET(FLOCKLAB_SIG1) == 0) {
    vTaskDelay(pdMS_TO_TICKS(1));
  };

  /* initialize time reference */
  TickType_t xLastRoundPeriodStart = xTaskGetTickCount();
  TickType_t xTmpTs = xLastRoundPeriodStart;

  uint8_t nodeIdx;
  uint16_t txIdx;
  for (nodeIdx=0; nodeIdx<TESTCONFIG_NUM_NODES; nodeIdx++) {
    // indicate start of round (indication happens before SetupTime)
    FLOCKLAB_PIN_SET(FLOCKLAB_INT1);
    vTaskDelay(pdMS_TO_TICKS(1));
    FLOCKLAB_PIN_CLR(FLOCKLAB_INT1);
    if (TESTCONFIG_NODE_LIST[nodeIdx] == NODE_ID) {
      /* Node is transmitting in this round */
      // prepare tx
      if (RADIOCONFIG_MODULATION == MODEM_LORA) {
        linktest_set_tx_config_lora();
      }
      else {
        linktest_set_tx_config_fsk();
      }
      Radio.Standby();

      uint16_t key_length = 0;
      linktest_message_t msg = {
        .counter=0,
        .key=TESTCONFIG_KEY,
      };

      // wait
      xTmpTs = xLastRoundPeriodStart;
      vTaskDelayUntil(&xTmpTs, pdMS_TO_TICKS(SetupTime));

      // start of round
      LOG_INFO("{\"type\":\"StartOfRound\",\"node\":%d}", TESTCONFIG_NODE_LIST[nodeIdx]);
      // wait
      xTmpTs = xLastRoundPeriodStart;
      vTaskDelayUntil(&xTmpTs, pdMS_TO_TICKS(SetupTime + StartDelay));

      for (txIdx=0; txIdx<TESTCONFIG_NUM_TX; txIdx++) {
        // send
        msg.counter = txIdx;
        key_length  = strlen(msg.key);
        key_length  = (key_length > 254) ? 254 : key_length;
        Radio.Send((uint8_t*) &msg, sizeof(msg.counter) + key_length);

        // wait, if not last iteration
        if (txIdx < (TESTCONFIG_NUM_TX-1)) {
          xTmpTs = xLastRoundPeriodStart;
          vTaskDelayUntil(&xTmpTs, pdMS_TO_TICKS(SetupTime + StartDelay + (txIdx+1)*TxPeriod));
        }
      }
      // wait
      vTaskDelayUntil(&xLastRoundPeriodStart, pdMS_TO_TICKS(RoundPeriod));
    }
    else {
      /* Node is receiving in this round */
      if (RADIOCONFIG_MODULATION == MODEM_LORA) {
        linktest_set_rx_config_lora();
      }
      else {
        linktest_set_rx_config_fsk();
      }

      // wait
      xTmpTs = xLastRoundPeriodStart;
      vTaskDelayUntil(&xTmpTs, pdMS_TO_TICKS(SetupTime));
      // start of round
      LOG_INFO("{\"type\":\"StartOfRound\",\"node\":%d}", TESTCONFIG_NODE_LIST[nodeIdx]);

      // start rx mode (with deactivated preamble IRQs)
      Radio.RxBoostedMask(LINKTEST_IRQ_MASK);
      // LOG_INFO("RxBoostedMask executed");

      // wait
      vTaskDelayUntil(&xLastRoundPeriodStart, pdMS_TO_TICKS(RoundPeriod));

      Radio.Standby();
    }

    LOG_INFO("{\"type\":\"EndOfRound\",\"node\":%d}", TESTCONFIG_NODE_LIST[nodeIdx]);

#if !LOG_PRINT_IMMEDIATELY
    log_flush();
#endif /* LOG_PRINT_IMMEDIATELY */
  }

  FLOCKLAB_PIN_SET(FLOCKLAB_INT1);
  vTaskDelay(pdMS_TO_TICKS(500));
  FLOCKLAB_PIN_CLR(FLOCKLAB_INT1);

  /* Infinite loop */
  for(;;)
  {
    FLOCKLAB_PIN_SET(FLOCKLAB_INT1);
    vTaskDelay(pdMS_TO_TICKS(1));
    FLOCKLAB_PIN_CLR(FLOCKLAB_INT1);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


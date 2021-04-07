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

extern const uint16_t TESTCONFIG_NODE_LIST[];


void vTask_linktest(void const * argument)
{
  const uint32_t SetupTime  = TESTCONFIG_SETUP_TIME;
  const uint32_t StartDelay = TESTCONFIG_START_DELAY;
  const uint32_t StopDelay  = TESTCONFIG_STOP_DELAY;
  const uint32_t SlotGap    = TESTCONFIG_SLOT_GAP;

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
    TESTCONFIG_NUM_SLOTS,
    TESTCONFIG_SETUP_TIME,
    TESTCONFIG_START_DELAY,
    TESTCONFIG_STOP_DELAY,
    TESTCONFIG_SLOT_GAP,
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

  uint32_t SlotTime;
  linktest_init(&SlotTime);

  uint32_t SlotPeriod = SlotTime + SlotGap;
  uint32_t RoundPeriod = SetupTime + StartDelay + (TESTCONFIG_NUM_SLOTS-1)*SlotPeriod + SlotTime + StopDelay;

  /* wait for sync signal */
  while (FLOCKLAB_PIN_GET(FLOCKLAB_SIG1) == 0) {
    vTaskDelay(pdMS_TO_TICKS(1));
  };

  /* initialize time reference */
  TickType_t xLastRoundPeriodStart = xTaskGetTickCount();
  TickType_t xTmpTs = xLastRoundPeriodStart;

  uint8_t roundIdx;
  uint16_t slotIdx;
  for (roundIdx=0; roundIdx<TESTCONFIG_NUM_NODES; roundIdx++) {
    // indicate start of round (indication happens before SetupTime)
    FLOCKLAB_PIN_SET(FLOCKLAB_INT1);
    vTaskDelay(pdMS_TO_TICKS(1));
    FLOCKLAB_PIN_CLR(FLOCKLAB_INT1);

    // wait SetupTime
    xTmpTs = xLastRoundPeriodStart;
    vTaskDelayUntil(&xTmpTs, pdMS_TO_TICKS(SetupTime));
    // start of round
    LOG_INFO("{\"type\":\"StartOfRound\",\"round\":%d,\"node\":%d}", roundIdx, TESTCONFIG_NODE_LIST[roundIdx]);

    linktest_round_pre(roundIdx);

    for (slotIdx=0; slotIdx<TESTCONFIG_NUM_SLOTS; slotIdx++) {
      // wait StartDelay
      xTmpTs = xLastRoundPeriodStart;
      vTaskDelayUntil(&xTmpTs, pdMS_TO_TICKS(SetupTime + StartDelay));

      linktest_slot(roundIdx, slotIdx, SetupTime + StartDelay + (slotIdx+1)*SlotPeriod);
      // wait, if not last iteration
      if (slotIdx < (TESTCONFIG_NUM_SLOTS-1)) {
        xTmpTs = xLastRoundPeriodStart;
        vTaskDelayUntil(&xTmpTs, pdMS_TO_TICKS(SetupTime + StartDelay + (slotIdx+1)*SlotPeriod));
      }

      // wait, if not last iteration
      if (slotIdx < (TESTCONFIG_NUM_SLOTS-1)) {
        xTmpTs = xLastRoundPeriodStart;
        vTaskDelayUntil(&xTmpTs, pdMS_TO_TICKS(SetupTime + StartDelay + (slotIdx+1)*SlotPeriod));
      }
    }

    vTaskDelayUntil(&xLastRoundPeriodStart, pdMS_TO_TICKS(RoundPeriod));

    linktest_round_post(roundIdx);

    LOG_INFO("{\"type\":\"EndOfRound\",\"round\":%d,\"node\":%d}", roundIdx, TESTCONFIG_NODE_LIST[roundIdx]);

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

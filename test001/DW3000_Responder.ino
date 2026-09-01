/*
 * ============================================================
 * DW3000 UWB - SS-TWR RESPONDER
 * Raw Ranging Characterization Firmware
 * ============================================================
 *
 * Hardware : Makerfabs ESP32 UWB DW3000
 * Function : Responder
 *
 * The responder does NOT calculate distance.
 * Distance is calculated and recorded by the Initiator.
 * ============================================================
 */

#include "dw3000.h"
#include "SPI.h"

/* ============================================================
   DEVICE IDENTIFICATION
   ============================================================ */

const char DEVICE_ID[] = "DW02";


/* ============================================================
   HARDWARE PINS
   ============================================================ */

constexpr uint8_t PIN_RST = 27;
constexpr uint8_t PIN_IRQ = 34;
constexpr uint8_t PIN_SS  = 4;


/* ============================================================
   ANTENNA DELAYS
   Keep identical for all Stage-1 devices.
   ============================================================ */

constexpr uint16_t TX_ANT_DLY = 16385;
constexpr uint16_t RX_ANT_DLY = 16385;


/* ============================================================
   SS-TWR TIMING
   ============================================================ */

constexpr uint16_t POLL_RX_TO_RESP_TX_DLY_UUS = 900;


/* ============================================================
   MESSAGE DEFINITIONS
   ============================================================ */

constexpr uint8_t MSG_COMMON_LEN = 10;
constexpr uint8_t MSG_SEQ_IDX    = 2;

constexpr uint8_t RESP_POLL_RX_TS_IDX = 10;
constexpr uint8_t RESP_TX_TS_IDX      = 14;


// Expected poll message
static uint8_t pollMsg[] = {
  0x41, 0x88, 0,
  0xCA, 0xDE,
  'W', 'A', 'V', 'E',
  0xE0,
  0, 0
};


// Response message
static uint8_t responseMsg[] = {
  0x41, 0x88, 0,
  0xCA, 0xDE,
  'V', 'E', 'W', 'A',
  0xE1,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


static uint8_t rxBuffer[20];


/* ============================================================
   DW3000 RADIO CONFIGURATION
   Must match the Initiator.
   ============================================================ */

static dwt_config_t uwbConfig = {
  5,                // Channel
  DWT_PLEN_128,     // Preamble
  DWT_PAC8,         // PAC
  9,                // TX preamble code
  9,                // RX preamble code
  1,                // Non-standard 8-symbol SFD
  DWT_BR_6M8,       // 6.8 Mb/s
  DWT_PHRMODE_STD,
  DWT_PHRRATE_STD,
  129,              // SFD timeout
  DWT_STS_MODE_OFF,
  DWT_STS_LEN_64,
  DWT_PDOA_M0
};


/* ============================================================
   GLOBAL VARIABLES
   ============================================================ */

extern SPISettings _fastSPI;
extern dwt_txconfig_t txconfig_options;

static uint8_t frameSeq = 0;


/* ============================================================
   INITIALIZATION
   ============================================================ */

void initializeDW3000()
{
  UART_init();

  _fastSPI = SPISettings(
    7000000L,
    MSBFIRST,
    SPI_MODE0
  );

  spiBegin(PIN_IRQ, PIN_RST);
  spiSelect(PIN_SS);

  delay(2);

  dwt_softreset();

  delay(2);


  while (!dwt_checkidlerc())
  {
    Serial.println("# ERROR: DW3000 IDLE FAILED");
    delay(1000);
  }


  if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
  {
    Serial.println("# ERROR: DW3000 INIT FAILED");

    while (true)
    {
      delay(1000);
    }
  }


  dwt_setleds(
    DWT_LEDS_ENABLE |
    DWT_LEDS_INIT_BLINK
  );


  if (dwt_configure(&uwbConfig) == DWT_ERROR)
  {
    Serial.println("# ERROR: DW3000 CONFIG FAILED");

    while (true)
    {
      delay(1000);
    }
  }


  // RF configuration
  dwt_configuretxrf(
    &txconfig_options
  );


  // Default antenna delays
  dwt_settxantennadelay(
    TX_ANT_DLY
  );

  dwt_setrxantennadelay(
    RX_ANT_DLY
  );


  // Enable PA and LNA
  dwt_setlnapamode(
    DWT_LNA_ENABLE |
    DWT_PA_ENABLE
  );


  // No timeout while waiting for poll.
  dwt_setrxtimeout(0);
}


/* ============================================================
   STARTUP INFORMATION
   ============================================================ */

void printStartupInformation()
{
  Serial.println();
  Serial.println("# ========================================");
  Serial.println("# DW3000 RAW RANGING - RESPONDER");
  Serial.println("# ========================================");

  Serial.print("# DEVICE_ID=");
  Serial.println(DEVICE_ID);

  Serial.println("# PROTOCOL=SS-TWR");
  Serial.println("# CHANNEL=5");
  Serial.println("# TX_ANT_DLY=16385");
  Serial.println("# RX_ANT_DLY=16385");

  Serial.println("# RESPONDER_READY");
  Serial.println("# ========================================");
}


/* ============================================================
   SETUP
   ============================================================ */

void setup()
{
  initializeDW3000();

  delay(500);

  printStartupInformation();
}


/* ============================================================
   MAIN RESPONDER LOOP
   ============================================================ */

void loop()
{
  /* ----------------------------------------------------------
     1. Enable receiver
     ---------------------------------------------------------- */

  dwt_setrxtimeout(0);

  dwt_rxenable(
    DWT_START_RX_IMMEDIATE
  );


  /* ----------------------------------------------------------
     2. Wait for poll frame
     ---------------------------------------------------------- */

  uint32_t statusReg;

  do
  {
    statusReg =
      dwt_read32bitreg(SYS_STATUS_ID);

  } while (
    !(statusReg &
      (
        SYS_STATUS_RXFCG_BIT_MASK |
        SYS_STATUS_ALL_RX_ERR
      )
    )
  );


  /* ----------------------------------------------------------
     3. Valid received frame
     ---------------------------------------------------------- */

  if (statusReg & SYS_STATUS_RXFCG_BIT_MASK)
  {
    dwt_write32bitreg(
      SYS_STATUS_ID,
      SYS_STATUS_RXFCG_BIT_MASK
    );


    const uint32_t frameLength =
      dwt_read32bitreg(RX_FINFO_ID)
      & RXFLEN_MASK;


    if (frameLength > sizeof(rxBuffer))
    {
      return;
    }


    dwt_readrxdata(
      rxBuffer,
      frameLength,
      0
    );


    if (frameLength < MSG_COMMON_LEN)
    {
      return;
    }


    /* Ignore sequence field for comparison. */
    rxBuffer[MSG_SEQ_IDX] = 0;


    // Ignore frames that are not our ranging poll.
    if (
      memcmp(
        rxBuffer,
        pollMsg,
        MSG_COMMON_LEN
      ) != 0
    )
    {
      return;
    }


    /* --------------------------------------------------------
       4. Poll received correctly
       -------------------------------------------------------- */

    uint64_t pollRxTimestamp =
      get_rx_timestamp_u64();


    /* --------------------------------------------------------
       5. Calculate scheduled response transmission time
       -------------------------------------------------------- */

    uint32_t responseTxTime =
      (
        pollRxTimestamp +
        (
          POLL_RX_TO_RESP_TX_DLY_UUS *
          UUS_TO_DWT_TIME
        )
      )
      >> 8;


    dwt_setdelayedtrxtime(
      responseTxTime
    );


    /* --------------------------------------------------------
       6. Calculate timestamp that will be placed in response
       -------------------------------------------------------- */

    uint64_t responseTxTimestamp =
      (
        (
          (uint64_t)
          (responseTxTime & 0xFFFFFFFEUL)
        )
        << 8
      )
      + TX_ANT_DLY;


    /* --------------------------------------------------------
       7. Insert timestamps into response message
       -------------------------------------------------------- */

    resp_msg_set_ts(
      &responseMsg[RESP_POLL_RX_TS_IDX],
      pollRxTimestamp
    );

    resp_msg_set_ts(
      &responseMsg[RESP_TX_TS_IDX],
      responseTxTimestamp
    );


    responseMsg[MSG_SEQ_IDX] =
      frameSeq;


    /* --------------------------------------------------------
       8. Write response into DW3000
       -------------------------------------------------------- */

    dwt_writetxdata(
      sizeof(responseMsg),
      responseMsg,
      0
    );

    dwt_writetxfctrl(
      sizeof(responseMsg),
      0,
      1
    );


    /* --------------------------------------------------------
       9. Scheduled transmission
       -------------------------------------------------------- */

    int txResult =
      dwt_starttx(
        DWT_START_TX_DELAYED
      );


    if (txResult == DWT_SUCCESS)
    {
      // Wait until transmission is completed.
      while (
        !(
          dwt_read32bitreg(SYS_STATUS_ID)
          &
          SYS_STATUS_TXFRS_BIT_MASK
        )
      )
      {
        // Waiting
      }


      // Clear TX completed flag.
      dwt_write32bitreg(
        SYS_STATUS_ID,
        SYS_STATUS_TXFRS_BIT_MASK
      );


      frameSeq++;
    }
    else
    {
      // Useful only for debugging.
      Serial.println(
        "# ERROR: RESPONDER_DELAYED_TX_FAILED"
      );
    }
  }

  /* ----------------------------------------------------------
     RX error
     ---------------------------------------------------------- */

  else
  {
    dwt_write32bitreg(
      SYS_STATUS_ID,
      SYS_STATUS_ALL_RX_ERR
    );
  }
}

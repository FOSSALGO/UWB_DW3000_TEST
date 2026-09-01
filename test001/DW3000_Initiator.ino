/*
 * ============================================================
 * DW3000 UWB - SS-TWR INITIATOR
 * Raw Ranging Characterization Firmware
 * ============================================================
 *
 * Hardware : Makerfabs ESP32 UWB DW3000
 * Function : Initiator / ranging device
 *
 * Output CSV:
 * type,attempt_idx,valid_idx,elapsed_ms,frame_seq,status,
 * distance_m,clock_offset_ratio,status_reg
 *
 * IMPORTANT:
 * - No filtering
 * - No calibration
 * - No averaging
 * - Raw ranging result only
 * ============================================================
 */

#include "dw3000.h"
#include "SPI.h"

/* ============================================================
   DEVICE IDENTIFICATION
   Change this for each physical device.
   Example: DW01, DW02, DW03, ...
   ============================================================ */

const char DEVICE_ID[] = "DW01";


/* ============================================================
   HARDWARE PINS - Makerfabs ESP32 UWB DW3000
   ============================================================ */

constexpr uint8_t PIN_RST = 27;
constexpr uint8_t PIN_IRQ = 34;
constexpr uint8_t PIN_SS  = 4;


/* ============================================================
   EXPERIMENT SETTINGS
   ============================================================ */

// Baseline Makerfabs = 1000 ms.
// Change to 200 ms only after sampling-rate pilot test.
constexpr uint32_t RANGE_INTERVAL_MS = 1000;

// Keep identical on all devices during Stage 1.
constexpr uint16_t TX_ANT_DLY = 16385;
constexpr uint16_t RX_ANT_DLY = 16385;


/* ============================================================
   SS-TWR TIMING
   ============================================================ */

constexpr uint16_t POLL_TX_TO_RESP_RX_DLY_UUS = 600;
constexpr uint16_t RESP_RX_TIMEOUT_UUS        = 400;


/* ============================================================
   MESSAGE DEFINITIONS
   ============================================================ */

constexpr uint8_t MSG_COMMON_LEN = 10;
constexpr uint8_t MSG_SEQ_IDX    = 2;

constexpr uint8_t RESP_POLL_RX_TS_IDX = 10;
constexpr uint8_t RESP_TX_TS_IDX      = 14;

// Poll message
static uint8_t pollMsg[] = {
  0x41, 0x88, 0,
  0xCA, 0xDE,
  'W', 'A', 'V', 'E',
  0xE0,
  0, 0
};

// Expected response
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
   ============================================================ */

static dwt_config_t uwbConfig = {
  5,                // Channel
  DWT_PLEN_128,     // Preamble length
  DWT_PAC8,         // PAC size
  9,                // TX preamble code
  9,                // RX preamble code
  1,                // Non-standard 8-symbol SFD
  DWT_BR_6M8,       // Data rate 6.8 Mb/s
  DWT_PHRMODE_STD,  // Standard PHR
  DWT_PHRRATE_STD,  // Standard PHR rate
  129,              // SFD timeout
  DWT_STS_MODE_OFF, // STS disabled
  DWT_STS_LEN_64,
  DWT_PDOA_M0
};


/* ============================================================
   GLOBAL VARIABLES
   ============================================================ */

extern SPISettings _fastSPI;
extern dwt_txconfig_t txconfig_options;

static uint8_t frameSeq = 0;

static uint32_t attemptCount = 0;
static uint32_t validCount   = 0;


/* ============================================================
   INITIALIZATION
   ============================================================ */

void initializeDW3000()
{
  // UART_init() already sets Serial to 115200 baud.
  UART_init();

  _fastSPI = SPISettings(
    7000000L,
    MSBFIRST,
    SPI_MODE0
  );

  spiBegin(PIN_IRQ, PIN_RST);
  spiSelect(PIN_SS);

  delay(2);

  // Software reset
  dwt_softreset();

  delay(2);

  // Wait until DW3000 enters IDLE_RC
  while (!dwt_checkidlerc())
  {
    Serial.println("# ERROR: DW3000 IDLE FAILED");
    delay(1000);
  }

  // Initialize DW3000
  if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
  {
    Serial.println("# ERROR: DW3000 INIT FAILED");

    while (true)
    {
      delay(1000);
    }
  }

  // Enable LEDs for basic hardware indication
  dwt_setleds(
    DWT_LEDS_ENABLE |
    DWT_LEDS_INIT_BLINK
  );

  // Configure UWB radio
  if (dwt_configure(&uwbConfig) == DWT_ERROR)
  {
    Serial.println("# ERROR: DW3000 CONFIG FAILED");

    while (true)
    {
      delay(1000);
    }
  }

  // Configure RF parameters
  dwt_configuretxrf(&txconfig_options);

  // Default antenna delay
  dwt_settxantennadelay(TX_ANT_DLY);
  dwt_setrxantennadelay(RX_ANT_DLY);

  // RX timing after poll transmission
  dwt_setrxaftertxdelay(
    POLL_TX_TO_RESP_RX_DLY_UUS
  );

  dwt_setrxtimeout(
    RESP_RX_TIMEOUT_UUS
  );

  // Enable LNA and PA
  dwt_setlnapamode(
    DWT_LNA_ENABLE |
    DWT_PA_ENABLE
  );
}


/* ============================================================
   CSV OUTPUT
   ============================================================ */

void printStartupInformation()
{
  Serial.println();
  Serial.println("# ========================================");
  Serial.println("# DW3000 RAW RANGING - INITIATOR");
  Serial.println("# ========================================");

  Serial.print("# DEVICE_ID=");
  Serial.println(DEVICE_ID);

  Serial.println("# PROTOCOL=SS-TWR");
  Serial.println("# CHANNEL=5");
  Serial.println("# TX_ANT_DLY=16385");
  Serial.println("# RX_ANT_DLY=16385");

  Serial.print("# RANGE_INTERVAL_MS=");
  Serial.println(RANGE_INTERVAL_MS);

  Serial.println("# ========================================");

  // CSV header
  Serial.println(
    "type,attempt_idx,valid_idx,elapsed_ms,"
    "frame_seq,status,distance_m,"
    "clock_offset_ratio,status_reg"
  );
}


void logSuccess(
  uint8_t seq,
  double distance,
  float clockOffsetRatio,
  uint32_t statusReg
)
{
  validCount++;

  Serial.printf(
    "DATA,%lu,%lu,%lu,%u,OK,%.6f,%.9f,0x%08lX\n",
    attemptCount,
    validCount,
    millis(),
    seq,
    distance,
    clockOffsetRatio,
    statusReg
  );
}


void logFailure(
  uint8_t seq,
  const char *reason,
  uint32_t statusReg
)
{
  Serial.printf(
    "FAIL,%lu,,%lu,%u,%s,,,0x%08lX\n",
    attemptCount,
    millis(),
    seq,
    reason,
    statusReg
  );
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
   MAIN RANGING LOOP
   ============================================================ */

void loop()
{
  attemptCount++;

  // Save sequence number used in this measurement.
  const uint8_t currentSeq = frameSeq;

  pollMsg[MSG_SEQ_IDX] = currentSeq;

  frameSeq++;

  /* ----------------------------------------------------------
     1. Prepare poll message
     ---------------------------------------------------------- */

  dwt_write32bitreg(
    SYS_STATUS_ID,
    SYS_STATUS_TXFRS_BIT_MASK
  );

  dwt_writetxdata(
    sizeof(pollMsg),
    pollMsg,
    0
  );

  dwt_writetxfctrl(
    sizeof(pollMsg),
    0,
    1
  );


  /* ----------------------------------------------------------
     2. Transmit poll and automatically wait for response
     ---------------------------------------------------------- */

  int txResult = dwt_starttx(
    DWT_START_TX_IMMEDIATE |
    DWT_RESPONSE_EXPECTED
  );

  if (txResult == DWT_ERROR)
  {
    logFailure(
      currentSeq,
      "TX_START_ERROR",
      0
    );

    delay(RANGE_INTERVAL_MS);
    return;
  }


  /* ----------------------------------------------------------
     3. Wait for response, timeout, or RX error
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
        SYS_STATUS_ALL_RX_TO |
        SYS_STATUS_ALL_RX_ERR
      )
    )
  );


  /* ----------------------------------------------------------
     4. Successful frame reception
     ---------------------------------------------------------- */

  if (statusReg & SYS_STATUS_RXFCG_BIT_MASK)
  {
    dwt_write32bitreg(
      SYS_STATUS_ID,
      SYS_STATUS_RXFCG_BIT_MASK |
      SYS_STATUS_TXFRS_BIT_MASK
    );

    const uint32_t frameLength =
      dwt_read32bitreg(RX_FINFO_ID)
      & RXFLEN_MASK;


    // Protect RX buffer
    if (frameLength > sizeof(rxBuffer))
    {
      logFailure(
        currentSeq,
        "FRAME_TOO_LONG",
        statusReg
      );

      delay(RANGE_INTERVAL_MS);
      return;
    }


    // Read response frame
    dwt_readrxdata(
      rxBuffer,
      frameLength,
      0
    );


    if (frameLength < MSG_COMMON_LEN)
    {
      logFailure(
        currentSeq,
        "SHORT_FRAME",
        statusReg
      );

      delay(RANGE_INTERVAL_MS);
      return;
    }


    /* Ignore sequence number during frame comparison. */
    rxBuffer[MSG_SEQ_IDX] = 0;


    if (
      memcmp(
        rxBuffer,
        responseMsg,
        MSG_COMMON_LEN
      ) != 0
    )
    {
      logFailure(
        currentSeq,
        "UNEXPECTED_FRAME",
        statusReg
      );

      delay(RANGE_INTERVAL_MS);
      return;
    }


    /* --------------------------------------------------------
       5. Read local timestamps
       -------------------------------------------------------- */

    uint32_t pollTxTs =
      dwt_readtxtimestamplo32();

    uint32_t respRxTs =
      dwt_readrxtimestamplo32();


    /* --------------------------------------------------------
       6. Read clock offset
       -------------------------------------------------------- */

    float clockOffsetRatio =
      ((float)dwt_readclockoffset())
      / (uint32_t)(1UL << 26);


    /* --------------------------------------------------------
       7. Read timestamps inserted by responder
       -------------------------------------------------------- */

    uint32_t pollRxTs;
    uint32_t respTxTs;

    resp_msg_get_ts(
      &rxBuffer[RESP_POLL_RX_TS_IDX],
      &pollRxTs
    );

    resp_msg_get_ts(
      &rxBuffer[RESP_TX_TS_IDX],
      &respTxTs
    );


    /* --------------------------------------------------------
       8. Calculate Time of Flight
       -------------------------------------------------------- */

    int32_t roundTripInitiator =
      (int32_t)(respRxTs - pollTxTs);

    int32_t replyResponder =
      (int32_t)(respTxTs - pollRxTs);


    double tof =
      (
        roundTripInitiator -
        replyResponder *
        (1.0 - clockOffsetRatio)
      )
      / 2.0;


    tof *= DWT_TIME_UNITS;


    /* --------------------------------------------------------
       9. Convert ToF to distance
       -------------------------------------------------------- */

    double distance =
      tof * SPEED_OF_LIGHT;


    /* --------------------------------------------------------
       10. Store RAW result
       -------------------------------------------------------- */

    logSuccess(
      currentSeq,
      distance,
      clockOffsetRatio,
      statusReg
    );
  }

  /* ----------------------------------------------------------
     RX timeout / error
     ---------------------------------------------------------- */

  else
  {
    dwt_write32bitreg(
      SYS_STATUS_ID,
      SYS_STATUS_ALL_RX_TO |
      SYS_STATUS_ALL_RX_ERR |
      SYS_STATUS_TXFRS_BIT_MASK
    );

    if (statusReg & SYS_STATUS_ALL_RX_TO)
    {
      logFailure(
        currentSeq,
        "RX_TIMEOUT",
        statusReg
      );
    }
    else
    {
      logFailure(
        currentSeq,
        "RX_ERROR",
        statusReg
      );
    }
  }


  /* ----------------------------------------------------------
     Delay before next ranging attempt
     ---------------------------------------------------------- */

  delay(RANGE_INTERVAL_MS);
}

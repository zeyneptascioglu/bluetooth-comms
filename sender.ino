/*  NANO 33 BLE / BLE Rev 2  ▸  SENDER  (Peripheral + USB debug)
 *  – identical public API and BLE packets –
 *  – now echoes each packet over Serial as well –
 */

#include <Arduino.h>
#include <mbed.h>
#include <events/mbed_events.h>
#include <ArduinoBLE.h>

/* ---------- public globals (unchanged) ------------------ */
const uint16_t TOP = 4000;
volatile  uint8_t  count = 0;
volatile  uint16_t filtered_data[2] = {0, 0};

uint32_t encode(uint16_t a, uint16_t b) { return (uint32_t)a << 12 | b; }

/* ---------- filter coefficients & state ---------------- */
const float hp_b[6] PROGMEM = { 0.5719106,-2.769239, 5.451519,-5.451519, 2.769239,-0.5719106};
const float hp_a[6] PROGMEM = { 1.0,      -3.7941800,5.9381560,-4.7090570,1.8627980,-0.2809456};
const float lp_b[4] PROGMEM = { 0.0000148, 0.0000443,0.0000443,0.0000148};
const float lp_a[4] PROGMEM = { 1.0,      -2.9349960,2.8749080,-0.9397943};

static float hp_x[2][6] = {{312,312,312,312,312,312},{312,312,312,312,312,312}};
static float hp_y[2][6] = {{0,0,0,0,0,0},{0,0,0,0,0,0}};
static float lp_x[2][4] = {{0,0,0,0},{0,0,0,0}};
static float lp_y[2][4] = {{0,0,0,0},{0,0,0,0}};

/* ---------- event infrastructure ----------------------- */
events::EventQueue queue(16 * EVENTS_EVENT_SIZE);
mbed::Ticker       ticker;

/* ---------- BLE objects -------------------------------- */
BLEService dataService("17b10000-b000-400d-b000-000000000001");          // custom 128-bit UUID
BLECharacteristic txChar("17b10001-b000-400d-b000-000000000001",
                         BLERead | BLENotify, 20);

void sampleTask()
{
  static uint8_t adc = 0;                                // 0 → A0, 1 → A1
  uint16_t raw = analogRead(adc == 0 ? A0 : A1);         // 12-bit unsigned
  raw >>= 2;
  
  /* ---- identical filter cascade ---------------------- */
  memmove(&hp_x[adc][1], &hp_x[adc][0], 5*sizeof(float));
  memmove(&hp_y[adc][1], &hp_y[adc][0], 5*sizeof(float));
  hp_x[adc][0] = (float)raw;
  hp_y[adc][0] = hp_b[0]*hp_x[adc][0] + hp_b[1]*hp_x[adc][1] + hp_b[2]*hp_x[adc][2] +
                 hp_b[3]*hp_x[adc][3] + hp_b[4]*hp_x[adc][4] + hp_b[5]*hp_x[adc][5] -
                 (hp_a[1]*hp_y[adc][1] + hp_a[2]*hp_y[adc][2] + hp_a[3]*hp_y[adc][3] +
                  hp_a[4]*hp_y[adc][4] + hp_a[5]*hp_y[adc][5]);

  memmove(&lp_x[adc][1], &lp_x[adc][0], 3*sizeof(float));
  memmove(&lp_y[adc][1], &lp_y[adc][0], 3*sizeof(float));
  lp_x[adc][0] = fabsf(hp_y[adc][0]);
  lp_y[adc][0] = lp_b[0]*lp_x[adc][0] + lp_b[1]*lp_x[adc][1] + lp_b[2]*lp_x[adc][2] +
                 lp_b[3]*lp_x[adc][3] -
                 (lp_a[1]*lp_y[adc][1] + lp_a[2]*lp_y[adc][2] + lp_a[3]*lp_y[adc][3]);

  filtered_data[adc] = (uint16_t)roundf(8 * lp_y[adc][0]);

  adc ^= 1;
  ++count;

  /* every 100 samples (≈20 Hz) → send packet */
  if (count >= 100)
  {
    count = 0;

    uint16_t a = filtered_data[0] > 1000 ? 1000 : filtered_data[0];
    uint16_t b = filtered_data[1] > 1000 ? 1000 : filtered_data[1];

    char msg[12];                              // up to “4294967295\n\0”
    sprintf(msg, "%lu\n", encode(a, b));       // ASCII, ≤11 B

    /* --- Bluetooth --- */
    txChar.writeValue((uint8_t*)msg, strlen(msg));

    /* --- USB debug --- */
    Serial.write(msg);                         // identical line
  }
}

void sampleISR() { queue.call(sampleTask); }

void setup()
{
  /* USB debug */
  Serial.begin(9600);            // same rate the PC expects
  while (!Serial && millis() < 3000) ;   // wait a moment (optional)

  analogReadResolution(12);

  /* BLE init ------------------------------------------- */
  if (!BLE.begin()) { while (1); }
  BLE.setLocalName("NanoSender");
  BLE.setAdvertisedService(dataService);
  dataService.addCharacteristic(txChar);
  BLE.addService(dataService);
  txChar.writeValue((uint8_t*)"boot\n", 5);
  BLE.advertise();
  // BLE.setTxPower(4);           // <- uncomment for longer range

  /* 2 kHz hardware ticker */
  ticker.attach_us(sampleISR, 500);
}

void loop()
{
  BLE.poll();             // keep the BLE stack serviced
  queue.dispatch(0);      // run pending filter tasks
}

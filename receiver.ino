/*****************************************************************
 *  NANO 33 BLE  ▸  RECEIVER  (Central → USB bridge @ 9600 baud)
 *  - Connects to the peripheral named "NanoSender"
 *  - Subscribes to its custom characteristic
 *  - Forwards every BLE notification verbatim to Serial
 *****************************************************************/

#include <Arduino.h>
#include <ArduinoBLE.h>

/* ---------- UUIDs must match the Sender --------------------- */
const char* PERIPHERAL_NAME = "NanoSender";
const char* SERVICE_UUID    = "17b10000-b000-400d-b000-000000000001";
const char* CHAR_UUID       = "17b10001-b000-400d-b000-000000000001";

/* ---------- Globals ---------------------------------------- */
BLEDevice        peripheral;
BLECharacteristic dataChar;

/* ---------- helper: scan → connect → subscribe ------------- */
void connectToSender()
{
  Serial.println(F("Scanning for NanoSender…"));
  BLE.scanForName(PERIPHERAL_NAME);

  while (true)
  {
    peripheral = BLE.available();
    if (!peripheral) continue;                          // still scanning

    if (peripheral.localName() != PERIPHERAL_NAME) {    // not our target
      continue;
    }

    Serial.print  (F("Found ")); Serial.print(PERIPHERAL_NAME);
    Serial.println(F(", connecting…"));
    BLE.stopScan();

    if (!peripheral.connect())
    {
      Serial.println(F("  Connection failed"));
      BLE.scanForName(PERIPHERAL_NAME);
      continue;
    }

    Serial.println(F("  Connected, discovering attributes…"));

    /* ----  discoverService() takes a UUID *string*  ---- */
    if (!peripheral.discoverService(SERVICE_UUID))
    {
      Serial.println(F("  Service not found"));
      peripheral.disconnect();
      BLE.scanForName(PERIPHERAL_NAME);
      continue;
    }

    /* Get the characteristic directly from the peripheral */
    dataChar = peripheral.characteristic(CHAR_UUID);
    if (!dataChar)
    {
      Serial.println(F("  Characteristic not found"));
      peripheral.disconnect();
      BLE.scanForName(PERIPHERAL_NAME);
      continue;
    }

    if (dataChar.canSubscribe()) dataChar.subscribe();
    Serial.println(F("  Subscribed – streaming data"));
    break;                                              // success!
  }
}

/* ----------------------------------------------------------- */
void setup()
{
  Serial.begin(9600);
  while (!Serial && millis() < 3000) ;   // wait (optional)

  if (!BLE.begin())
  {
    Serial.println(F("Failed to start BLE!"));
    while (1);
  }

  BLE.setEventHandler(BLEDisconnected, [](BLEDevice) {
    Serial.println(F("Disconnected – retrying…"));
  });

  connectToSender();
}

/* ----------------------------------------------------------- */
void loop()
{
  BLE.poll();                                   // run stack

  /* Re-connect if link is lost */
  if (!peripheral || !peripheral.connected())
  {
    connectToSender();
    return;
  }

  /* Forward each notification verbatim */
  if (dataChar.valueUpdated())
  {
    uint8_t buf[20];
    int len = dataChar.valueLength();
    dataChar.readValue(buf, len);
    Serial.write(buf, len);
  }
}

#include "NimBLEDevice.h"

/* ESP32 LED Settings */
#ifndef LED_BUILTIN
#define LED_BUILTIN 2                            // If your board doesn't have a defined LED_BUILTIN, replace 2 with the LED pin value
#endif
static const bool ledHighEqualsON = true;            // ESP32 board LED ON=HIGH (Default). If your ESP32 LED is turning OFF on scanning and turning ON while IDLE, then set this value to false
static const bool ledOnBootScan = true;              // Turn on LED during initial boot scan
static const bool ledOnScan = true;                  // Turn on LED while scanning (non-boot)
static const bool ledOnCommand = true;               // Turn on LED while MQTT command is processing. If scanning, LED will blink after scan completes. You may not notice it, there is no delay after scan
static int ledONValue = HIGH;
static int ledOFFValue = LOW;


// Set your Bot's mac address here
std::string botAddr = "E8:19:59:6E:D7:4D";
// Configure time between power and comunication trial.
static int pressDelay = 1;


static BLEUUID serviceUUID("cba20d00-224d-11e6-9fb8-0002a5d5c51b");
static BLEUUID    charUUID("cba20002-224d-11e6-9fb8-0002a5d5c51b");

static boolean doConnect = false;
static boolean connected = false;
static boolean alreadyPressed = false;
static boolean doScan = false;
NimBLEClient*  pClient;
static NimBLERemoteCharacteristic* pRemoteCharacteristic;
static NimBLEAdvertisedDevice* myDevice;
static NimBLEScan* pScan;
static const bool printSerialOutputForDebugging = true;     // Only set to true when you want to debug an issue from Arduino IDE. Lots of Serial output from scanning can crash the ESP32

static bool isActiveScan = true;
static const int initialScan = 120;                          // How many seconds to scan for bots on ESP reboot and autoRescan. Once all devices are found scan stops, so you can set this to a big number


bool connectToServer(NimBLEAdvertisedDevice * advDeviceToUse);

void printAString (const char * aString) {
  if (printSerialOutputForDebugging) {
    Serial.println(aString);
  }
}

void printAString (std::string & aString) {
  if (printSerialOutputForDebugging) {
    Serial.println(aString.c_str());
  }
}

void printAString (String & aString) {
  if (printSerialOutputForDebugging) {
    Serial.println(aString);
  }
}

void printAString (int aInt) {
  if (printSerialOutputForDebugging) {
    Serial.println(aInt);
  }
}
  
static void notifyCallback(
  NimBLERemoteCharacteristic* pNimBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pNimBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);
}

class MyClientCallback : public NimBLEClientCallbacks {

    void onConnect(NimBLEClient* pClient) {
      printAString("Connected");
      pClient->updateConnParams(120, 120, 0, 60);
    };

    void onDisconnect(NimBLEClient* pClient) {
    };

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
      if (params->itvl_min < 24) { /** 1.25ms units */
        return false;
      } else if (params->itvl_max > 40) { /** 1.25ms units */
        return false;
      } else if (params->latency > 2) { /** Number of intervals allowed to skip */
        return false;
      } else if (params->supervision_timeout > 100) { /** 10ms units */
        return false;
      }

      return true;
    };

    uint32_t onPassKeyRequest() {
      printAString("Client Passkey Request");
      return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key) {
      printAString("The passkey YES/NO number: ");
      printAString(pass_key);
      return true;
    };

    void onAuthenticationComplete(ble_gap_conn_desc* desc) {
      if (!desc->sec_state.encrypted) {

        printAString("Encrypt connection failed - disconnecting");

        NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
        return;
      }
    };
};

static MyClientCallback clientCB;


bool connectToServer(NimBLEAdvertisedDevice * advDeviceToUse) {
  printAString("Try to connect");
  NimBLEClient* pClient = nullptr;
  if (NimBLEDevice::getClientListSize()) {

    pClient = NimBLEDevice::getClientByPeerAddress(advDeviceToUse->getAddress());
    if (pClient) {
      if (!pClient->connect(advDeviceToUse, false)) {
        printAString("Connect failed");
      }
      else {
        printAString("Connected client");
      }
    }
    else {
      pClient = NimBLEDevice::getDisconnectedClient();
    }
  }
  if (!pClient) {
    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
      printAString("Max clients reached - no more connections available");
      return false;
    }
    printAString("Try again");
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&clientCB, false);
    pClient->setConnectionParams(12, 12, 0, 51);
    pClient->setConnectTimeout(10);
    yield();
  }
  if (!pClient->isConnected()) {
    if (!pClient->connect(advDeviceToUse)) {
      NimBLEDevice::deleteClient(pClient);
      printAString("Failed to connect, deleted client");
      return false;
    }
  }
  printAString("Connected to: ");
  printAString(pClient->getPeerAddress().toString().c_str());
  printAString("RSSI: ");
  printAString(pClient->getRssi());
  connected = true;
  return true;
}



class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
  // needed to satisfy override of abstract
  void checkToContinueScan() {
    printAString("checkToContinueScan");
  }
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice->toString().c_str());

    if (ledOnScan) {
      digitalWrite(LED_BUILTIN, ledONValue);
    }
      
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(serviceUUID)) {
      NimBLEDevice::getScan()->stop();
      //myDevice = new NimBLEAdvertisedDevice(advertisedDevice);
      myDevice = advertisedDevice;
      doConnect = true;
      doScan = true;
    }
  }
  // needed to satisfy override of abstract
  bool callForInfoAdvDev(std::string deviceMac, long anRSSI,  std::string & aValueString) {
    printAString("callForInfoAdvDev");
    return true;
  }
};

void scanEndedCB(NimBLEScanResults results) {
  printAString("START scanEndedCB");
  yield();
  isActiveScan = false;
  pScan->setActiveScan(isActiveScan);
  if (ledOnScan || ledOnCommand) {
    digitalWrite(LED_BUILTIN, ledOFFValue);
  }
  delay(50);
  printAString("Scan Ended");
}

void notifyCB(NimBLERemoteCharacteristic * pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  printAString("notifyCB");
  std::string deviceMac = pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress();
  printAString(deviceMac);
}

bool subscribeToNotify(NimBLEAdvertisedDevice* advDeviceToUse) {
  NimBLEClient* pClient = NimBLEDevice::getClientByPeerAddress(advDeviceToUse->getAddress());
  NimBLERemoteService* pSvc = nullptr;
  NimBLERemoteCharacteristic* pChr = nullptr;

  pSvc = pClient->getService("cba20d00-224d-11e6-9fb8-0002a5d5c51b"); // custom device service
  if (pSvc) {    /** make sure it's not null */
    pChr = pSvc->getCharacteristic("cba20003-224d-11e6-9fb8-0002a5d5c51b"); // custom characteristic to notify
  }
  if (pChr) {    /** make sure it's not null */
    if (pChr->canNotify()) {
      if (!pChr->subscribe(true, notifyCB)) {
        return false;
      }
    }
  }
  else {
    printAString("CUSTOM notify service not found.");
    return false;
  }
  printAString("subscribed to notify");
  return true;
}

bool sendCommandBytesWithResponse(NimBLERemoteCharacteristic * pChr, byte * bArray, int aSize ) {
  printAString("sendCommandBytesWithResponse");
  if (pChr == nullptr) {
    return false;
  }
  return pChr->writeValue(bArray, aSize, true);
}

bool sendBotCommandBytes(NimBLERemoteCharacteristic * pChr, byte * bArray, int aSize ) {
  printAString("sendBotCommandBytes");
  if (pChr == nullptr) {
    return false;
  }
  return sendCommandBytesWithResponse(pChr, bArray, aSize);
}

bool sendCommand(NimBLEAdvertisedDevice * advDeviceToUse, const char * type, int attempts, bool disconnectAfter) {
  printAString("sendBotCommandBytes");
  printAString(type);
  
  if (advDeviceToUse == nullptr) {
    return false;
  }
  printAString("Sending command...");

  byte bArrayPress[] = {0x57, 0x01};
  byte bArrayOn[] = {0x57, 0x01, 0x01};
  byte bArrayOff[] = {0x57, 0x01, 0x02};
  byte bArrayPlugOn[] = {0x57, 0x0F, 0x50, 0x01, 0x01, 0x80};
  byte bArrayPlugOff[] = {0x57, 0x0F, 0x50, 0x01, 0x01, 0x00};
  byte bArrayOpen[] =  {0x57, 0x0F, 0x45, 0x01, 0x05, 0xFF, 0x00};
  byte bArrayClose[] = {0x57, 0x0F, 0x45, 0x01, 0x05, 0xFF, 0x64};
  byte bArrayPause[] = {0x57, 0x0F, 0x45, 0x01, 0x00, 0xFF};
  byte bArrayPos[] =  {0x57, 0x0F, 0x45, 0x01, 0x05, 0xFF, NULL};
  byte bArrayGetSettings[] = {0x57, 0x02};
  byte bArrayHoldSecs[] = {0x57, 0x0F, 0x08, NULL };
  byte bArrayBotMode[] = {0x57, 0x03, 0x64, NULL, NULL};

  byte bArrayPressPass[] = {0x57, 0x11, NULL, NULL, NULL, NULL};
  byte bArrayOnPass[] = {0x57, 0x11, NULL , NULL, NULL, NULL, 0x01};
  byte bArrayOffPass[] = {0x57, 0x11, NULL, NULL, NULL, NULL, 0x02};
  byte bArrayGetSettingsPass[] = {0x57, 0x12, NULL, NULL, NULL, NULL};
  byte bArrayHoldSecsPass[] = {0x57, 0x1F, NULL, NULL, NULL, NULL, 0x08, NULL };
  byte bArrayBotModePass[] = {0x57, 0x13, NULL, NULL, NULL, NULL, 0x64, NULL};       // The proper array to use for setting mode with password (firmware 4.9)

  std::string anAddr = advDeviceToUse->getAddress();
  if (!NimBLEDevice::getClientListSize()) {
    return false;
  }
  NimBLEClient* pClient = NimBLEDevice::getClientByPeerAddress(anAddr);
  if (!pClient) {
    return false;
  }

  bool tryConnect = !(pClient->isConnected());
  int count = 1;
  while (tryConnect  || !pClient ) {
    if (count > 20) {
      printAString("Failed to connect for sending command");
      return false;
    }
    count++;
    printAString("Attempt to send command. Not connecting. Try connecting...");
    tryConnect = !(connectToServer(advDeviceToUse));
    if (!tryConnect) {
      pClient = NimBLEDevice::getClientByPeerAddress(anAddr);
    }
  }
  bool returnValue = true;
  returnValue = subscribeToNotify(advDeviceToUse);
  bool skipWaitAfter = false;
  if (returnValue) {
    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    pSvc = pClient->getService("cba20d00-224d-11e6-9fb8-0002a5d5c51b");
    if (pSvc) {
      pChr = pSvc->getCharacteristic("cba20002-224d-11e6-9fb8-0002a5d5c51b");
    }
    if (pChr) {
      if (pChr->canWrite()) {
        bool wasSuccess = false;
        if (strcmp(type, "PRESS") == 0) {
           printAString("Attempt to send PRESS");
          wasSuccess = sendBotCommandBytes(pChr, bArrayPress, 2);
        }
        else if (strcmp(type, "ON") == 0) {
          wasSuccess = sendBotCommandBytes(pChr, bArrayOn, 3);
        }
        else if (strcmp(type, "OFF") == 0) {
          wasSuccess = sendBotCommandBytes(pChr, bArrayOff, 3);
        }
        if (wasSuccess) {
          printAString("Wrote new value to: ");
          printAString(pChr->getUUID().toString().c_str());
        } else {
          returnValue = false;
        }
      } else {
        returnValue = false;
      }
      yield();
    } else {
      printAString("CUSTOM write service not found.");
      returnValue = false;
    }
  }
  if (!returnValue) {
    if (attempts >= 10) {
      printAString("Sending failed. Disconnecting client");
      pClient->disconnect();
    } return false;
  }
  if (disconnectAfter) {
    pClient->disconnect();
    yield();
  }
  printAString("Success! Command sent/received to/from SwitchBot");
  if (!skipWaitAfter) {
    //lastCommandSent[anAddr] = millis();
  }
  return true;
}

void setup() {
  pinMode (LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  NimBLEDevice::init("");

  NimBLEDevice::whiteListAdd(NimBLEAddress(botAddr, 1));

  NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  //NimBLEDevice::setScanFilterMode(2);
  pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setInterval(70);
  pScan->setWindow(40);
  pScan->setDuplicateFilter(false);
  pScan->setActiveScan(true);
  pScan->setMaxResults(100);
  isActiveScan = true;
  pScan->setActiveScan(isActiveScan);
  delay(50);
  if (ledOnScan) {
    digitalWrite(LED_BUILTIN, ledONValue);
  }
  pScan->start(initialScan, scanEndedCB, true);
}


void loop() {
  int fromStart = millis()/1000;
  if (!alreadyPressed && fromStart > pressDelay) {
    if (doConnect == true) {
      if (connectToServer(myDevice)) {
        Serial.println("Connected");
        doConnect = false;
      } else {
        Serial.println("Connection failed");
      }
    }
  
    yield();
    delay(100);    

    if (connected) {
      Serial.println("Sending PRESS command");
      sendCommand(myDevice, "PRESS", 1, true);
      yield();
      alreadyPressed = true;
      Serial.println("Success!");
    }
    
    yield();
    delay(500);    
  }
}

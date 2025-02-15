
/** NimBLE_Server Demo:
 *
 *  Demonstrates many of the available features of the NimBLE client library.
 *
 *  Created: on March 24 2020
 *      Author: H2zero
 *
*/

#include <NimBLEDevice.h>

static const NimBLEAdvertisedDevice* advDevice;

static bool doConnect = false;
static uint32_t scanTime = 0 * 1000; // In milliseconds, 0 = scan forever


/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        Serial.println("Connected");
        /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout.
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
         */
        pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient, int reason) {
        Serial.printf("%s Disconnected, reason = %d - Starting scan\n",
                      pClient->getPeerAddress().toString().c_str(), reason);
        NimBLEDevice::getScan()->start(scanTime);
    };

    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        if(params->itvl_min < 24) { /** 1.25ms units */
            return false;
        } else if(params->itvl_max > 40) { /** 1.25ms units */
            return false;
        } else if(params->latency > 2) { /** Number of intervals allowed to skip */
            return false;
        } else if(params->supervision_timeout > 100) { /** 10ms units */
            return false;
        }

        return true;
    };

    /********************* Security handled here **********************
    ****** Note: these are the same return values as defaults ********/
    void onPassKeyEntry(NimBLEConnInfo& connInfo){
        Serial.println("Server Passkey Entry");
        /** This should prompt the user to enter the passkey displayed
         * on the peer device.
         */
        NimBLEDevice::injectPassKey(connInfo, 123456);
    };

    void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pass_key){
        Serial.print("The passkey YES/NO number: ");Serial.println(pass_key);
        /** Inject false if passkeys don't match. */
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    };

    /** Pairing process complete, we can check the results in connInfo */
    void onAuthenticationComplete(NimBLEConnInfo& connInfo){
        if(!connInfo.isEncrypted()) {
            Serial.println("Encrypt connection failed - disconnecting");
            /** Find the client with the connection handle provided in desc */
            NimBLEDevice::getClientByHandle(connInfo.getConnHandle())->disconnect();
            return;
        }
    }
};


/** Define a class to handle the callbacks when advertisments are received */
class scanCallbacks: public NimBLEScanCallbacks {

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
        Serial.print("Advertised Device found: ");
        Serial.println(advertisedDevice->toString().c_str());
        if(advertisedDevice->isAdvertisingService(NimBLEUUID("DEAD")))
        {
            Serial.println("Found Our Service");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            advDevice = advertisedDevice;
            /** Ready to connect now */
            doConnect = true;
        }
    }

    /** Callback to process the results of the completed scan or restart it */
    void onScanEnd(const NimBLEScanResults& results, int reason) {
        Serial.print("Scan Ended; reason = "); Serial.println(reason);
    }
};


/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
    str += std::string(pRemoteCharacteristic->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    str += ", Value = " + std::string((char*)pData, length);
    Serial.println(str.c_str());
}


/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks clientCB;


/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer() {
    NimBLEClient* pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if(NimBLEDevice::getCreatedClientCount()) {
        /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if(pClient){
            if(!pClient->connect(advDevice, false)) {
                Serial.println("Reconnect failed");
                return false;
            }
            Serial.println("Reconnected client");
        }
        /** We don't already have a client that knows this device,
         *  we will check for a client that is disconnected that we can use.
         */
        else {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if(!pClient) {
        if(NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.println("Max clients reached - no more connections available");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        Serial.println("New client created");

        pClient->setClientCallbacks(&clientCB, false);
        /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
         */
        pClient->setConnectionParams(12,12,0,51);
        /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
        pClient->setConnectTimeout(5 * 1000);


        if (!pClient->connect(advDevice)) {
            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            Serial.println("Failed to connect, deleted client");
            return false;
        }
    }

    if(!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            Serial.println("Failed to connect");
            return false;
        }
    }

    Serial.print("Connected to: ");
    Serial.println(pClient->getPeerAddress().toString().c_str());
    Serial.print("RSSI: ");
    Serial.println(pClient->getRssi());

    /** Now we can read/write/subscribe the charateristics of the services we are interested in */
    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor* pDsc = nullptr;

    pSvc = pClient->getService("DEAD");
    if(pSvc) {     /** make sure it's not null */
        pChr = pSvc->getCharacteristic("BEEF");

        if(pChr) {     /** make sure it's not null */
            if(pChr->canRead()) {
                Serial.print(pChr->getUUID().toString().c_str());
                Serial.print(" Value: ");
                Serial.println(pChr->readValue().c_str());
            }

            if(pChr->canWrite()) {
                if(pChr->writeValue("Tasty")) {
                    Serial.print("Wrote new value to: ");
                    Serial.println(pChr->getUUID().toString().c_str());
                }
                else {
                    /** Disconnect if write failed */
                    pClient->disconnect();
                    return false;
                }

                if(pChr->canRead()) {
                    Serial.print("The value of: ");
                    Serial.print(pChr->getUUID().toString().c_str());
                    Serial.print(" is now: ");
                    Serial.println(pChr->readValue().c_str());
                }
            }

            /** registerForNotify() has been removed and replaced with subscribe() / unsubscribe().
             *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr.
             *  Unsubscribe takes no parameters.
             */
            if(pChr->canNotify()) {
                //if(!pChr->registerForNotify(notifyCB)) {
                if(!pChr->subscribe(true, notifyCB)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
            else if(pChr->canIndicate()) {
                /** Send false as first argument to subscribe to indications instead of notifications */
                //if(!pChr->registerForNotify(notifyCB, false)) {
                if(!pChr->subscribe(false, notifyCB)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
        }

    } else {
        Serial.println("DEAD service not found.");
    }

    pSvc = pClient->getService("BAAD");
    if(pSvc) {     /** make sure it's not null */
        pChr = pSvc->getCharacteristic("F00D");

        if(pChr) {     /** make sure it's not null */
            if(pChr->canRead()) {
                Serial.print(pChr->getUUID().toString().c_str());
                Serial.print(" Value: ");
                Serial.println(pChr->readValue().c_str());
            }

            pDsc = pChr->getDescriptor(NimBLEUUID("C01D"));
            if(pDsc) {   /** make sure it's not null */
                Serial.print("Descriptor: ");
                Serial.print(pDsc->getUUID().toString().c_str());
                Serial.print(" Value: ");
                Serial.println(pDsc->readValue().c_str());
            }

            if(pChr->canWrite()) {
                if(pChr->writeValue("No tip!")) {
                    Serial.print("Wrote new value to: ");
                    Serial.println(pChr->getUUID().toString().c_str());
                }
                else {
                    /** Disconnect if write failed */
                    pClient->disconnect();
                    return false;
                }

                if(pChr->canRead()) {
                    Serial.print("The value of: ");
                    Serial.print(pChr->getUUID().toString().c_str());
                    Serial.print(" is now: ");
                    Serial.println(pChr->readValue().c_str());
                }
            }

            /** registerForNotify() has been removed and replaced with subscribe() / unsubscribe().
             *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=true.
             *  Unsubscribe parameter defaults are: response=true.
             */
            if(pChr->canNotify()) {
                //if(!pChr->registerForNotify(notifyCB)) {
                if(!pChr->subscribe(true, notifyCB)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
            else if(pChr->canIndicate()) {
                /** Send false as first argument to subscribe to indications instead of notifications */
                //if(!pChr->registerForNotify(notifyCB, false)) {
                if(!pChr->subscribe(false, notifyCB)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
        }

    } else {
        Serial.println("BAAD service not found.");
    }

    Serial.println("Done with this device!");
    return true;
}

void setup (){
    Serial.begin(115200);
    Serial.println("Starting NimBLE Client");
    /** Initialize NimBLE, no device name spcified as we are not advertising */
    NimBLEDevice::init("");

    /** Set the IO capabilities of the device, each option will trigger a different pairing method.
     *  BLE_HS_IO_KEYBOARD_ONLY    - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

    /** 2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, secure connections.
     *
     *  These are the default values, only shown here for demonstration.
     */
    //NimBLEDevice::setSecurityAuth(false, false, true);
    NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

    /** Optional: set the transmit power, default is 3db */
#ifdef ESP_PLATFORM
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
#else
    NimBLEDevice::setPower(9); /** +9db */
#endif

    /** create new scan */
    NimBLEScan* pScan = NimBLEDevice::getScan();

    /** create a callback that gets called when advertisers are found */
    pScan->setScanCallbacks(new scanCallbacks());

    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(45);
    pScan->setWindow(15);

    /** Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(true);
    /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops.
     */
    pScan->start(scanTime);
}


void loop (){
    /** Loop here until we find a device we want to connect to */
    while(!doConnect){
        delay(1);
    }

    doConnect = false;

    /** Found a device we want to connect to, do it now */
    if(connectToServer()) {
        Serial.println("Success! we should now be getting notifications, scanning for more!");
    } else {
        Serial.println("Failed to connect, starting scan");
    }

    NimBLEDevice::getScan()->start(scanTime);
}

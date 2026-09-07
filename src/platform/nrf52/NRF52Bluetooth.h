#pragma once

#include "BluetoothCommon.h"
#include <Arduino.h>

class NRF52Bluetooth : BluetoothApi
{
  public:
    void setup();
    void shutdown();
    void startDisabled();
    void resumeAdvertising();
    void clearBonds();
    bool isConnected();
    bool isInitialized() const { return initialized; }
    int getRssi();
    void sendLog(const uint8_t *logMessage, size_t length);

  private:
    bool setupAttempted = false;
    bool initialized = false;
    bool enabled = false;
    static void restoreSecurityState();
    static void restoreTxPower();
    static void onConnectionSecured(uint16_t conn_handle);
    static bool onPairingPasskey(uint16_t conn_handle, uint8_t const passkey[6], bool match_request);
    static void onPairingCompleted(uint16_t conn_handle, uint8_t auth_status);

    static bool onUnwantedPairing(uint16_t conn_handle, uint8_t const passkey[6], bool match_request);
    static void disconnect();
};

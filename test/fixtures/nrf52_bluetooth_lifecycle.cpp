#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define LOG_INFO(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#define LOG_WARN(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
#define LOG_POWERFSM(...) ((void)0)
#define IF_SCREEN(x) x
#define RECORD_CRITICALERROR(x) (++criticalErrors)
#define VALID_BLE_TX_POWER(x) ((x) == 4)
#define BANDWIDTH_MAX 1
#define BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE 1
#define SECMODE_ENC_WITH_MITM 1
#define SECMODE_OPEN 0
#define HW_VERSION "test"
#define APP_VERSION "test"
#define optstr(x) x
constexpr int meshtastic_Config_BluetoothConfig_PairingMode_NO_PIN = 0;
constexpr int meshtastic_Config_BluetoothConfig_PairingMode_FIXED_PIN = 1;
constexpr int meshtastic_PowerMon_State_BT_On = 1;
struct BluetoothApi {
};
struct {
    struct {
        bool enabled = false;
        int mode = 0;
        unsigned fixed_pin = 123456;
    } bluetooth;
} config;
bool useSoftDevice = true;
unsigned criticalErrors = 0, meshRegistrations = 0;
uint32_t rebootAtMsec = 0, shutdownAtMsec = 0, now = 0;
uint32_t millis()
{
    return now;
}
struct Throttle {
    static bool isWithinTimespanMs(uint32_t start, uint32_t duration) { return now - start < duration; }
};
using PairCallback = bool (*)(uint16_t, const uint8_t *, bool);
struct SecurityFake {
    PairCallback pairing = nullptr;
    void (*complete)(uint16_t, uint8_t) = nullptr;
    void (*secured)(uint16_t) = nullptr;
    bool mitm = false, display = false;
    void setPairPasskeyCallback(PairCallback callback)
    {
        pairing = callback;
        mitm = true;
    }
    void setPairCompleteCallback(decltype(complete) callback) { complete = callback; }
    void setSecuredCallback(decltype(secured) callback) { secured = callback; }
    void setMITM(bool value) { mitm = value; }
    void setIOCaps(bool value, bool, bool) { display = value; }
    void setPIN(const char *) {}
};
struct ServiceFake {
    unsigned registrations = 0;
    void begin() { ++registrations; }
    void setPermission(int, int) {}
    void setModel(const char *) {}
    void setFirmwareRev(const char *) {}
    void write(int) {}
};
ServiceFake meshBleService, bledfu, bledfusecure, bledis, blebas;
struct AdvertisingFake {
    bool restart = false, running = false, succeed = true;
    unsigned starts = 0, stops = 0, payloads = 0;
    void restartOnDisconnect(bool value) { restart = value; }
    void stop()
    {
        running = false;
        ++stops;
    }
    void clearData() {}
    void addFlags(int) {}
    void addService(ServiceFake &) { ++payloads; }
    void addTxPower() {}
    void addName() {}
    void setInterval(int, int) {}
    void setFastTimeout(int) {}
    bool isRunning() { return running; }
    bool start(int)
    {
        ++starts;
        running = succeed;
        return running;
    }
};
struct PeriphFake {
    void setConnectCallback(void (*)(uint16_t)) {}
    void setDisconnectCallback(void (*)(uint16_t, uint8_t)) {}
    void setConnSlaveLatency(int) {}
    void setConnInterval(int, int) {}
};
struct BluefruitFake {
    SecurityFake Security;
    AdvertisingFake Advertising, ScanResponse;
    PeriphFake Periph;
    bool beginOk = true, connectedNow = false, pendingDisconnect = false, delayDisconnect = false;
    unsigned begins = 0, disconnects = 0;
    int txPower = 0;
    void autoConnLed(bool) {}
    void configPrphBandwidth(int) {}
    bool begin()
    {
        ++begins;
        return beginOk;
    }
    bool setTxPower(int value)
    {
        txPower = value;
        return true;
    }
    void setName(const char *) {}
    unsigned connected() { return connectedNow; }
    void disconnect(unsigned)
    {
        assert(!Advertising.restart && !Advertising.running);
        ++disconnects;
        pendingDisconnect = true;
    }
    void deliverDisconnect()
    {
        if (pendingDisconnect) {
            pendingDisconnect = connectedNow = false;
            if (Advertising.restart && !Advertising.running)
                Advertising.start(0);
        }
    }
} Bluefruit;
void delay(unsigned duration)
{
    now += duration;
    if (!Bluefruit.delayDisconnect)
        Bluefruit.deliverDisconnect();
}
struct PowerMonFake {
    bool on = false;
    void setState(int) { on = true; }
    void clearState(int) { on = false; }
} powerMonitor;
PowerMonFake *powerMon = &powerMonitor;
struct HardwareRNG {
    static void fill(uint8_t *, unsigned) {}
};
const char *getDeviceName()
{
    return "Superbase";
}
void onConnect(uint16_t) {}
void onDisconnect(uint16_t, uint8_t) {}
void setupMeshService()
{
    ++meshRegistrations;
}
uint32_t configuredPasskey = 0;
int lastBatteryLevel = -1;

// PRODUCTION_CLASS
NRF52Bluetooth *nrf52Bluetooth = nullptr;
bool NRF52Bluetooth::onPairingPasskey(uint16_t, const uint8_t *, bool)
{
    return true;
}
bool NRF52Bluetooth::onUnwantedPairing(uint16_t, const uint8_t *, bool)
{
    return false;
}
void NRF52Bluetooth::onPairingCompleted(uint16_t, uint8_t) {}
void NRF52Bluetooth::onConnectionSecured(uint16_t) {}

// PRODUCTION_BODIES
struct NodeDBFake {
    bool saved = false;
    unsigned saves = 0;
    void saveToDisk()
    {
        saved = config.bluetooth.enabled;
        ++saves;
    }
} db;
NodeDBFake *nodeDB = &db;
struct ScreenFake {
    std::string banner;
    void showSimpleBanner(const char *text, int) { banner = text; }
} display;
ScreenFake *screen = &display;
// PRODUCTION_TOGGLE

void reset(int mode, bool bootEnabled)
{
    delete nrf52Bluetooth;
    nrf52Bluetooth = nullptr;
    Bluefruit = {};
    powerMonitor = {};
    bledfu = bledis = blebas = {};
    meshRegistrations = criticalErrors = now = 0;
    rebootAtMsec = shutdownAtMsec = 0;
    config.bluetooth.mode = mode;
    config.bluetooth.enabled = bootEnabled;
}
void checkOn()
{
    assert(config.bluetooth.enabled && powerMonitor.on);
    assert(Bluefruit.Advertising.running && Bluefruit.Advertising.restart);
#ifdef NRF52_BLE_TX_POWER
    assert(Bluefruit.txPower == NRF52_BLE_TX_POWER);
#else
    assert(Bluefruit.txPower == 0);
#endif
    assert(Bluefruit.Security.mitm == (config.bluetooth.mode != 0));
    assert(Bluefruit.Security.display == (config.bluetooth.mode != 0));
    assert((Bluefruit.Security.pairing != nullptr) == (config.bluetooth.mode != 0));
    assert((Bluefruit.Security.complete != nullptr) == (config.bluetooth.mode != 0));
    assert((Bluefruit.Security.secured != nullptr) == (config.bluetooth.mode != 0));
}
void checkOff()
{
    assert(!powerMonitor.on && !Bluefruit.Advertising.running && !Bluefruit.Advertising.restart);
    assert(Bluefruit.txPower == -40);
}
int main()
{
    for (int mode : {0, 1, 2})
        for (bool boot : {false, true}) {
            reset(mode, boot);
            setBluetoothEnableUnlessRestarting();
            assert(Bluefruit.begins == 1 && meshRegistrations == 1);
            auto *original = nrf52Bluetooth;
            if (!boot) {
                checkOff();
                assert(Bluefruit.Advertising.starts == 0);
                toggle();
            }
            checkOn();
            auto callback = Bluefruit.Security.pairing;
            rebootAtMsec = 1234;
            for (int cycle = 0; cycle < 5; ++cycle) {
                Bluefruit.connectedNow = true;
                Bluefruit.Advertising.running = false;
                setBluetoothEnable(true);
                assert(Bluefruit.connectedNow && !Bluefruit.Advertising.running);
                toggle();
                assert(!config.bluetooth.enabled && !db.saved && display.banner == "Bluetooth OFF");
                assert(!Bluefruit.connectedNow);
                checkOff();
                now += 30000;
                setBluetoothEnable(true);
                checkOff();
                assert(rebootAtMsec == 1234);
                toggle();
                checkOn();
                assert(db.saved && display.banner == "Bluetooth ON");
                assert(Bluefruit.Security.pairing == callback && rebootAtMsec == 1234);
                assert(nrf52Bluetooth == original && Bluefruit.begins == 1 && meshRegistrations == 1);
            }
            assert(bledis.registrations == 1 && blebas.registrations == 1 && bledfu.registrations == 1);
            assert(Bluefruit.Advertising.payloads == 1);
            nrf52Bluetooth->startDisabled();
            checkOff();
            assert(Bluefruit.begins == 1);
            setBluetoothEnableUnlessRestarting();
            checkOff();
            rebootAtMsec = 0;
            setBluetoothEnableUnlessRestarting();
            checkOn();
            Bluefruit.connectedNow = true;
            Bluefruit.Advertising.running = false;
            Bluefruit.delayDisconnect = true;
            toggle();
            checkOff();
            Bluefruit.deliverDisconnect();
            checkOff();
            setBluetoothEnableUnlessRestarting();
            checkOff();
            toggle();
            checkOn();
            Bluefruit.Advertising.running = false;
            setBluetoothEnable(true);
            checkOn();
            toggle();
            Bluefruit.Advertising.succeed = false;
            toggle();
            checkOff();
        }
    reset(0, false);
    Bluefruit.beginOk = false;
    for (int i = 0; i < 5; ++i) {
        setBluetoothEnable(true);
        nrf52Bluetooth->setup();
    }
    assert(Bluefruit.begins == 1 && meshRegistrations == 0 && criticalErrors == 1 && !powerMonitor.on);
    delete nrf52Bluetooth;
    nrf52Bluetooth = nullptr;
    std::puts("PASS: BLE lifecycle, 3 pairing modes, 2 boot states, 5 connected cycles, deferred disconnect, failure paths");
}

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

static std::vector<int> calls;
struct SpiFake {
    void lock() { calls.push_back(1); }
    void unlock() { calls.push_back(6); }
} spi;
auto *spiLock = &spi;
struct FsFake {
    void _lockFS() { calls.push_back(2); }
    void _unlockFS() { calls.push_back(4); }
} InternalFS;
void flash_nrf5x_flush()
{
    calls.push_back(3);
}
using err_t = int;
struct ble_gatts_evt_write_t {
    uint16_t len;
    uint8_t data[1];
};
class BLECharacteristic
{
  public:
    using write_authorize_cb_t = void (*)(uint16_t, BLECharacteristic *, ble_gatts_evt_write_t *);
    void setWriteAuthorizeCallback(write_authorize_cb_t cb) { _wr_authorize_cb = cb; }
    void write(ble_gatts_evt_write_t &request) { _wr_authorize_cb(0, this, &request); }

  protected:
    write_authorize_cb_t _wr_authorize_cb = nullptr;
};
class BLEDfu
{
  public:
    virtual err_t begin()
    {
        _chr_control.setWriteAuthorizeCallback(
            [](uint16_t, BLECharacteristic *, ble_gatts_evt_write_t *) { calls.push_back(5); });
        return 0;
    }
    void write(ble_gatts_evt_write_t &request) { _chr_control.write(request); }

  protected:
    BLECharacteristic _chr_control;
};

// PRODUCTION_QUIESCE
// PRODUCTION_DFU

int main()
{
    nrf52FlashQuiesce();
    assert((calls == std::vector<int>{1, 2, 3}));
    QuiescingBLEDfu service;
    assert(service.begin() == 0);
    for (uint16_t length : {0, 1}) {
        for (uint8_t command : {0, 1, 2}) {
            calls.clear();
            ble_gatts_evt_write_t request{length, {command}};
            service.write(request);
            const std::vector<int> expected = command == 1 ? std::vector<int>{1, 2, 3, 4, 5, 6} : std::vector<int>{5};
            assert(calls == expected);
        }
    }
    puts("PASS: flash flush lock order and DFU START/non-START callbacks, including zero-length library behavior");
}

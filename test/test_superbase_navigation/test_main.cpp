#include "MeshTypes.h"
#include "TestUtil.h"
#include "UptimeClock.h"
#include "input/TrackballInterruptBase.h"
#include "graphics/Screen.h"
#include "graphics/draw/NotificationRenderer.h"
#include "platform/portduino/PortduinoGlue.h"
#include <PortduinoGPIO.h>
#include <memory>
#include <vector>
#include <unity.h>

// No real GPIO, I2C, OLED, BLE or radio. The actual input driver, InputBroker,
// Screen input handler, menu renderer and OLEDDisplayUi execute unchanged.
extern std::vector<std::unique_ptr<GPIOPinIf>> pins;
extern bool osk_found;
namespace graphics {
class MemoryDisplay : public OLEDDisplay {
  public:
    MemoryDisplay() { setGeometry(GEOMETRY_128_128); }
    unsigned writes = 0;
    void display() override { ++writes; }
  protected:
    int getBufferOffset() override { return 0; }
    bool connect() override { return true; }
    void sendInitCommands() override {}
};
struct ScreenNavigationTest {
    static void blank(OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t) {}
    static void init(Screen &s, InputBroker &broker) {
        delete s.ui;
        delete s.dispdev;
        auto *display = new MemoryDisplay();
        TEST_ASSERT_TRUE(display->init());
        s.dispdev = display;
        s.ui = new OLEDDisplayUi(display);
        s.ui->disableAutoTransition();
        s.ui->disableAllIndicators();
        s.ui->setTimePerTransition(0);
        s.ui->setTargetFPS(30);
        static FrameCallback frames[] = {blank, blank, blank};
        s.ui->setFrames(frames, 3);
        s.framesetInfo = {};
        s.framesetInfo.frameCount = 3;
        s.framesetInfo.positions.home = 0;
        s.framesetInfo.positions.system = 1;
        s.framesetInfo.positions.lora = 2;
        s.screenOn = s.showingNormalScreen = s.useDisplay = true;
        s.displayWidth = s.displayHeight = 128;
        s.inputObserver.observe(&broker);
    }
    static void tick(Screen &s) { ready(s); s.ui->update(); }
    static void ready(Screen &s) { s.ui->getUiState()->lastUpdate = millis() - 1000; }
    static unsigned frame(Screen &s) { return s.ui->getUiState()->currentFrame; }
    static void focus(Screen &s, unsigned n) { s.ui->switchToFrame(n); }
    static void power(Screen &s, bool on) { s.screenOn = on; }
};
}
namespace {
constexpr uint8_t gpio[] = {10, 21, 17, 37, 16};
constexpr input_broker_event eventFor[] = {INPUT_BROKER_SELECT, INPUT_BROKER_UP, INPUT_BROKER_DOWN,
                                         INPUT_BROKER_LEFT, INPUT_BROKER_RIGHT};
class LevelPin : public GPIOPin {
  public:
    LevelPin(unsigned n) : GPIOPin(n, "navigation-test") { setSilent(); }
    PinStatus level = HIGH;
    PinStatus readPin() override { return level; }
    void attachInterrupt(voidFuncPtr, PinStatus) override {} // IRQs are injected explicitly, or deliberately absent.
};
struct Capture : Observer<const InputEvent *> {
    std::vector<input_broker_event> events;
    int onNotify(const InputEvent *e) override { events.push_back(e->inputEvent); return 0; }
};
TrackballInterruptBase *tb;
Capture capture;
std::unique_ptr<GPIOPinIf> savedPins[5];
LevelPin *levels[5];
NodeDB *testDB;
bool savedReal, savedOsk;
bool realClock;
void irq(unsigned key) {
    switch (key) {
    case 0: tb->intPressHandler(); break;
    case 1: tb->intUpHandler(); break;
    case 2: tb->intDownHandler(); break;
    case 3: tb->intLeftHandler(); break;
    case 4: tb->intRightHandler(); break;
    }
}
void poll(unsigned advance = 0) {
    if (realClock) testDelay(advance);
    else Time::advanceTestMillis(advance);
    graphics::ScreenNavigationTest::ready(*screen);
    tb->runOnce();
}
void press(unsigned key, bool withIrq) {
    levels[key]->level = LOW;
    if (withIrq) irq(key);
    poll();
    poll(20);
}
void release(unsigned key) {
    levels[key]->level = HIGH;
    poll(20);
    poll(20);
}
void attachScreen() {
    realClock = true;
    Time::useRealClock();
    graphics::ScreenNavigationTest::init(*screen, *inputBroker);
}
void noop() {}
void expectTap(unsigned key) {
    irq(key); // The falling edge occurred; the switch is already released before the first poll.
    poll(30);
    TEST_ASSERT_EQUAL_UINT(1, capture.events.size());
    TEST_ASSERT_EQUAL(eventFor[key], capture.events[0]);
    poll(50);
    TEST_ASSERT_EQUAL_UINT(1, capture.events.size());
}
void expectPolling(unsigned key) {
    press(key, false);
    release(key);
    TEST_ASSERT_EQUAL_UINT(1, capture.events.size());
    TEST_ASSERT_EQUAL(eventFor[key], capture.events[0]);
}
}
void setUp() {
    if (!testDB) testDB = new NodeDB();
    nodeDB = testDB;
    config = meshtastic_LocalConfig_init_zero;
    moduleConfig = meshtastic_LocalModuleConfig_init_zero;
    config.lora.region = meshtastic_Config_LoRaConfig_RegionCode_US;
    savedReal = realHardware;
    savedOsk = osk_found;
    for (unsigned i=0;i<5;++i) {
        savedPins[i] = std::move(pins[gpio[i]]);
        levels[i] = new LevelPin(gpio[i]);
        pins[gpio[i]].reset(levels[i]);
    }
    realClock = false;
    Time::setTestMillis(100000);
    // Broker's power policy reads isScreenOn; the UI fixture will be attached only in UI tests.
    screen = std::make_unique<graphics::Screen>(ScanI2C::ADDRESS_NONE, meshtastic_Config_DisplayConfig_OledType_OLED_AUTO, GEOMETRY_128_128);
    graphics::ScreenNavigationTest::power(*screen, true);
    inputBroker = new InputBroker();
    capture.events.clear();
    capture.observe(inputBroker);
    tb = new TrackballInterruptBase("trackball1");
    tb->init(gpio[2],gpio[1],gpio[3],gpio[4],gpio[0],eventFor[2],eventFor[1],eventFor[3],eventFor[4],
             eventFor[0],INPUT_BROKER_SELECT_LONG,noop,noop,noop,noop,noop);
    inputBroker->registerSource(tb);
    graphics::NotificationRenderer::resetBanner();
    testDelay(12); // Legacy direction IRQ debounce uses real millis().
}
void tearDown() {
    graphics::NotificationRenderer::resetBanner();
    screen.reset();
    capture.unobserve(inputBroker);
    delete inputBroker; inputBroker = nullptr; // unregister source before destroying it
    delete tb; tb = nullptr;
    for (unsigned i=0;i<5;++i) pins[gpio[i]] = std::move(savedPins[i]);
    realHardware = savedReal;
    osk_found = savedOsk;
    Time::useRealClock();
}
void test_released_up_tap() { expectTap(1); }
void test_released_down_tap() { expectTap(2); }
void test_released_left_tap() { expectTap(3); }
void test_released_right_tap() { expectTap(4); }
void test_released_select_tap() { expectTap(0); }
void test_up_without_interrupt() { expectPolling(1); }
void test_down_without_interrupt() { expectPolling(2); }
void test_left_without_interrupt() { expectPolling(3); }
void test_right_without_interrupt() { expectPolling(4); }
void test_select_without_interrupt() { expectPolling(0); }
void test_distinct_buttons_do_not_share_debounce() {
    irq(3); irq(4); poll(30);
    TEST_ASSERT_EQUAL_UINT(2,capture.events.size());
    TEST_ASSERT_EQUAL(INPUT_BROKER_LEFT,capture.events[0]);
    TEST_ASSERT_EQUAL(INPUT_BROKER_RIGHT,capture.events[1]);
}
void test_bounce_does_not_duplicate_direction() {
    levels[4]->level=LOW; irq(4); poll();
    irq(4); poll(1); irq(4); poll(1);
    TEST_ASSERT_EQUAL_UINT(1,capture.events.size());
    levels[4]->level=HIGH; poll(2); levels[4]->level=LOW; poll(2);
    TEST_ASSERT_EQUAL_UINT(1,capture.events.size());
    release(4);
}
void test_repeat_and_release_at_clock_zero() {
    Time::setTestMillis(0);
    press(4,true);
    poll(480);
    TEST_ASSERT_EQUAL_UINT(2,capture.events.size());
    poll(149); TEST_ASSERT_EQUAL_UINT(2,capture.events.size());
    poll(1); TEST_ASSERT_EQUAL_UINT(3,capture.events.size());
    release(4); poll(1000); TEST_ASSERT_EQUAL_UINT(3,capture.events.size());
}
void test_repeat_survives_clock_wrap() {
    Time::setTestMillis(UINT32_MAX-200);
    press(4,true); poll(480);
    TEST_ASSERT_EQUAL_UINT(2,capture.events.size());
    release(4); poll(1000); TEST_ASSERT_EQUAL_UINT(2,capture.events.size());
}
void test_long_select_does_not_become_short_on_release() {
    press(0,false); poll(500);
    TEST_ASSERT_EQUAL_UINT(1,capture.events.size());
    TEST_ASSERT_EQUAL(INPUT_BROKER_SELECT_LONG,capture.events[0]);
    release(0); TEST_ASSERT_EQUAL_UINT(1,capture.events.size());
}
void test_irq_and_poll_do_not_duplicate() {
    press(4,true); poll(100); release(4);
    TEST_ASSERT_EQUAL_UINT(1,capture.events.size());
}
void test_ui_right_without_irq_changes_frame() {
    attachScreen(); press(4,false); graphics::ScreenNavigationTest::tick(*screen);
    TEST_ASSERT_EQUAL_UINT(1,graphics::ScreenNavigationTest::frame(*screen));
}
void test_ui_left_released_before_poll_changes_frame() {
    attachScreen(); irq(3); poll(30); graphics::ScreenNavigationTest::tick(*screen);
    TEST_ASSERT_EQUAL_UINT(2,graphics::ScreenNavigationTest::frame(*screen));
}
void test_ui_select_without_irq_opens_home_menu() {
    attachScreen(); press(0,false); release(0);
    TEST_ASSERT_TRUE(graphics::NotificationRenderer::isMenuShowing());
}
void test_ui_menu_down_up_and_cancel() {
    attachScreen();
    static const char *labels[]={"First","Second","Third"};
    static const int values[]={0,1,2};
    graphics::BannerOverlayOptions o; o.message="Test menu"; o.optionsArrayPtr=labels; o.optionsEnumPtr=values; o.optionsCount=3;
    screen->showOverlayBanner(o);
    press(2,false); release(2); TEST_ASSERT_EQUAL_INT(1,graphics::NotificationRenderer::curSelected);
    press(1,false); release(1); TEST_ASSERT_EQUAL_INT(0,graphics::NotificationRenderer::curSelected);
    InputEvent cancel={}; cancel.inputEvent=INPUT_BROKER_CANCEL;
    graphics::ScreenNavigationTest::ready(*screen);
    inputBroker->injectInputEvent(&cancel);
    TEST_ASSERT_FALSE(graphics::NotificationRenderer::isMenuShowing());
}
void test_wake_event_does_not_navigate_blindly() {
    attachScreen(); graphics::ScreenNavigationTest::power(*screen,false);
    press(4,false); release(4);
    TEST_ASSERT_EQUAL_UINT(0,capture.events.size());
    // Model the completion of the already requested power transition (not physical power hardware).
    graphics::ScreenNavigationTest::power(*screen,true);
    press(4,false); graphics::ScreenNavigationTest::tick(*screen);
    TEST_ASSERT_EQUAL_UINT(1,graphics::ScreenNavigationTest::frame(*screen));
}
void setup() {
    initializeTestEnvironment();
    UNITY_BEGIN();
    RUN_TEST(test_released_up_tap); RUN_TEST(test_released_down_tap);
    RUN_TEST(test_released_left_tap); RUN_TEST(test_released_right_tap); RUN_TEST(test_released_select_tap);
    RUN_TEST(test_up_without_interrupt); RUN_TEST(test_down_without_interrupt);
    RUN_TEST(test_left_without_interrupt); RUN_TEST(test_right_without_interrupt); RUN_TEST(test_select_without_interrupt);
    RUN_TEST(test_distinct_buttons_do_not_share_debounce); RUN_TEST(test_bounce_does_not_duplicate_direction);
    RUN_TEST(test_repeat_and_release_at_clock_zero); RUN_TEST(test_repeat_survives_clock_wrap);
    RUN_TEST(test_long_select_does_not_become_short_on_release); RUN_TEST(test_irq_and_poll_do_not_duplicate);
    RUN_TEST(test_ui_right_without_irq_changes_frame); RUN_TEST(test_ui_left_released_before_poll_changes_frame);
    RUN_TEST(test_ui_select_without_irq_opens_home_menu); RUN_TEST(test_ui_menu_down_up_and_cancel);
    RUN_TEST(test_wake_event_does_not_navigate_blindly);
    exit(UNITY_END());
}
void loop() {}

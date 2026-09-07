#include "TestUtil.h"
#include "UptimeClock.h"
#include "airtime.h"
#include "main.h"
#include "mesh/Channels.h"
#include "mesh/CryptoEngine.h"
#include "mesh/MeshService.h"
#include "mesh/NodeDB.h"
#include "mesh/RadioLibInterface.h"
#include "mesh/Router.h"
#include "mesh/TransmitHistory.h"
#include "modules/NodeInfoModule.h"
#include "modules/PositionModule.h"
#include "modules/RoutingModule.h"
#include "modules/Telemetry/DeviceTelemetry.h"
#include "support/MockMeshService.h"
#include <memory>
#include <unity.h>
#include <vector>

class BroadcastTestAccess
{
  public:
    static void tick(PositionModule &m) { m.runOnce(); }
    static void tick(NodeInfoModule &m) { m.runOnce(); }
    static uint32_t sent(PositionModule &m) { return m.lastGpsSend; }
    static uint32_t generation(PositionModule &m) { return m.currentGeneration; }
    static uint32_t generation(NodeInfoModule &m) { return m.currentGeneration; }
    static bool urgent(NodeInfoModule &m) { return m.shorterTimeout; }
    static void smart(PositionModule &m, const meshtastic_PositionLite &p, uint32_t now) { m.trySmartBroadcast(p, now); }
    static meshtastic_Telemetry stats(DeviceTelemetryModule &m) { return m.getLocalStatsTelemetry(); }
};

class CaptureRouter : public Router
{
  public:
    ErrorCode result = ERRNO_OK;
    std::vector<meshtastic_PortNum> attempts;
    std::vector<meshtastic_MeshPacket *> packets;
    ~CaptureRouter()
    {
        for (auto *p : packets)
            packetPool.release(p);
    }
    ErrorCode send(meshtastic_MeshPacket *p) override
    {
        attempts.push_back(p->decoded.portnum);
        if (result == ERRNO_OK)
            packets.push_back(p);
        else
            packetPool.release(p);
        return result;
    }
};

class IdleRadio : public RadioInterface
{
  public:
    ErrorCode send(meshtastic_MeshPacket *p) override
    {
        packetPool.release(p);
        return ERRNO_OK;
    }
    uint32_t getPacketTime(uint32_t, bool = false) override { return 0; }
};

class NoiseRadio : public RadioLibInterface
{
  public:
    NoiseRadio() : RadioLibInterface(nullptr, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC) {}
    bool busy = false;
    int16_t rssi = -105;
    unsigned reads = 0;
    void ready(bool receiving = true)
    {
        isReceiving = receiving;
        lastNoiseFloorUpdate = millis() - 60000;
    }
    bool isChannelActive() override { return false; }
    bool isActivelyReceiving() override { return busy; }
    uint32_t getPacketTime(uint32_t, bool) override { return 0; }
    int16_t getCurrentRSSI() override
    {
        ++reads;
        return rssi;
    }
    void addReceiveMetadata(meshtastic_MeshPacket *) override {}
    void setRadioIsr(void (*)()) override {}
    void clearRadioIsr() override {}
};

static NodeDB *savedDB;
static Router *savedRouter;
static MeshService *savedService;
static AirTime *savedAir;
static TransmitHistory *savedHistory;
static concurrency::Lock *savedLock;
static meshtastic_LocalConfig savedConfig;
static meshtastic_ChannelFile savedChannels;
static meshtastic_MyNodeInfo savedNode;
static meshtastic_User savedOwner;
static meshtastic_Position savedPosition;
static uint32_t savedGeneration;
static const RegionInfo *savedRegion;
static CaptureRouter *capture;
static MockMeshService *mesh;

static void fix(int32_t latitude = 407825770)
{
    meshtastic_Position p = meshtastic_Position_init_zero;
    p.has_latitude_i = p.has_longitude_i = true;
    p.latitude_i = latitude;
    p.longitude_i = -1192084390;
    nodeDB->updatePosition(nodeDB->getNodeNum(), p, RX_SRC_LOCAL);
    mesh->refreshLocalMeshNode();
}

void setUp()
{
    savedDB = nodeDB;
    savedRouter = router;
    savedService = service;
    savedAir = airTime;
    savedHistory = transmitHistory;
    savedLock = cryptLock;
    savedConfig = config;
    savedChannels = channelFile;
    savedNode = myNodeInfo;
    savedOwner = owner;
    savedPosition = localPosition;
    savedGeneration = radioGeneration;
    savedRegion = myRegion;
    nodeDB = new NodeDB();
    config = meshtastic_LocalConfig_init_zero;
    config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
    config.lora.override_duty_cycle = true;
    config.position.position_broadcast_secs = 60;
    config.position.broadcast_smart_minimum_interval_secs = 30;
    config.position.broadcast_smart_minimum_distance = 10;
    myRegion = getRegion(meshtastic_Config_LoRaConfig_RegionCode_US);
    airTime = new AirTime();
    cryptLock = nullptr;
    capture = new CaptureRouter();
    router = capture;
    capture->addInterface(std::unique_ptr<RadioInterface>(new IdleRadio()));
    mesh = new MockMeshService();
    service = mesh;
    channelFile = meshtastic_ChannelFile_init_zero;
    channelFile.channels_count = 1;
    auto &ch = channelFile.channels[0];
    ch.role = meshtastic_Channel_Role_PRIMARY;
    ch.has_settings = ch.settings.has_module_settings = true;
    ch.settings.module_settings.position_precision = 32;
    channels.onConfigChanged();
    nodeDB->getOrCreateMeshNode(nodeDB->getNodeNum());
    transmitHistory = nullptr;
    transmitHistory = TransmitHistory::getInstance();
    transmitHistory->clear();
    radioGeneration = 7;
    Time::setTestMillis(600000);
    fix();
}

void tearDown()
{
    while (auto *p = mesh->getForPhone())
        mesh->releaseToPool(p);
    while (auto *q = mesh->getQueueStatusForPhone())
        mesh->releaseQueueStatusToPool(q);
    transmitHistory->clear();
    delete transmitHistory;
    delete capture;
    delete cryptLock;
    delete mesh;
    delete airTime;
    delete nodeDB;
    nodeDB = savedDB;
    router = savedRouter;
    service = savedService;
    airTime = savedAir;
    transmitHistory = savedHistory;
    cryptLock = savedLock;
    config = savedConfig;
    channelFile = savedChannels;
    myNodeInfo = savedNode;
    owner = savedOwner;
    localPosition = savedPosition;
    radioGeneration = savedGeneration;
    myRegion = savedRegion;
    if (channelFile.channels_count)
        channels.onConfigChanged();
    Time::useRealClock();
}

static void test_periodic_rejection_preserves_cadence_and_generation()
{
    PositionModule m;
    capture->result = ERRNO_NO_INTERFACES;
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(0, BroadcastTestAccess::sent(m));
    TEST_ASSERT_EQUAL_UINT32(0, BroadcastTestAccess::generation(m));
    TEST_ASSERT_EQUAL_UINT32(0, transmitHistory->getLastSentToMeshMillis(meshtastic_PortNum_POSITION_APP));
    capture->result = ERRNO_OK;
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(600000, BroadcastTestAccess::sent(m));
    TEST_ASSERT_EQUAL_UINT32(7, BroadcastTestAccess::generation(m));
    TEST_ASSERT_EQUAL_UINT32(1, capture->packets.size());
    TEST_ASSERT_TRUE(capture->packets.back()->decoded.want_response);
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(1, capture->packets.size());
}

static void test_disabled_position_channel_does_not_consume_generation()
{
    PositionModule m;
    channelFile.channels[0].settings.module_settings.position_precision = 0;
    channels.onConfigChanged();
    TEST_ASSERT_FALSE(m.sendOurPosition());
    TEST_ASSERT_EQUAL_UINT32(0, BroadcastTestAccess::generation(m));
    channelFile.channels[0].settings.module_settings.position_precision = 32;
    channels.onConfigChanged();
    TEST_ASSERT_TRUE(m.sendOurPosition());
    TEST_ASSERT_TRUE(capture->packets.back()->decoded.want_response);
}

static void test_stale_position_does_not_start_cadence()
{
    PositionModule m;
    nodeDB->clearLocalPosition();
    TEST_ASSERT_FALSE(m.sendOurPosition());
    TEST_ASSERT_EQUAL_UINT32(0, BroadcastTestAccess::sent(m));
    TEST_ASSERT_EQUAL_UINT32(0, capture->packets.size());
    fix();
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(1, capture->packets.size());
}

static void test_smart_rejection_retries_same_movement_and_records_success()
{
    PositionModule m;
    BroadcastTestAccess::tick(m);
    transmitHistory->clear();
    Time::setTestMillis(900000);
    fix(417825770);
    meshtastic_PositionLite p;
    TEST_ASSERT_TRUE(nodeDB->copyNodePosition(nodeDB->getNodeNum(), p));
    capture->result = ERRNO_NO_INTERFACES;
    BroadcastTestAccess::smart(m, p, 900000);
    TEST_ASSERT_EQUAL_UINT32(600000, BroadcastTestAccess::sent(m));
    TEST_ASSERT_EQUAL_UINT32(0, transmitHistory->getLastSentToMeshMillis(meshtastic_PortNum_POSITION_APP));
    capture->result = ERRNO_OK;
    BroadcastTestAccess::smart(m, p, 900000);
    TEST_ASSERT_EQUAL_UINT32(900000, BroadcastTestAccess::sent(m));
    TEST_ASSERT_EQUAL_UINT32(2, capture->packets.size());
    TEST_ASSERT_NOT_EQUAL(0, transmitHistory->getLastSentToMeshMillis(meshtastic_PortNum_POSITION_APP));
    BroadcastTestAccess::smart(m, p, 900000);
    TEST_ASSERT_EQUAL_UINT32(2, capture->packets.size());
}

static void test_nodeinfo_rejection_keeps_history_and_generation_pending()
{
    NodeInfoModule m;
    capture->result = ERRNO_NO_INTERFACES;
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(0, BroadcastTestAccess::generation(m));
    TEST_ASSERT_EQUAL_UINT32(0, transmitHistory->getLastSentToMeshMillis(meshtastic_PortNum_NODEINFO_APP));
    capture->result = ERRNO_OK;
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(7, BroadcastTestAccess::generation(m));
    TEST_ASSERT_EQUAL_UINT32(1, capture->packets.size());
    TEST_ASSERT_TRUE(capture->packets.back()->decoded.want_response);
}

static void test_nodeinfo_throttle_keeps_generation_pending_and_resets_urgent_mode()
{
    NodeInfoModule m;
    transmitHistory->setLastSentToMesh(meshtastic_PortNum_NODEINFO_APP);
    Time::setTestMillis(millis());
    TEST_ASSERT_FALSE(m.sendOurNodeInfo(NODENUM_BROADCAST, true, 0, true));
    TEST_ASSERT_FALSE(BroadcastTestAccess::urgent(m));
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(0, BroadcastTestAccess::generation(m));
    transmitHistory->clear();
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(7, BroadcastTestAccess::generation(m));
    TEST_ASSERT_TRUE(capture->packets.back()->decoded.want_response);
}

static void test_hidden_nodeinfo_does_not_consume_generation()
{
    NodeInfoModule m;
    config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN;
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(0, BroadcastTestAccess::generation(m));
    config.device.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
    BroadcastTestAccess::tick(m);
    TEST_ASSERT_EQUAL_UINT32(7, BroadcastTestAccess::generation(m));
}

static void test_position_ping_falls_back_when_stale_or_rejected()
{
    PositionModule position;
    NodeInfoModule info;
    auto *savedPos = positionModule;
    auto *savedInfo = nodeInfoModule;
    positionModule = &position;
    nodeInfoModule = &info;
    nodeDB->clearLocalPosition();
    bool staleResult = mesh->trySendPosition(NODENUM_BROADCAST, true);
    transmitHistory->clear();
    fix();
    capture->result = ERRNO_NO_INTERFACES;
    bool rejectedResult = mesh->trySendPosition(NODENUM_BROADCAST, true);
    positionModule = savedPos;
    nodeInfoModule = savedInfo;
    TEST_ASSERT_FALSE(staleResult);
    TEST_ASSERT_FALSE(rejectedResult);
    TEST_ASSERT_EQUAL_UINT32(3, capture->attempts.size());
    TEST_ASSERT_EQUAL(meshtastic_PortNum_NODEINFO_APP, capture->attempts[0]);
    TEST_ASSERT_EQUAL(meshtastic_PortNum_POSITION_APP, capture->attempts[1]);
    TEST_ASSERT_EQUAL(meshtastic_PortNum_NODEINFO_APP, capture->attempts[2]);
}

static void test_smart_minimum_interval_survives_clock_rollover()
{
    PositionModule m;
    Time::setTestMillis(UINT32_MAX - 10000);
    BroadcastTestAccess::tick(m);
    fix(417825770);
    meshtastic_PositionLite p;
    TEST_ASSERT_TRUE(nodeDB->copyNodePosition(nodeDB->getNodeNum(), p));
    Time::setTestMillis(10000);
    BroadcastTestAccess::smart(m, p, 10000);
    TEST_ASSERT_EQUAL_UINT32(1, capture->packets.size());
    Time::setTestMillis(300000);
    BroadcastTestAccess::smart(m, p, 300000);
    TEST_ASSERT_EQUAL_UINT32(2, capture->packets.size());
    TEST_ASSERT_EQUAL_UINT32(300000, BroadcastTestAccess::sent(m));
}

static void test_noise_stats_never_sample_and_report_only_valid_measurements()
{
    auto *saved = RadioLibInterface::instance;
    {
        NoiseRadio radio;
        RadioLibInterface::instance = &radio;
        DeviceTelemetryModule telemetry;
        TEST_ASSERT_EQUAL_INT(0, BroadcastTestAccess::stats(telemetry).variant.local_stats.noise_floor);
        TEST_ASSERT_EQUAL_UINT(0, radio.reads);
        radio.ready();
        radio.updateNoiseFloor();
        TEST_ASSERT_EQUAL_INT(-105, BroadcastTestAccess::stats(telemetry).variant.local_stats.noise_floor);
        TEST_ASSERT_EQUAL_UINT(1, radio.reads);
        radio.ready();
        radio.rssi = -115;
        radio.updateNoiseFloor();
        TEST_ASSERT_EQUAL_INT(-110, BroadcastTestAccess::stats(telemetry).variant.local_stats.noise_floor);
        radio.resetNoiseFloor();
        TEST_ASSERT_EQUAL_INT(0, BroadcastTestAccess::stats(telemetry).variant.local_stats.noise_floor);
        radio.ready();
        radio.rssi = -128;
        radio.updateNoiseFloor();
        TEST_ASSERT_FALSE(radio.hasNoiseFloorSamples());
        radio.ready(false);
        radio.updateNoiseFloor();
        radio.ready();
        radio.busy = true;
        radio.updateNoiseFloor();
        TEST_ASSERT_EQUAL_UINT(3, radio.reads);
    }
    RadioLibInterface::instance = saved;
}

extern "C" void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();
    RUN_TEST(test_periodic_rejection_preserves_cadence_and_generation);
    RUN_TEST(test_disabled_position_channel_does_not_consume_generation);
    RUN_TEST(test_stale_position_does_not_start_cadence);
    RUN_TEST(test_smart_rejection_retries_same_movement_and_records_success);
    RUN_TEST(test_nodeinfo_rejection_keeps_history_and_generation_pending);
    RUN_TEST(test_nodeinfo_throttle_keeps_generation_pending_and_resets_urgent_mode);
    RUN_TEST(test_hidden_nodeinfo_does_not_consume_generation);
    RUN_TEST(test_position_ping_falls_back_when_stale_or_rejected);
    RUN_TEST(test_smart_minimum_interval_survives_clock_rollover);
    RUN_TEST(test_noise_stats_never_sample_and_report_only_valid_measurements);
    exit(UNITY_END());
}
extern "C" void loop() {}

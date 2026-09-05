#include "TrackballInterruptImpl1.h"
#include "InputBroker.h"
#include "configuration.h"

TrackballInterruptImpl1 *trackballInterruptImpl1;

static void scheduleInputPoll()
{
#if defined(MUZI_BASE)
    trackballInterruptImpl1->setIntervalFromNow(0);
    runASAP = true;
    BaseType_t higherWake = 0;
    concurrency::mainDelay.interruptFromISR(&higherWake);
#else
    trackballInterruptImpl1->setIntervalFromNow(20);
#endif
}

TrackballInterruptImpl1::TrackballInterruptImpl1() : TrackballInterruptBase("trackball1") {}

void TrackballInterruptImpl1::init(uint8_t pinDown, uint8_t pinUp, uint8_t pinLeft, uint8_t pinRight, uint8_t pinPress)
{
    input_broker_event eventDown = INPUT_BROKER_DOWN;
    input_broker_event eventUp = INPUT_BROKER_UP;
    input_broker_event eventLeft = INPUT_BROKER_LEFT;
    input_broker_event eventRight = INPUT_BROKER_RIGHT;
    input_broker_event eventPressed = INPUT_BROKER_SELECT;
    input_broker_event eventPressedLong = INPUT_BROKER_SELECT_LONG;

    TrackballInterruptBase::init(pinDown, pinUp, pinLeft, pinRight, pinPress, eventDown, eventUp, eventLeft, eventRight,
                                 eventPressed, eventPressedLong, TrackballInterruptImpl1::handleIntDown,
                                 TrackballInterruptImpl1::handleIntUp, TrackballInterruptImpl1::handleIntLeft,
                                 TrackballInterruptImpl1::handleIntRight, TrackballInterruptImpl1::handleIntPressed);
    inputBroker->registerSource(this);
}

void TrackballInterruptImpl1::handleIntDown()
{
    trackballInterruptImpl1->intDownHandler();
    scheduleInputPoll();
}
void TrackballInterruptImpl1::handleIntUp()
{
    trackballInterruptImpl1->intUpHandler();
    scheduleInputPoll();
}
void TrackballInterruptImpl1::handleIntLeft()
{
    trackballInterruptImpl1->intLeftHandler();
    scheduleInputPoll();
}
void TrackballInterruptImpl1::handleIntRight()
{
    trackballInterruptImpl1->intRightHandler();
    scheduleInputPoll();
}
void TrackballInterruptImpl1::handleIntPressed()
{
    trackballInterruptImpl1->intPressHandler();
    scheduleInputPoll();
}

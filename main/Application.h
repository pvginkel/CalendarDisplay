#pragma once

#include "ApplicationBase.h"
#include "Buttons.h"
#include "CalendarUI.h"
#include "LoadingUI.h"

class Application : public ApplicationBase {
    Device* _device;
    LoadingUI* _loading_ui;
    CalendarUI* _calendar_ui;
    Buttons _buttons;

public:
    Application(Device* device);

protected:
    void do_begin() override;
    void do_ready() override;
    void do_network_connection_failed() override;
    void do_process() override;

private:
    void state_changed();
};

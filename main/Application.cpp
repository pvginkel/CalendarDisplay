#include "includes.h"

#include "Application.h"

#include "Messages.h"
#include "driver/i2c.h"

LOG_TAG(Application);

Application::Application(Device* device)
    : _device(device), _loading_ui(nullptr), _calendar_ui(nullptr), _buttons(&get_queue()) {}

void Application::do_begin() {
    ESP_LOGI(TAG, "Setting up loading UI");

    get_mqtt_connection().on_connected_changed([this](auto state) {
        if (state.connected) {
            state_changed();
        }
    });

    _loading_ui = new LoadingUI(is_silent_startup());

    _loading_ui->begin();
    _loading_ui->set_title(MSG_STARTING);
    _loading_ui->set_state(LoadingUIState::Loading);
    _loading_ui->render();
}

void Application::do_network_connection_failed() {
    if (_loading_ui) {
        _loading_ui->set_error(MSG_FAILED_TO_CONNECT);
        _loading_ui->set_state(LoadingUIState::Error);
        _loading_ui->render();
    }
}

void Application::do_ready() {
    // Intialization complete.

    delete _loading_ui;
    _loading_ui = nullptr;

    // Enable the buttons.
    _buttons.begin();

    ESP_LOGI(TAG, "Connected, showing UI");

    _calendar_ui = new CalendarUI(this, _device, &_buttons);
    _calendar_ui->begin();
}

void Application::do_process() {
    _device->process();

    if (_calendar_ui) {
        _calendar_ui->update();
    }
}

void Application::state_changed() {
    if (!get_mqtt_connection().is_connected()) {
        return;
    }

    get_mqtt_connection().send_state();
}

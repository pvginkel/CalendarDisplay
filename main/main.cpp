#include "includes.h"

#include "Application.h"

extern "C" {
void app_main(void) {
    Device device;

    device.begin();

    Application application(&device);

    application.begin();

    while (1) {
        application.process();
    }
}
}

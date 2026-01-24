#include "apps.h"
#include "../common/components.h"
#include "../ui_internal.h"
#include <stddef.h>

static void home_renderPreview(void) {
    comp_time_and_wifi();
}

static void home_poll(void) {
    // Force redraw every second to update clock
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    if (now - lastUpdate >= 1000) {
        lastUpdate = now;
        ui_redraw();
    }
}

const App APP_HOME = {
    "Home",
    home_renderPreview,
    NULL, // No specific action on select
    home_poll
};

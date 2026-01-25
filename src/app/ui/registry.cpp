#include "registry.h"
#include "apps/apps.h"

static const App* s_apps[] = {
    &APP_HOME,
    &APP_TEXT,
    &APP_BESZEL,
    &APP_NYT,
    &APP_EPUB,
    &APP_SETTINGS
};

const App** registry_getApps(void) {
    return s_apps;
}

size_t registry_getCount(void) {
    return sizeof(s_apps) / sizeof(s_apps[0]);
}

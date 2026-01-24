#pragma once
#include "common/types.h"
#include <stddef.h>

// Returns the list of registered apps
const App** registry_getApps(void);
// Returns the number of registered apps
size_t registry_getCount(void);

#pragma once
#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Set the current active view (screens inside an app).
// Passing NULL returns to the main app carousel.
void ui_setView(const View* view);

// Request a redraw of the current screen
void ui_redraw(void);

// Trigger a vertical animation (for submenus or carousel)
void ui_triggerVerticalAnimation(bool up);

#ifdef __cplusplus
}
#endif

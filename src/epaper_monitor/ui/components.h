// Minimal UI components (headers)
#pragma once

#include <Arduino.h>

// Forward view type so views can reference it
struct View;

// Small components: Title+Text and Switch
void comp_title_and_text(const char *title, const char *text);
void comp_time_and_wifi(void);

// Small switch component: render a label + ON/OFF state
void comp_switch(const char *label, bool state);

// Button helper (renders a label centered)
void comp_button(const char *label);

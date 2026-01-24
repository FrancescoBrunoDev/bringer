// Minimal UI components (headers)
#pragma once

#include <Arduino.h>

// Forward view type so views can reference it
struct View;

// Small components: Title+Text and Switch
void comp_title_and_text(const char *title, const char *text, int16_t x_offset = 0, int16_t y_offset = 0, bool update = true);
void comp_time_and_wifi(int16_t x_offset = 0, int16_t y_offset = 0, bool update = true);

// Small switch component: render a label + ON/OFF state
void comp_switch(const char *label, bool state);

// New toggle component using big text and animations
void comp_toggle(const char *label, bool state, int16_t x_offset = 0, int16_t y_offset = 0);

// Button helper (renders a label centered)
void comp_button(const char *label);

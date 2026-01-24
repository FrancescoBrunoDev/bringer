#pragma once

#include <Arduino.h>
#include <vector>
#include <stdint.h>

enum EpdComponentType {
    EPD_COMP_HEADER,
    EPD_COMP_ROW,
    EPD_COMP_PROGRESS,
    EPD_COMP_SEPARATOR
};

struct EpdComponent {
    EpdComponentType type;
    String text1;
    String text2;
    float value;
    uint16_t color;
};

// A page is a collection of components to be rendered on the e-paper
struct EpdPage {
    String title;
    std::vector<EpdComponent> components;
};

// API to queue a structured page for rendering
void epd_displayPage(const EpdPage& page);

#include "HardwareButtonBar.h"
#include <string.h>

HardwareButtonBar::HardwareButtonBar()
    : bounds_{0, 218, 320, 22},
      enabled_a_(true),
      enabled_b_(true),
      enabled_c_(true) {
    strcpy(label_a_, "");
    strcpy(label_b_, "");
    strcpy(label_c_, "");
}

void HardwareButtonBar::setBounds(int16_t x, int16_t y, int16_t w, int16_t h) {
    bounds_.x = x;
    bounds_.y = y;
    bounds_.w = w;
    bounds_.h = h;
}

void HardwareButtonBar::setLabels(const char* btnA, const char* btnB, const char* btnC) {
    if (btnA) {
        strncpy(label_a_, btnA, sizeof(label_a_) - 1);
        label_a_[sizeof(label_a_) - 1] = '\0';
    }
    if (btnB) {
        strncpy(label_b_, btnB, sizeof(label_b_) - 1);
        label_b_[sizeof(label_b_) - 1] = '\0';
    }
    if (btnC) {
        strncpy(label_c_, btnC, sizeof(label_c_) - 1);
        label_c_[sizeof(label_c_) - 1] = '\0';
    }
}

void HardwareButtonBar::setEnabled(bool btnA, bool btnB, bool btnC) {
    enabled_a_ = btnA;
    enabled_b_ = btnB;
    enabled_c_ = btnC;
}

void HardwareButtonBar::draw(Renderer& renderer) {
    // Draw background
    renderer.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h,
                     Renderer::Color(COLOR_BACKGROUND), true);

    // Draw vertical dividers between buttons
    // Divider 1: between BtnA and BtnB (x=106-108)
    renderer.drawRect(BTN_A_X + BTN_A_W + 1, bounds_.y, 2, bounds_.h,
                     Renderer::Color(COLOR_DIVIDER), true);

    // Divider 2: between BtnB and BtnC (x=212-214)
    renderer.drawRect(BTN_B_X + BTN_B_W + 1, bounds_.y, 2, bounds_.h,
                     Renderer::Color(COLOR_DIVIDER), true);

    // Draw button labels
    drawButton(renderer, BTN_A_X, BTN_A_W, label_a_, enabled_a_);
    drawButton(renderer, BTN_B_X, BTN_B_W, label_b_, enabled_b_);
    drawButton(renderer, BTN_C_X, BTN_C_W, label_c_, enabled_c_);
}

void HardwareButtonBar::drawButton(Renderer& renderer, int16_t x, int16_t w,
                                   const char* label, bool enabled) {
    if (!label || strlen(label) == 0) {
        return;  // Skip empty labels
    }

    // Calculate center position for text
    int16_t center_x = x + (w / 2);
    int16_t center_y = bounds_.y + (bounds_.h / 2);

    // Choose color based on enabled state
    Renderer::Color text_color = enabled ? Renderer::Color(COLOR_TEXT_ENABLED)
                                         : Renderer::Color(COLOR_TEXT_DISABLED);

    // Draw label centered
    renderer.setTextDatum(MC_DATUM);  // Middle-center
    renderer.drawString(center_x, center_y, label, &fonts::Font2, text_color);
}

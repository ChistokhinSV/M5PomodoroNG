#ifndef HARDWARE_BUTTON_BAR_H
#define HARDWARE_BUTTON_BAR_H

#include "../Renderer.h"

/**
 * HardwareButtonBar - On-screen labels for M5Stack Core2 hardware buttons
 *
 * Layout (320×22 pixels, y=218-240):
 * ┌──────────────────────────────────────┐
 * │ [BtnA Label] [BtnB Label] [BtnC Label] │
 * └──────────────────────────────────────┘
 *    x=3-105      x=109-211     x=215-317
 *
 * Features:
 * - 3-column layout aligned with M5Stack Core2 capacitive touch zones
 * - Enabled/disabled visual states (gray text for disabled)
 * - Dark background (#202020) with subtle dividers
 * - Centered text per button zone
 *
 * Usage:
 *   HardwareButtonBar bar;
 *   bar.setBounds(0, 218, 320, 22);
 *   bar.setLabels("Back", "Start", "Settings");
 *   bar.setEnabled(true, true, false);  // Disable BtnC
 *   bar.draw(renderer);
 */
class HardwareButtonBar {
public:
    HardwareButtonBar();

    // Configuration
    void setBounds(int16_t x, int16_t y, int16_t w, int16_t h);
    void setLabels(const char* btnA, const char* btnB, const char* btnC);
    void setEnabled(bool btnA, bool btnB, bool btnC);

    // Rendering
    void draw(Renderer& renderer);

private:
    // Bounds
    Renderer::Rect bounds_;

    // Labels (max 12 chars each to fit in 102px width)
    char label_a_[13];
    char label_b_[13];
    char label_c_[13];

    // Enabled states
    bool enabled_a_;
    bool enabled_b_;
    bool enabled_c_;

    // Layout constants (M5Stack Core2 hardware button zones)
    static constexpr int16_t BTN_A_X = 3;
    static constexpr int16_t BTN_A_W = 102;
    static constexpr int16_t BTN_B_X = 109;
    static constexpr int16_t BTN_B_W = 102;
    static constexpr int16_t BTN_C_X = 215;
    static constexpr int16_t BTN_C_W = 102;

    // Colors
    static constexpr uint16_t COLOR_BACKGROUND = 0x2104;  // Dark gray #202020
    static constexpr uint16_t COLOR_DIVIDER = 0x39E7;     // Light gray #3A3A3A
    static constexpr uint16_t COLOR_TEXT_ENABLED = TFT_WHITE;
    static constexpr uint16_t COLOR_TEXT_DISABLED = 0x7BEF;  // Gray #7A7A7A

    // Helper methods
    void drawButton(Renderer& renderer, int16_t x, int16_t w, const char* label, bool enabled);
};

#endif // HARDWARE_BUTTON_BAR_H

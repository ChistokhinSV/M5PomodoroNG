#include "PauseScreen.h"
#include <M5Unified.h>
#include <stdio.h>

PauseScreen::PauseScreen(TimerStateMachine& state_machine,
                         LEDController& led_controller,
                         NavigationCallback navigate_callback)
    : state_machine_(state_machine),
      led_controller_(led_controller),
      navigate_callback_(navigate_callback),
      needs_redraw_(true) {

    // Configure widgets with layout positions
    // Status bar at top (320×20)
    status_bar_.setBounds(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT);

    // Note: Hardware buttons replaced custom touch buttons
    // BtnA (left): Resume timer
    // BtnC (right): Stop timer
}

void PauseScreen::updateStatus(uint8_t battery, bool charging, bool wifi,
                                const char* mode, uint8_t hour, uint8_t minute) {
    status_bar_.updateBattery(battery, charging);
    status_bar_.updateWiFi(wifi);
    status_bar_.updateMode(mode);
    status_bar_.updateTime(hour, minute);
}

void PauseScreen::update(uint32_t deltaMs) {
    // Set LED to pulsing amber
    led_controller_.setPattern(LEDController::Pattern::PULSE,
                               LEDController::Color::Orange());  // Amber/Orange

    // Update widgets
    status_bar_.update(deltaMs);

    needs_redraw_ = true;
}

void PauseScreen::draw(Renderer& renderer) {
    if (!needs_redraw_) return;

    // Clear background (black)
    renderer.clear(Renderer::Color(TFT_BLACK));

    // Draw status bar at top
    status_bar_.draw(renderer);

    // Draw "PAUSED" text
    drawPausedText(renderer);

    // Draw frozen timer
    drawTimer(renderer);

    // Hardware buttons drawn by ScreenManager (HardwareButtonBar)

    needs_redraw_ = false;
}

void PauseScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
    // Touch handling removed - now uses hardware buttons
    // Hardware button handling done by ScreenManager via onButtonA/C()
}

void PauseScreen::drawPausedText(Renderer& renderer) {
    // Draw large "PAUSED" text centered in amber
    renderer.setTextDatum(MC_DATUM);  // Middle-center
    renderer.drawString(SCREEN_WIDTH / 2, PAUSED_TEXT_Y, "PAUSED",
                       &fonts::Font6, Renderer::Color(COLOR_AMBER));
}

void PauseScreen::drawTimer(Renderer& renderer) {
    // Get frozen remaining time from state machine
    uint8_t minutes, seconds;
    state_machine_.getRemainingTime(minutes, seconds);

    // Format as MM:SS
    char time_str[6];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", minutes, seconds);

    // Draw timer in amber (large font)
    renderer.setTextDatum(MC_DATUM);  // Middle-center
    renderer.drawString(SCREEN_WIDTH / 2, TIMER_Y, time_str,
                       &fonts::Font8, Renderer::Color(COLOR_AMBER));
}

// Hardware button interface implementation
void PauseScreen::getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC) {
    btnA = "Resume";  // Resume timer
    btnB = "";        // Unused
    btnC = "Stop";    // Stop timer
}

void PauseScreen::onButtonA() {
    // Resume timer
    Serial.println("[PauseScreen] BtnA: Resume timer");
    state_machine_.handleEvent(TimerStateMachine::Event::RESUME);
    needs_redraw_ = true;
}

void PauseScreen::onButtonB() {
    // Unused
}

void PauseScreen::onButtonC() {
    // Stop timer
    Serial.println("[PauseScreen] BtnC: Stop timer");
    state_machine_.handleEvent(TimerStateMachine::Event::STOP);
    needs_redraw_ = true;
}

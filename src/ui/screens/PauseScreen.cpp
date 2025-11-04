#include "PauseScreen.h"
#include <M5Unified.h>
#include <stdio.h>

// Static instance pointer for button callbacks
PauseScreen* PauseScreen::instance_ = nullptr;

PauseScreen::PauseScreen(TimerStateMachine& state_machine, LEDController& led_controller)
    : state_machine_(state_machine),
      led_controller_(led_controller),
      needs_redraw_(true) {

    // Set static instance for callbacks
    instance_ = this;

    // Configure widgets with layout positions
    // Status bar at top (320×20)
    status_bar_.setBounds(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT);

    // Resume button (left side)
    btn_resume_.setBounds(40, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
    btn_resume_.setLabel("Resume");
    btn_resume_.setCallback(onResumePress);
    btn_resume_.setColors(Renderer::Color(COLOR_AMBER), Renderer::Color(COLOR_AMBER_DARK));

    // Stop button (right side)
    btn_stop_.setBounds(170, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
    btn_stop_.setLabel("Stop");
    btn_stop_.setCallback(onStopPress);
    btn_stop_.setColors(Renderer::Color(COLOR_AMBER), Renderer::Color(COLOR_AMBER_DARK));
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
    btn_resume_.update(deltaMs);
    btn_stop_.update(deltaMs);

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

    // Draw buttons
    btn_resume_.draw(renderer);
    btn_stop_.draw(renderer);

    needs_redraw_ = false;
}

void PauseScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        // Touch down - check button hits
        if (btn_resume_.hitTest(x, y)) {
            btn_resume_.onTouch(x, y);
        } else if (btn_stop_.hitTest(x, y)) {
            btn_stop_.onTouch(x, y);
        }
    } else {
        // Touch up - release buttons
        btn_resume_.onRelease(x, y);
        btn_stop_.onRelease(x, y);
    }
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

// Button callback implementations
void PauseScreen::onResumePress() {
    if (!instance_) return;

    // Resume timer → goes back to RUNNING or BREAK state
    instance_->state_machine_.handleEvent(TimerStateMachine::Event::RESUME);
    Serial.println("[PauseScreen] Resume timer");

    instance_->needs_redraw_ = true;
}

void PauseScreen::onStopPress() {
    if (!instance_) return;

    // Stop timer → goes to IDLE state
    instance_->state_machine_.handleEvent(TimerStateMachine::Event::STOP);
    Serial.println("[PauseScreen] Stop timer");

    instance_->needs_redraw_ = true;
}

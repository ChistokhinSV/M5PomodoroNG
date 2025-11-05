#include "SettingsScreen.h"
#include "../ScreenManager.h"
#include <M5Unified.h>
#include <stdio.h>

// Static instance pointer for button callbacks
SettingsScreen* SettingsScreen::instance_ = nullptr;

SettingsScreen::SettingsScreen(Config& config)
    : config_(config),
      current_page_(0),
      needs_redraw_(true) {

    // Set static instance for callbacks
    instance_ = this;

    // Configure status bar
    status_bar_.setBounds(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT);

    // Configure navigation buttons
    btn_back_.setBounds(10, SCREEN_HEIGHT - NAV_HEIGHT + 5, 70, 25);
    btn_back_.setLabel("<- Back");
    btn_back_.setCallback(onBackPress);

    btn_prev_.setBounds(90, SCREEN_HEIGHT - NAV_HEIGHT + 5, 60, 25);
    btn_prev_.setLabel("Prev");
    btn_prev_.setCallback(onPrevPress);

    btn_next_.setBounds(160, SCREEN_HEIGHT - NAV_HEIGHT + 5, 60, 25);
    btn_next_.setLabel("Next");
    btn_next_.setCallback(onNextPress);

    btn_reset_.setBounds(230, SCREEN_HEIGHT - NAV_HEIGHT + 5, 80, 25);
    btn_reset_.setLabel("Reset");
    btn_reset_.setCallback(onResetPress);

    // Calculate widget positions (CSS-style cumulative Y positioning)
    int16_t widget_start_y = STATUS_BAR_HEIGHT + TITLE_HEIGHT + 5;
    int16_t y = widget_start_y;

    // Page 0: Timer settings (3 sliders)
    slider_work_duration_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_work_duration_.setLabel("Work:");
    slider_work_duration_.setRange(5, 90);
    slider_work_duration_.setDisplayMode(Slider::DisplayMode::TIME_MIN);
    slider_work_duration_.setCallback([this](uint16_t val) { this->onWorkDurationChange(val); });
    slider_work_duration_.setMarginBottom(10);
    y += slider_work_duration_.getTotalHeight();

    slider_short_break_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_short_break_.setLabel("Short Break:");
    slider_short_break_.setRange(1, 10);
    slider_short_break_.setDisplayMode(Slider::DisplayMode::TIME_MIN);
    slider_short_break_.setCallback([this](uint16_t val) { this->onShortBreakChange(val); });
    slider_short_break_.setMarginBottom(10);
    y += slider_short_break_.getTotalHeight();

    slider_long_break_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_long_break_.setLabel("Long Break:");
    slider_long_break_.setRange(10, 30);
    slider_long_break_.setDisplayMode(Slider::DisplayMode::TIME_MIN);
    slider_long_break_.setCallback([this](uint16_t val) { this->onLongBreakChange(val); });
    slider_long_break_.setMarginBottom(10);

    // Page 1: Timer settings (1 slider + 2 toggles)
    y = widget_start_y;
    slider_sessions_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_sessions_.setLabel("Sessions:");
    slider_sessions_.setRange(1, 8);
    slider_sessions_.setDisplayMode(Slider::DisplayMode::NUMERIC);
    slider_sessions_.setCallback([this](uint16_t val) { this->onSessionsChange(val); });
    slider_sessions_.setMarginBottom(10);
    y += slider_sessions_.getTotalHeight();

    toggle_auto_break_.setBounds(10, y, 300, 20);  // Toggle height = 20px
    toggle_auto_break_.setLabel("Auto-start breaks");
    toggle_auto_break_.setCallback([this](bool val) { this->onAutoBreakChange(val); });
    toggle_auto_break_.setMarginBottom(10);
    y += toggle_auto_break_.getTotalHeight();

    toggle_auto_work_.setBounds(10, y, 300, 20);  // Toggle height = 20px
    toggle_auto_work_.setLabel("Auto-start work");
    toggle_auto_work_.setCallback([this](bool val) { this->onAutoWorkChange(val); });
    toggle_auto_work_.setMarginBottom(10);

    // Page 2: UI settings (1 slider + 1 toggle + 1 slider)
    y = widget_start_y;
    slider_brightness_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_brightness_.setLabel("Brightness:");
    slider_brightness_.setRange(0, 100);
    slider_brightness_.setDisplayMode(Slider::DisplayMode::PERCENTAGE);
    slider_brightness_.setCallback([this](uint16_t val) { this->onBrightnessChange(val); });
    slider_brightness_.setMarginBottom(10);
    y += slider_brightness_.getTotalHeight();

    toggle_sound_.setBounds(10, y, 300, 20);  // Toggle height = 20px
    toggle_sound_.setLabel("Sound enabled");
    toggle_sound_.setCallback([this](bool val) { this->onSoundChange(val); });
    toggle_sound_.setMarginBottom(10);
    y += toggle_sound_.getTotalHeight();

    slider_volume_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_volume_.setLabel("Volume:");
    slider_volume_.setRange(0, 100);
    slider_volume_.setDisplayMode(Slider::DisplayMode::PERCENTAGE);
    slider_volume_.setCallback([this](uint16_t val) { this->onVolumeChange(val); });
    slider_volume_.setMarginBottom(10);

    // Page 3: UI settings (2 toggles + 1 slider)
    y = widget_start_y;
    toggle_haptic_.setBounds(10, y, 300, 20);  // Toggle height = 20px
    toggle_haptic_.setLabel("Haptic feedback");
    toggle_haptic_.setCallback([this](bool val) { this->onHapticChange(val); });
    toggle_haptic_.setMarginBottom(10);
    y += toggle_haptic_.getTotalHeight();

    toggle_show_seconds_.setBounds(10, y, 300, 20);  // Toggle height = 20px
    toggle_show_seconds_.setLabel("Show seconds");
    toggle_show_seconds_.setCallback([this](bool val) { this->onShowSecondsChange(val); });
    toggle_show_seconds_.setMarginBottom(10);
    y += toggle_show_seconds_.getTotalHeight();

    slider_timeout_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_timeout_.setLabel("Timeout:");
    slider_timeout_.setRange(0, 600);
    slider_timeout_.setDisplayMode(Slider::DisplayMode::TIME_SEC);
    slider_timeout_.setCallback([this](uint16_t val) { this->onTimeoutChange(val); });
    slider_timeout_.setMarginBottom(10);

    // Page 4: Power settings (1 toggle + 1 slider + 1 toggle + 1 slider)
    y = widget_start_y;
    toggle_auto_sleep_.setBounds(10, y, 300, 20);  // Toggle height = 20px
    toggle_auto_sleep_.setLabel("Auto-sleep");
    toggle_auto_sleep_.setCallback([this](bool val) { this->onAutoSleepChange(val); });
    toggle_auto_sleep_.setMarginBottom(10);
    y += toggle_auto_sleep_.getTotalHeight();

    slider_sleep_after_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_sleep_after_.setLabel("Sleep after:");
    slider_sleep_after_.setRange(0, 600);
    slider_sleep_after_.setDisplayMode(Slider::DisplayMode::TIME_MIN);
    slider_sleep_after_.setCallback([this](uint16_t val) { this->onSleepAfterChange(val); });
    slider_sleep_after_.setMarginBottom(10);
    y += slider_sleep_after_.getTotalHeight();

    toggle_wake_rotation_.setBounds(10, y, 300, 20);  // Toggle height = 20px
    toggle_wake_rotation_.setLabel("Wake on rotation");
    toggle_wake_rotation_.setCallback([this](bool val) { this->onWakeRotationChange(val); });
    toggle_wake_rotation_.setMarginBottom(10);
    y += toggle_wake_rotation_.getTotalHeight();

    slider_min_battery_.setBounds(10, y, 300, WIDGET_HEIGHT);
    slider_min_battery_.setLabel("Low battery:");
    slider_min_battery_.setRange(5, 50);
    slider_min_battery_.setDisplayMode(Slider::DisplayMode::PERCENTAGE);
    slider_min_battery_.setCallback([this](uint16_t val) { this->onMinBatteryChange(val); });
    slider_min_battery_.setMarginBottom(10);

    // Load initial values from Config
    loadFromConfig();
}

void SettingsScreen::loadFromConfig() {
    // Load Pomodoro settings
    auto pomodoro = config_.getPomodoro();
    slider_work_duration_.setValue(pomodoro.work_duration_min);
    slider_short_break_.setValue(pomodoro.short_break_min);
    slider_long_break_.setValue(pomodoro.long_break_min);
    slider_sessions_.setValue(pomodoro.sessions_before_long);
    toggle_auto_break_.setState(pomodoro.auto_start_breaks);
    toggle_auto_work_.setState(pomodoro.auto_start_work);

    // Load UI settings
    auto ui = config_.getUI();
    slider_brightness_.setValue(ui.brightness);
    toggle_sound_.setState(ui.sound_enabled);
    slider_volume_.setValue(ui.sound_volume);
    toggle_haptic_.setState(ui.haptic_enabled);
    toggle_show_seconds_.setState(ui.show_seconds);
    slider_timeout_.setValue(ui.screen_timeout_sec);

    // Load Power settings
    auto power = config_.getPower();
    toggle_auto_sleep_.setState(power.auto_sleep_enabled);
    slider_sleep_after_.setValue(power.sleep_after_min);
    toggle_wake_rotation_.setState(power.wake_on_rotation);
    slider_min_battery_.setValue(power.min_battery_percent);
}

void SettingsScreen::updateStatus(uint8_t battery, bool charging, bool wifi,
                                   const char* mode, uint8_t hour, uint8_t minute) {
    status_bar_.updateBattery(battery, charging);
    status_bar_.updateWiFi(wifi);
    status_bar_.updateMode(mode);
    status_bar_.updateTime(hour, minute);
}

void SettingsScreen::update(uint32_t deltaMs) {
    // Update widgets on current page only
    status_bar_.update(deltaMs);

    if (current_page_ == 0) {
        slider_work_duration_.update(deltaMs);
        slider_short_break_.update(deltaMs);
        slider_long_break_.update(deltaMs);
    } else if (current_page_ == 1) {
        slider_sessions_.update(deltaMs);
        toggle_auto_break_.update(deltaMs);
        toggle_auto_work_.update(deltaMs);
    } else if (current_page_ == 2) {
        slider_brightness_.update(deltaMs);
        toggle_sound_.update(deltaMs);
        slider_volume_.update(deltaMs);
    } else if (current_page_ == 3) {
        toggle_haptic_.update(deltaMs);
        toggle_show_seconds_.update(deltaMs);
        slider_timeout_.update(deltaMs);
    } else if (current_page_ == 4) {
        toggle_auto_sleep_.update(deltaMs);
        slider_sleep_after_.update(deltaMs);
        toggle_wake_rotation_.update(deltaMs);
        slider_min_battery_.update(deltaMs);
    }

    // Update navigation buttons
    btn_back_.update(deltaMs);
    btn_prev_.update(deltaMs);
    btn_next_.update(deltaMs);
    btn_reset_.update(deltaMs);

    needs_redraw_ = true;
}

void SettingsScreen::draw(Renderer& renderer) {
    if (!needs_redraw_) return;

    // Clear background
    renderer.clear(Renderer::Color(TFT_BLACK));

    // Draw status bar
    status_bar_.draw(renderer);

    // Draw title and page indicator
    drawTitle(renderer);
    drawPageIndicator(renderer);

    // Draw current page content
    if (current_page_ == 0) {
        drawPage0(renderer);
    } else if (current_page_ == 1) {
        drawPage1(renderer);
    } else if (current_page_ == 2) {
        drawPage2(renderer);
    } else if (current_page_ == 3) {
        drawPage3(renderer);
    } else if (current_page_ == 4) {
        drawPage4(renderer);
    }

    // Draw navigation buttons
    btn_back_.draw(renderer);

    // Show Prev button only if not on first page
    if (current_page_ > 0) {
        btn_prev_.setVisible(true);
        btn_prev_.draw(renderer);
    } else {
        btn_prev_.setVisible(false);
    }

    // Show Next button only if not on last page
    if (current_page_ < TOTAL_PAGES - 1) {
        btn_next_.setVisible(true);
        btn_next_.draw(renderer);
    } else {
        btn_next_.setVisible(false);
    }

    // Show Reset button only on last page
    if (current_page_ == TOTAL_PAGES - 1) {
        btn_reset_.setVisible(true);
        btn_reset_.draw(renderer);
    } else {
        btn_reset_.setVisible(false);
    }

    needs_redraw_ = false;
}

void SettingsScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        // Touch down - check button hits
        if (btn_back_.hitTest(x, y)) {
            btn_back_.onTouch(x, y);
        } else if (btn_prev_.isVisible() && btn_prev_.hitTest(x, y)) {
            btn_prev_.onTouch(x, y);
        } else if (btn_next_.isVisible() && btn_next_.hitTest(x, y)) {
            btn_next_.onTouch(x, y);
        } else if (btn_reset_.isVisible() && btn_reset_.hitTest(x, y)) {
            btn_reset_.onTouch(x, y);
        }
        // Check widgets on current page
        else if (current_page_ == 0) {
            if (slider_work_duration_.hitTest(x, y)) slider_work_duration_.onTouch(x, y);
            else if (slider_short_break_.hitTest(x, y)) slider_short_break_.onTouch(x, y);
            else if (slider_long_break_.hitTest(x, y)) slider_long_break_.onTouch(x, y);
        } else if (current_page_ == 1) {
            if (slider_sessions_.hitTest(x, y)) slider_sessions_.onTouch(x, y);
            else if (toggle_auto_break_.hitTest(x, y)) toggle_auto_break_.onTouch(x, y);
            else if (toggle_auto_work_.hitTest(x, y)) toggle_auto_work_.onTouch(x, y);
        } else if (current_page_ == 2) {
            if (slider_brightness_.hitTest(x, y)) slider_brightness_.onTouch(x, y);
            else if (toggle_sound_.hitTest(x, y)) toggle_sound_.onTouch(x, y);
            else if (slider_volume_.hitTest(x, y)) slider_volume_.onTouch(x, y);
        } else if (current_page_ == 3) {
            if (toggle_haptic_.hitTest(x, y)) toggle_haptic_.onTouch(x, y);
            else if (toggle_show_seconds_.hitTest(x, y)) toggle_show_seconds_.onTouch(x, y);
            else if (slider_timeout_.hitTest(x, y)) slider_timeout_.onTouch(x, y);
        } else if (current_page_ == 4) {
            if (toggle_auto_sleep_.hitTest(x, y)) toggle_auto_sleep_.onTouch(x, y);
            else if (slider_sleep_after_.hitTest(x, y)) slider_sleep_after_.onTouch(x, y);
            else if (toggle_wake_rotation_.hitTest(x, y)) toggle_wake_rotation_.onTouch(x, y);
            else if (slider_min_battery_.hitTest(x, y)) slider_min_battery_.onTouch(x, y);
        }
    } else {
        // Touch up - release all buttons and widgets
        btn_back_.onRelease(x, y);
        btn_prev_.onRelease(x, y);
        btn_next_.onRelease(x, y);
        btn_reset_.onRelease(x, y);

        if (current_page_ == 0) {
            slider_work_duration_.onRelease(x, y);
            slider_short_break_.onRelease(x, y);
            slider_long_break_.onRelease(x, y);
        } else if (current_page_ == 1) {
            slider_sessions_.onRelease(x, y);
            toggle_auto_break_.onRelease(x, y);
            toggle_auto_work_.onRelease(x, y);
        } else if (current_page_ == 2) {
            slider_brightness_.onRelease(x, y);
            toggle_sound_.onRelease(x, y);
            slider_volume_.onRelease(x, y);
        } else if (current_page_ == 3) {
            toggle_haptic_.onRelease(x, y);
            toggle_show_seconds_.onRelease(x, y);
            slider_timeout_.onRelease(x, y);
        } else if (current_page_ == 4) {
            toggle_auto_sleep_.onRelease(x, y);
            slider_sleep_after_.onRelease(x, y);
            toggle_wake_rotation_.onRelease(x, y);
            slider_min_battery_.onRelease(x, y);
        }
    }
}

void SettingsScreen::drawTitle(Renderer& renderer) {
    // Draw page title
    char title[32];
    const char* page_name = "";

    if (current_page_ == 0) page_name = "Timer 1";
    else if (current_page_ == 1) page_name = "Timer 2";
    else if (current_page_ == 2) page_name = "UI 1";
    else if (current_page_ == 3) page_name = "UI 2";
    else if (current_page_ == 4) page_name = "Power";

    snprintf(title, sizeof(title), "Settings - %s (%d/%d)",
             page_name, current_page_ + 1, TOTAL_PAGES);

    renderer.setTextDatum(TL_DATUM);
    renderer.drawString(10, STATUS_BAR_HEIGHT + 5, title,
                       &fonts::Font2, Renderer::Color(TFT_CYAN));
}

void SettingsScreen::drawPageIndicator(Renderer& renderer) {
    // Draw page dots (right side of title area)
    int16_t dot_x = SCREEN_WIDTH - 80;  // Adjusted for 5 dots
    int16_t dot_y = STATUS_BAR_HEIGHT + 12;
    int16_t dot_spacing = 15;

    for (uint8_t i = 0; i < TOTAL_PAGES; i++) {
        bool is_current = (i == current_page_);
        Renderer::Color color = is_current ? Renderer::Color(TFT_CYAN) : Renderer::Color(0x666666);

        renderer.drawCircle(dot_x + i * dot_spacing, dot_y, 4, color, true);
    }
}

void SettingsScreen::drawPage0(Renderer& renderer) {
    // Page 0: Timer settings (3 sliders)
    slider_work_duration_.draw(renderer);
    slider_short_break_.draw(renderer);
    slider_long_break_.draw(renderer);
}

void SettingsScreen::drawPage1(Renderer& renderer) {
    // Page 1: Timer settings (1 slider + 2 toggles)
    slider_sessions_.draw(renderer);
    toggle_auto_break_.draw(renderer);
    toggle_auto_work_.draw(renderer);
}

void SettingsScreen::drawPage2(Renderer& renderer) {
    // Page 2: UI settings (2 sliders + 1 toggle)
    slider_brightness_.draw(renderer);
    toggle_sound_.draw(renderer);
    slider_volume_.draw(renderer);
}

void SettingsScreen::drawPage3(Renderer& renderer) {
    // Page 3: UI settings (2 toggles + 1 slider)
    toggle_haptic_.draw(renderer);
    toggle_show_seconds_.draw(renderer);
    slider_timeout_.draw(renderer);
}

void SettingsScreen::drawPage4(Renderer& renderer) {
    // Page 4: Power settings (4 widgets)
    toggle_auto_sleep_.draw(renderer);
    slider_sleep_after_.draw(renderer);
    toggle_wake_rotation_.draw(renderer);
    slider_min_battery_.draw(renderer);
}

// Navigation button callbacks
void SettingsScreen::onBackPress() {
    if (!instance_) return;

    // Save config before exiting
    instance_->config_.save();
    Serial.println("[SettingsScreen] Back button pressed, config saved");

    // Navigate back to main screen
    if (g_navigate_callback) {
        g_navigate_callback(ScreenID::MAIN);
    }
}

void SettingsScreen::onPrevPress() {
    if (!instance_) return;

    if (instance_->current_page_ > 0) {
        instance_->current_page_--;
        Serial.printf("[SettingsScreen] Previous page: %d\n", instance_->current_page_);
        instance_->needs_redraw_ = true;
    }
}

void SettingsScreen::onNextPress() {
    if (!instance_) return;

    if (instance_->current_page_ < TOTAL_PAGES - 1) {
        instance_->current_page_++;
        Serial.printf("[SettingsScreen] Next page: %d\n", instance_->current_page_);
        instance_->needs_redraw_ = true;
    }
}

void SettingsScreen::onResetPress() {
    if (!instance_) return;

    // Reset config to defaults
    instance_->config_.reset();
    instance_->config_.save();

    // Reload widgets with new values
    instance_->loadFromConfig();

    Serial.println("[SettingsScreen] Reset to defaults");
    instance_->needs_redraw_ = true;
}

// Value change callbacks - Pomodoro settings
void SettingsScreen::onWorkDurationChange(uint16_t value) {
    auto pomodoro = config_.getPomodoro();
    pomodoro.work_duration_min = value;
    config_.setPomodoro(pomodoro);
}

void SettingsScreen::onShortBreakChange(uint16_t value) {
    auto pomodoro = config_.getPomodoro();
    pomodoro.short_break_min = value;
    config_.setPomodoro(pomodoro);
}

void SettingsScreen::onLongBreakChange(uint16_t value) {
    auto pomodoro = config_.getPomodoro();
    pomodoro.long_break_min = value;
    config_.setPomodoro(pomodoro);
}

void SettingsScreen::onSessionsChange(uint16_t value) {
    auto pomodoro = config_.getPomodoro();
    pomodoro.sessions_before_long = value;
    config_.setPomodoro(pomodoro);
}

void SettingsScreen::onAutoBreakChange(bool state) {
    auto pomodoro = config_.getPomodoro();
    pomodoro.auto_start_breaks = state;
    config_.setPomodoro(pomodoro);
}

void SettingsScreen::onAutoWorkChange(bool state) {
    auto pomodoro = config_.getPomodoro();
    pomodoro.auto_start_work = state;
    config_.setPomodoro(pomodoro);
}

// Value change callbacks - UI settings
void SettingsScreen::onBrightnessChange(uint16_t value) {
    auto ui = config_.getUI();
    ui.brightness = value;
    config_.setUI(ui);
}

void SettingsScreen::onSoundChange(bool state) {
    auto ui = config_.getUI();
    ui.sound_enabled = state;
    config_.setUI(ui);
}

void SettingsScreen::onVolumeChange(uint16_t value) {
    auto ui = config_.getUI();
    ui.sound_volume = value;
    config_.setUI(ui);
}

void SettingsScreen::onHapticChange(bool state) {
    auto ui = config_.getUI();
    ui.haptic_enabled = state;
    config_.setUI(ui);
}

void SettingsScreen::onShowSecondsChange(bool state) {
    auto ui = config_.getUI();
    ui.show_seconds = state;
    config_.setUI(ui);
}

void SettingsScreen::onTimeoutChange(uint16_t value) {
    auto ui = config_.getUI();
    ui.screen_timeout_sec = value;
    config_.setUI(ui);
}

// Value change callbacks - Power settings
void SettingsScreen::onAutoSleepChange(bool state) {
    auto power = config_.getPower();
    power.auto_sleep_enabled = state;
    config_.setPower(power);
}

void SettingsScreen::onSleepAfterChange(uint16_t value) {
    auto power = config_.getPower();
    power.sleep_after_min = value;
    config_.setPower(power);
}

void SettingsScreen::onWakeRotationChange(bool state) {
    auto power = config_.getPower();
    power.wake_on_rotation = state;
    config_.setPower(power);
}

void SettingsScreen::onMinBatteryChange(uint16_t value) {
    auto power = config_.getPower();
    power.min_battery_percent = value;
    config_.setPower(power);
}

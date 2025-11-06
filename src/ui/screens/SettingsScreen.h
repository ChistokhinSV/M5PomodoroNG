#ifndef SETTINGSSCREEN_H
#define SETTINGSSCREEN_H

#include <functional>
#include "../Renderer.h"
#include "../widgets/StatusBar.h"
#include "../widgets/Button.h"
#include "../widgets/Slider.h"
#include "../widgets/Toggle.h"
#include "../../core/Config.h"

// Forward declare ScreenID from ScreenManager.h (avoid circular include)
enum class ScreenID;
using NavigationCallback = std::function<void(ScreenID)>;

/**
 * Settings screen - multi-page configuration UI
 *
 * Layout (320×240):
 * ┌─────────────────────────────────┐
 * │ [WiFi][Mode][Time][Battery]     │ ← StatusBar (20px)
 * ├─────────────────────────────────┤
 * │  Settings - Timer (1/3)     ●○○ │ ← Title + page indicator (25px)
 * ├─────────────────────────────────┤
 * │ Work Duration:     [===|==] 25m │ ← Sliders/Toggles (6× ~25px = 150px)
 * │ Short Break:       [==|===]  5m │
 * │ Long Break:        [==|===] 15m │
 * │ Sessions:          [==|===]   4 │
 * │ Auto-start breaks  [ON]         │
 * │ Auto-start work    [OFF]        │
 * ├─────────────────────────────────┤
 * │   [← Back]  [Prev] [Next]       │ ← Navigation (35px)
 * └─────────────────────────────────┘
 *
 * Pages:
 * - Page 0: Timer settings (6 options)
 * - Page 1: UI settings (6 options)
 * - Page 2: Power settings (4 options + Reset button)
 *
 * Features:
 * - 16 configurable settings across 3 pages
 * - Touch navigation between pages
 * - Immediate save to Config on value change
 * - Reset to defaults button on last page
 */
class SettingsScreen {
public:
    SettingsScreen(Config& config, NavigationCallback navigate_callback);

    // Lifecycle
    void draw(Renderer& renderer);
    void update(uint32_t deltaMs);
    void handleTouch(int16_t x, int16_t y, bool pressed);

    // Configuration
    void updateStatus(uint8_t battery, bool charging, bool wifi, const char* mode, uint8_t hour, uint8_t minute);
    void markDirty() { needs_redraw_ = true; }

    // Hardware button interface
    void getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC,
                        bool& enabledA, bool& enabledB, bool& enabledC);
    void onButtonA();  // Back to Main
    void onButtonB();  // Previous page
    void onButtonC();  // Next page

private:
    Config& config_;
    NavigationCallback navigate_callback_;

    // Widgets
    StatusBar status_bar_;

    // Page 0: Timer settings (3 sliders + 3 mode buttons) - MP-50
    Slider slider_work_duration_;
    Slider slider_short_break_;
    Slider slider_long_break_;
    Button button_mode_classic_;   // MP-50: Classic preset (25/5/15)
    Button button_mode_study_;     // MP-50: Study preset (45/15/30)
    Button button_mode_custom_;    // MP-50: Custom preset (user-defined)

    // Page 1: Timer settings (1 slider + 2 toggles)
    Slider slider_sessions_;
    Toggle toggle_auto_break_;
    Toggle toggle_auto_work_;

    // Page 1: UI settings (6 widgets)
    Slider slider_brightness_;
    Toggle toggle_sound_;
    Slider slider_volume_;
    Toggle toggle_haptic_;
    Toggle toggle_show_seconds_;
    Slider slider_timeout_;

    // Page 2: Power settings (4 widgets + button)
    Toggle toggle_auto_sleep_;
    Slider slider_sleep_after_;
    Toggle toggle_wake_rotation_;
    Slider slider_min_battery_;

    // Note: Navigation buttons removed, now using hardware buttons

    // State
    uint8_t current_page_;
    static constexpr uint8_t TOTAL_PAGES = 5;
    bool needs_redraw_;

    // Layout constants
    static constexpr int16_t SCREEN_WIDTH = 320;
    static constexpr int16_t SCREEN_HEIGHT = 240;
    static constexpr int16_t STATUS_BAR_HEIGHT = 20;
    static constexpr int16_t TITLE_HEIGHT = 25;
    static constexpr int16_t WIDGET_HEIGHT = 32;  // Match slider button height
    static constexpr int16_t NAV_HEIGHT = 35;

    // Drawing helpers
    void drawTitle(Renderer& renderer);
    void drawPageIndicator(Renderer& renderer);
    void drawPage0(Renderer& renderer);  // Timer settings (3 sliders)
    void drawPage1(Renderer& renderer);  // Timer settings (2 sliders + 1 toggle)
    void drawPage2(Renderer& renderer);  // UI settings (2 sliders + 1 toggle)
    void drawPage3(Renderer& renderer);  // UI settings (2 toggles + 1 slider)
    void drawPage4(Renderer& renderer);  // Power settings (4 widgets)

    // Initialize widgets from Config
    void loadFromConfig();

    // Value change callbacks
    void onWorkDurationChange(uint16_t value);
    void onShortBreakChange(uint16_t value);
    void onLongBreakChange(uint16_t value);
    void onSessionsChange(uint16_t value);
    void onAutoBreakChange(bool state);
    void onAutoWorkChange(bool state);

    // Mode preset callbacks (MP-50)
    void onModeClassic();
    void onModeStudy();
    void onModeCustom();
    void updateModeHighlights();  // Update button colors based on active mode
    void updateCustomButtonLabel();  // Update Custom button label with current template values

    void onBrightnessChange(uint16_t value);
    void onSoundChange(bool state);
    void onVolumeChange(uint16_t value);
    void onHapticChange(bool state);
    void onShowSecondsChange(bool state);
    void onTimeoutChange(uint16_t value);

    void onAutoSleepChange(bool state);
    void onSleepAfterChange(uint16_t value);
    void onWakeRotationChange(bool state);
    void onMinBatteryChange(uint16_t value);
};

#endif // SETTINGSSCREEN_H

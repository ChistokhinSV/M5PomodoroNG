#ifndef SCREEN_H
#define SCREEN_H

#include "Renderer.h"
#include "TouchEventManager.h"
#include "widgets/Widget.h"
#include <cstdint>

/**
 * Screen - Base class for all UI screens
 *
 * Provides common interface and functionality for screen implementations.
 * Includes built-in TouchEventManager for automatic widget event routing.
 *
 * All screens (MainScreen, StatsScreen, SettingsScreen, PauseScreen) inherit
 * from this class to ensure consistent interface and reduce code duplication.
 *
 * Key Features:
 * - Virtual interface for rendering, updates, and input handling
 * - Built-in TouchEventManager (protected member)
 * - Helper method for widget registration
 * - Hardware button interface (BtnA/B/C)
 * - Status bar update interface
 *
 * Usage Example:
 *
 *   class MyScreen : public Screen {
 *   public:
 *       MyScreen() {
 *           // Register widgets for touch routing
 *           registerWidget(&button_);
 *           registerWidget(&slider_);
 *       }
 *
 *       void draw(Renderer& renderer) override {
 *           renderer.clear(Renderer::Color(TFT_BLACK));
 *           button_.draw(renderer);
 *           slider_.draw(renderer);
 *       }
 *
 *       void update(uint32_t deltaMs) override {
 *           button_.update(deltaMs);
 *           slider_.update(deltaMs);
 *       }
 *
 *   private:
 *       Button button_;
 *       Slider slider_;
 *   };
 *
 * Thread Safety: NOT thread-safe. Call only from UI task (Core 0).
 *
 * MP-62: Screen base class with TouchEventManager integration
 */
class Screen {
public:
    virtual ~Screen() = default;

    /**
     * Draw screen content to renderer.
     * Called by ScreenManager when screen is active and needs redraw.
     *
     * @param renderer Renderer to draw to (320×240 canvas)
     */
    virtual void draw(Renderer& renderer) = 0;

    /**
     * Update screen state and widgets.
     * Called by ScreenManager every frame (~50ms).
     *
     * @param deltaMs Time since last update in milliseconds
     */
    virtual void update(uint32_t deltaMs) = 0;

    /**
     * Handle touch input (down and up events).
     * Default implementation delegates to touch_mgr_ for automatic widget routing.
     * Override if custom touch handling is needed (e.g., gesture detection).
     *
     * @param x Touch X coordinate (0-319, absolute screen coordinates)
     * @param y Touch Y coordinate (0-239, absolute screen coordinates)
     * @param pressed true=touch down, false=touch up
     */
    virtual void handleTouch(int16_t x, int16_t y, bool pressed);

    /**
     * Update status bar information.
     * Called by ScreenManager when battery, time, or network status changes.
     *
     * @param battery Battery level (0-100%)
     * @param charging true if charging
     * @param wifi true if WiFi connected
     * @param mode Current pomodoro mode ("Classic", "Study", "Custom")
     * @param hour Current time hour (0-23)
     * @param minute Current time minute (0-59)
     */
    virtual void updateStatus(uint8_t battery, bool charging, bool wifi,
                              const char* mode, uint8_t hour, uint8_t minute) {}

    /**
     * Mark screen dirty to force redraw on next frame.
     * Call when screen content changes and needs to be redrawn.
     */
    virtual void markDirty() { needs_redraw_ = true; }

    /**
     * Get hardware button labels for display in HardwareButtonBar.
     * Called by ScreenManager to update button bar labels.
     *
     * @param btnA Output: Label for BtnA (left button)
     * @param btnB Output: Label for BtnB (center button)
     * @param btnC Output: Label for BtnC (right button)
     */
    virtual void getButtonLabels(const char*& btnA, const char*& btnB, const char*& btnC) {
        btnA = "";
        btnB = "";
        btnC = "";
    }

    /**
     * Handle BtnA press (left hardware button).
     * Override to provide screen-specific functionality.
     */
    virtual void onButtonA() {}

    /**
     * Handle BtnB press (center hardware button).
     * Override to provide screen-specific functionality.
     */
    virtual void onButtonB() {}

    /**
     * Handle BtnC press (right hardware button).
     * Override to provide screen-specific functionality.
     */
    virtual void onButtonC() {}

protected:
    /**
     * Register widget for touch event routing.
     * Helper method that delegates to touch_mgr_.addWidget().
     * Call from derived class constructor or when adding dynamic widgets.
     *
     * @param widget Pointer to widget (must remain valid while screen is active)
     */
    void registerWidget(Widget* widget);

    /**
     * Unregister widget from touch event routing.
     * Helper method that delegates to touch_mgr_.removeWidget().
     * Call when removing dynamic widgets (e.g., page switching in SettingsScreen).
     *
     * @param widget Pointer to widget to remove
     */
    void unregisterWidget(Widget* widget);

    /**
     * Clear all registered widgets.
     * Helper method that delegates to touch_mgr_.clearWidgets().
     * Useful for page switching or dynamic widget management.
     */
    void clearWidgets();

    TouchEventManager touch_mgr_;  // Automatic touch event routing to widgets
    bool needs_redraw_;            // Flag to trigger redraw (default: true)
};

#endif // SCREEN_H

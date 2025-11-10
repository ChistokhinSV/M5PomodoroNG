#ifndef TOUCH_EVENT_MANAGER_H
#define TOUCH_EVENT_MANAGER_H

#include "widgets/Widget.h"
#include <vector>

/**
 * TouchEventManager - Centralized touch event routing
 *
 * Manages touch event distribution to widgets with automatic hit testing,
 * Z-order support, and release tracking. Eliminates manual if/else chains
 * in screen classes, reducing boilerplate from ~100 lines to ~5 lines.
 *
 * Key Features:
 * - Automatic hit testing and event routing to widgets
 * - Z-order support: last added widget = top layer (wins hit test)
 * - Respects Widget::isVisible() and Widget::isEnabled() flags
 * - Automatic release tracking: only widget that received onTouch gets onRelease
 * - Simple drag handling: widget receives release wherever finger lifts
 *
 * Usage Example:
 *
 *   // In screen constructor
 *   TouchEventManager touch_mgr_;
 *   touch_mgr_.addWidget(&slider_);
 *   touch_mgr_.addWidget(&button_);
 *
 *   // In screen's handleTouch() method
 *   void MyScreen::handleTouch(int16_t x, int16_t y, bool pressed) {
 *       touch_mgr_.handleTouch(x, y, pressed);
 *   }
 *
 *   // For multi-page screens (like SettingsScreen)
 *   void changePage(uint8_t page) {
 *       touch_mgr_.clearWidgets();
 *       if (page == 0) {
 *           touch_mgr_.addWidget(&page0_widget1_);
 *           touch_mgr_.addWidget(&page0_widget2_);
 *       }
 *       // ...
 *   }
 *
 * Thread Safety: NOT thread-safe. Call only from UI task (Core 0).
 *
 * MP-61: Centralized widget touch event routing
 */
class TouchEventManager {
public:
    TouchEventManager();

    /**
     * Register a widget for touch event routing.
     * Widgets are stored in registration order; last added = top layer (Z-order).
     *
     * @param widget Pointer to widget (must remain valid while registered)
     */
    void addWidget(Widget* widget);

    /**
     * Unregister a widget from touch event routing.
     * If this widget is currently active (received onTouch), active state is cleared.
     *
     * @param widget Pointer to widget to remove
     */
    void removeWidget(Widget* widget);

    /**
     * Clear all registered widgets.
     * Resets active widget tracking.
     */
    void clearWidgets();

    /**
     * Handle touch event and route to appropriate widget.
     *
     * Touch Down (pressed=true):
     * - Hit test widgets in reverse order (top to bottom Z-order)
     * - Skip invisible or disabled widgets
     * - First hit widget receives onTouch() and becomes active
     * - Subsequent widgets not tested (top layer wins)
     *
     * Touch Up (pressed=false):
     * - Active widget (received onTouch) gets onRelease()
     * - Release coordinates may differ from touch coordinates (drag support)
     * - Active widget cleared after release
     *
     * @param x Touch X coordinate (absolute screen coordinates, 0-319)
     * @param y Touch Y coordinate (absolute screen coordinates, 0-239)
     * @param pressed true=touch down, false=touch up
     */
    void handleTouch(int16_t x, int16_t y, bool pressed);

    /**
     * Get number of registered widgets.
     * Useful for debugging and validation.
     */
    size_t getWidgetCount() const { return widgets_.size(); }

    /**
     * Get currently active widget (widget that received onTouch).
     * Returns nullptr if no active widget.
     * Useful for debugging and testing.
     */
    Widget* getActiveWidget() const { return active_widget_; }

private:
    std::vector<Widget*> widgets_;    // Z-order: first=bottom, last=top
    Widget* active_widget_;           // Widget that received onTouch (for matching onRelease)
};

#endif // TOUCH_EVENT_MANAGER_H

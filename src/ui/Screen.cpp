#include "Screen.h"

void Screen::handleTouch(int16_t x, int16_t y, bool pressed) {
    // Default implementation: delegate to TouchEventManager for automatic widget routing
    touch_mgr_.handleTouch(x, y, pressed);
}

void Screen::registerWidget(Widget* widget) {
    touch_mgr_.addWidget(widget);
}

void Screen::unregisterWidget(Widget* widget) {
    touch_mgr_.removeWidget(widget);
}

void Screen::clearWidgets() {
    touch_mgr_.clearWidgets();
}

#include "TouchEventManager.h"
#include <algorithm>

TouchEventManager::TouchEventManager()
    : active_widget_(nullptr) {
}

void TouchEventManager::addWidget(Widget* widget) {
    if (widget) {
        widgets_.push_back(widget);
    }
}

void TouchEventManager::removeWidget(Widget* widget) {
    // Remove from widget list
    widgets_.erase(
        std::remove(widgets_.begin(), widgets_.end(), widget),
        widgets_.end()
    );

    // Clear active widget if it was the removed widget
    if (active_widget_ == widget) {
        active_widget_ = nullptr;
    }
}

void TouchEventManager::clearWidgets() {
    widgets_.clear();
    active_widget_ = nullptr;
}

void TouchEventManager::handleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        // Touch down - find topmost hit widget (reverse iteration = top first)
        for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
            Widget* widget = *it;

            // Skip invisible or disabled widgets
            if (!widget->isVisible() || !widget->isEnabled()) {
                continue;
            }

            // Hit test
            if (widget->hitTest(x, y)) {
                widget->onTouch(x, y);
                active_widget_ = widget;
                return;  // Stop at first hit (top layer wins)
            }
        }

        // No hit - clear active widget
        active_widget_ = nullptr;

    } else {
        // Touch up - only release the widget that received onTouch
        if (active_widget_) {
            active_widget_->onRelease(x, y);
            active_widget_ = nullptr;
        }
    }
}

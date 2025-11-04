#include "Widget.h"

bool Widget::hitTest(int16_t x, int16_t y) const {
    if (!visible_ || !enabled_) {
        return false;
    }
    return bounds_.contains(x, y);
}

void Widget::setVisible(bool visible) {
    if (visible_ != visible) {
        visible_ = visible;
        markDirty();
    }
}

void Widget::setEnabled(bool enabled) {
    if (enabled_ != enabled) {
        enabled_ = enabled;
        markDirty();
    }
}

void Widget::setBounds(int16_t x, int16_t y, int16_t w, int16_t h) {
    bounds_.x = x;
    bounds_.y = y;
    bounds_.w = w;
    bounds_.h = h;
    markDirty();
}

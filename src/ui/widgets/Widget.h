#ifndef WIDGET_H
#define WIDGET_H

#include "../Renderer.h"
#include <stdint.h>

/**
 * Base class for all UI widgets
 *
 * Provides common functionality:
 * - Positioning and bounds management
 * - Visibility and enabled state
 * - Touch hit testing
 * - Dirty flag for selective rendering
 *
 * Widgets are rendered using Renderer primitives and manage their own state.
 * They integrate with the dirty rectangle system for efficient screen updates.
 */
class Widget {
public:
    virtual ~Widget() = default;

    // Core rendering lifecycle
    virtual void draw(Renderer& renderer) = 0;
    virtual void update(uint32_t deltaMs) {}

    // Touch handling
    virtual bool hitTest(int16_t x, int16_t y) const;
    virtual void onTouch(int16_t x, int16_t y) {}
    virtual void onRelease(int16_t x, int16_t y) {}

    // State management
    void setVisible(bool visible);
    void setEnabled(bool enabled);
    bool isVisible() const { return visible_; }
    bool isEnabled() const { return enabled_; }
    bool isDirty() const { return dirty_; }

    // Position and bounds
    void setBounds(int16_t x, int16_t y, int16_t w, int16_t h);
    Renderer::Rect getBounds() const { return bounds_; }

    // CSS-style margins (layout only, not interactive)
    void setMargin(int16_t top, int16_t bottom);
    void setMarginBottom(int16_t bottom);
    int16_t getTotalHeight() const { return bounds_.h + margin_top_ + margin_bottom_; }

    // Utility
    void clearDirty() { dirty_ = false; }

protected:
    Renderer::Rect bounds_ = {0, 0, 0, 0};
    int16_t margin_top_ = 0;      // Space above widget (CSS-style)
    int16_t margin_bottom_ = 0;   // Space below widget (CSS-style)
    bool visible_ = true;
    bool enabled_ = true;
    bool dirty_ = true;  // Start dirty to force initial draw

    void markDirty() { dirty_ = true; }
};

#endif // WIDGET_H

#ifndef MOCK_WIDGET_H
#define MOCK_WIDGET_H

#include "../../src/ui/widgets/Widget.h"
#include "../../src/ui/Renderer.h"

/**
 * MockWidget - Test double for Widget class
 *
 * Tracks method calls for verification in unit tests.
 * Used to test TouchEventManager without requiring real widget implementations.
 *
 * Features:
 * - Call counting: onTouch(), onRelease()
 * - Coordinate tracking: last touch/release coordinates
 * - State verification: visible, enabled, bounds
 * - Reset method: clear counters between tests
 *
 * Usage Example:
 *
 *   TEST(TouchEventManagerTest, RoutesSingleTouch) {
 *       MockWidget widget;
 *       widget.setBounds(10, 10, 50, 50);
 *
 *       TouchEventManager mgr;
 *       mgr.addWidget(&widget);
 *
 *       mgr.handleTouch(30, 30, true);  // Touch inside bounds
 *       EXPECT_EQ(1, widget.onTouchCount());
 *
 *       mgr.handleTouch(30, 30, false);  // Release
 *       EXPECT_EQ(1, widget.onReleaseCount());
 *   }
 *
 * MP-63: Unit test infrastructure for TouchEventManager
 */
class MockWidget : public Widget {
public:
    MockWidget()
        : on_touch_count_(0),
          on_release_count_(0),
          last_touch_x_(0),
          last_touch_y_(0),
          last_release_x_(0),
          last_release_y_(0) {
    }

    /**
     * Draw method (no-op for testing).
     * Widget::draw() is pure virtual, so must be implemented.
     */
    void draw(Renderer& renderer) override {
        // No-op for testing
    }

    /**
     * Handle touch event.
     * Increments counter and records coordinates.
     */
    void onTouch(int16_t x, int16_t y) override {
        on_touch_count_++;
        last_touch_x_ = x;
        last_touch_y_ = y;
    }

    /**
     * Handle release event.
     * Increments counter and records coordinates.
     */
    void onRelease(int16_t x, int16_t y) override {
        on_release_count_++;
        last_release_x_ = x;
        last_release_y_ = y;
    }

    // Test accessors
    int onTouchCount() const { return on_touch_count_; }
    int onReleaseCount() const { return on_release_count_; }
    int16_t lastTouchX() const { return last_touch_x_; }
    int16_t lastTouchY() const { return last_touch_y_; }
    int16_t lastReleaseX() const { return last_release_x_; }
    int16_t lastReleaseY() const { return last_release_y_; }

    /**
     * Reset all counters and recorded state.
     * Call between tests to ensure clean state.
     */
    void reset() {
        on_touch_count_ = 0;
        on_release_count_ = 0;
        last_touch_x_ = 0;
        last_touch_y_ = 0;
        last_release_x_ = 0;
        last_release_y_ = 0;
    }

private:
    int on_touch_count_;
    int on_release_count_;
    int16_t last_touch_x_;
    int16_t last_touch_y_;
    int16_t last_release_x_;
    int16_t last_release_y_;
};

#endif // MOCK_WIDGET_H

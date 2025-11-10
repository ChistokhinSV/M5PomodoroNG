#include <gtest/gtest.h>
#include "../src/ui/TouchEventManager.h"
#include "mocks/MockWidget.h"

/**
 * Unit tests for TouchEventManager
 *
 * Tests automatic touch event routing to widgets with hit testing,
 * Z-order support, visibility/enabled checks, and release tracking.
 *
 * MP-63: Verify TouchEventManager routing logic
 */

// Test 1: Basic touch and release routing to single widget
TEST(TouchEventManagerTest, RoutesSingleTouch) {
    MockWidget widget;
    widget.setBounds(10, 10, 50, 50);  // 10x10 to 60x60

    TouchEventManager mgr;
    mgr.addWidget(&widget);

    // Touch inside bounds
    mgr.handleTouch(30, 30, true);  // pressed=true
    EXPECT_EQ(1, widget.onTouchCount());
    EXPECT_EQ(30, widget.lastTouchX());
    EXPECT_EQ(30, widget.lastTouchY());
    EXPECT_EQ(&widget, mgr.getActiveWidget());

    // Release
    mgr.handleTouch(30, 30, false);  // pressed=false
    EXPECT_EQ(1, widget.onReleaseCount());
    EXPECT_EQ(30, widget.lastReleaseX());
    EXPECT_EQ(30, widget.lastReleaseY());
    EXPECT_EQ(nullptr, mgr.getActiveWidget());  // Active widget cleared
}

// Test 2: Touch outside widget bounds - no event routed
TEST(TouchEventManagerTest, IgnoresTouchOutsideBounds) {
    MockWidget widget;
    widget.setBounds(10, 10, 50, 50);  // 10x10 to 60x60

    TouchEventManager mgr;
    mgr.addWidget(&widget);

    // Touch outside bounds
    mgr.handleTouch(100, 100, true);
    EXPECT_EQ(0, widget.onTouchCount());
    EXPECT_EQ(nullptr, mgr.getActiveWidget());

    // Release (no active widget, so no event routed)
    mgr.handleTouch(100, 100, false);
    EXPECT_EQ(0, widget.onReleaseCount());
}

// Test 3: Z-order - last added widget wins when overlapping
TEST(TouchEventManagerTest, RespectsZOrder) {
    MockWidget bottom, top;
    bottom.setBounds(0, 0, 100, 100);
    top.setBounds(20, 20, 60, 60);  // Overlaps bottom at (20,20)-(80,80)

    TouchEventManager mgr;
    mgr.addWidget(&bottom);  // Added first = bottom layer
    mgr.addWidget(&top);     // Added last = top layer

    // Touch in overlapping area (40, 40)
    mgr.handleTouch(40, 40, true);

    EXPECT_EQ(1, top.onTouchCount());      // Top widget gets event
    EXPECT_EQ(0, bottom.onTouchCount());   // Bottom widget ignored
    EXPECT_EQ(&top, mgr.getActiveWidget());
}

// Test 4: Invisible widgets skipped
TEST(TouchEventManagerTest, SkipsInvisibleWidgets) {
    MockWidget widget;
    widget.setBounds(10, 10, 50, 50);
    widget.setVisible(false);  // Make invisible

    TouchEventManager mgr;
    mgr.addWidget(&widget);

    // Touch inside bounds (but widget is invisible)
    mgr.handleTouch(30, 30, true);
    EXPECT_EQ(0, widget.onTouchCount());  // Widget skipped
    EXPECT_EQ(nullptr, mgr.getActiveWidget());
}

// Test 5: Disabled widgets skipped
TEST(TouchEventManagerTest, SkipsDisabledWidgets) {
    MockWidget widget;
    widget.setBounds(10, 10, 50, 50);
    widget.setEnabled(false);  // Make disabled

    TouchEventManager mgr;
    mgr.addWidget(&widget);

    // Touch inside bounds (but widget is disabled)
    mgr.handleTouch(30, 30, true);
    EXPECT_EQ(0, widget.onTouchCount());  // Widget skipped
    EXPECT_EQ(nullptr, mgr.getActiveWidget());
}

// Test 6: Only widget that received onTouch gets onRelease (drag support)
TEST(TouchEventManagerTest, OnlyReleasesTouchedWidget) {
    MockWidget widget1, widget2;
    widget1.setBounds(0, 0, 50, 50);
    widget2.setBounds(60, 0, 50, 50);

    TouchEventManager mgr;
    mgr.addWidget(&widget1);
    mgr.addWidget(&widget2);

    // Touch widget1
    mgr.handleTouch(25, 25, true);
    EXPECT_EQ(1, widget1.onTouchCount());
    EXPECT_EQ(0, widget2.onTouchCount());

    // Release at different location (widget2's bounds)
    // Widget1 should still get release (drag support)
    mgr.handleTouch(75, 25, false);

    EXPECT_EQ(1, widget1.onReleaseCount());  // widget1 gets release
    EXPECT_EQ(0, widget2.onReleaseCount());  // widget2 NOT released
    EXPECT_EQ(75, widget1.lastReleaseX());   // Release coords passed through
    EXPECT_EQ(25, widget1.lastReleaseY());
}

// Test 7: No crash when no widgets registered
TEST(TouchEventManagerTest, NoTouchWhenEmpty) {
    TouchEventManager mgr;

    // Touch with no widgets registered - should not crash
    mgr.handleTouch(100, 100, true);
    EXPECT_EQ(0u, mgr.getWidgetCount());
    EXPECT_EQ(nullptr, mgr.getActiveWidget());

    // Release with no widgets - should not crash
    mgr.handleTouch(100, 100, false);
    EXPECT_EQ(nullptr, mgr.getActiveWidget());
}

// Test 8: clearWidgets() resets active widget tracking
TEST(TouchEventManagerTest, ClearWidgetsResetsActiveWidget) {
    MockWidget widget;
    widget.setBounds(10, 10, 50, 50);

    TouchEventManager mgr;
    mgr.addWidget(&widget);

    // Touch widget (makes it active)
    mgr.handleTouch(30, 30, true);
    EXPECT_EQ(&widget, mgr.getActiveWidget());

    // Clear widgets
    mgr.clearWidgets();
    EXPECT_EQ(0u, mgr.getWidgetCount());
    EXPECT_EQ(nullptr, mgr.getActiveWidget());  // Active widget cleared

    // Release should be no-op (no active widget)
    mgr.handleTouch(30, 30, false);
    EXPECT_EQ(0, widget.onReleaseCount());  // No release sent
}

// Test 9: removeWidget() clears active widget if it was the removed widget
TEST(TouchEventManagerTest, RemoveWidgetClearsActiveWidget) {
    MockWidget widget;
    widget.setBounds(10, 10, 50, 50);

    TouchEventManager mgr;
    mgr.addWidget(&widget);

    // Touch widget (makes it active)
    mgr.handleTouch(30, 30, true);
    EXPECT_EQ(&widget, mgr.getActiveWidget());

    // Remove widget while active
    mgr.removeWidget(&widget);
    EXPECT_EQ(0u, mgr.getWidgetCount());
    EXPECT_EQ(nullptr, mgr.getActiveWidget());  // Active widget cleared

    // Release should be no-op (no active widget)
    mgr.handleTouch(30, 30, false);
    EXPECT_EQ(0, widget.onReleaseCount());  // No release sent
}

// Test 10: Multiple widgets, only first hit (top layer) gets event
TEST(TouchEventManagerTest, FirstHitWinsInZOrder) {
    MockWidget widget1, widget2, widget3;
    widget1.setBounds(0, 0, 100, 100);
    widget2.setBounds(0, 0, 100, 100);  // Same bounds as widget1
    widget3.setBounds(0, 0, 100, 100);  // Same bounds as widget1 & widget2

    TouchEventManager mgr;
    mgr.addWidget(&widget1);  // Bottom
    mgr.addWidget(&widget2);  // Middle
    mgr.addWidget(&widget3);  // Top

    // Touch at (50, 50) - all widgets overlap
    mgr.handleTouch(50, 50, true);

    EXPECT_EQ(0, widget1.onTouchCount());  // Bottom not touched
    EXPECT_EQ(0, widget2.onTouchCount());  // Middle not touched
    EXPECT_EQ(1, widget3.onTouchCount());  // Top touched (last added = top)
    EXPECT_EQ(&widget3, mgr.getActiveWidget());
}

// Test 11: Z-order with invisible top widget - next visible widget gets event
TEST(TouchEventManagerTest, ZOrderSkipsInvisibleTopWidget) {
    MockWidget bottom, top;
    bottom.setBounds(0, 0, 100, 100);
    top.setBounds(0, 0, 100, 100);  // Same bounds
    top.setVisible(false);  // Top widget invisible

    TouchEventManager mgr;
    mgr.addWidget(&bottom);
    mgr.addWidget(&top);

    // Touch at (50, 50) - top is invisible, so bottom should get event
    mgr.handleTouch(50, 50, true);

    EXPECT_EQ(1, bottom.onTouchCount());  // Bottom gets event
    EXPECT_EQ(0, top.onTouchCount());     // Top skipped (invisible)
    EXPECT_EQ(&bottom, mgr.getActiveWidget());
}

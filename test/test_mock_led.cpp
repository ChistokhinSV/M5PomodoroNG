/**
 * Example Unit Test: MockLEDController (MP-49)
 *
 * Demonstrates usage of MockLEDController for testing LED interactions
 * without actual M5Stack hardware.
 *
 * Test scenarios:
 * - LED color control and tracking
 * - Pattern and progress animations
 * - Brightness control
 * - State verification
 */

#include <gtest/gtest.h>
#include "mocks/MockLEDController.h"

/**
 * Test: MockLEDController tracks setAll() calls
 */
TEST(MockLEDControllerTest, TracksSetAll) {
    MockLEDController leds;

    // Set all LEDs to red
    auto red = ILEDController::Color::Red();
    leds.setAll(red);

    // Verify tracking
    EXPECT_EQ(1, leds.setAllCount());
    EXPECT_TRUE(MockLEDController::colorsEqual(red, leds.lastColor()));

    // Verify all pixels match
    for (uint8_t i = 0; i < ILEDController::LED_COUNT; i++) {
        EXPECT_TRUE(MockLEDController::colorsEqual(red, leds.getPixel(i)));
    }
}

/**
 * Test: MockLEDController tracks setPixel() calls
 */
TEST(MockLEDControllerTest, TracksSetPixel) {
    MockLEDController leds;

    // Set specific LED
    auto green = ILEDController::Color::Green();
    leds.setPixel(3, green);

    // Verify tracking
    EXPECT_EQ(1, leds.setPixelCount());
    EXPECT_EQ(3, leds.lastPixelIndex());
    EXPECT_TRUE(MockLEDController::colorsEqual(green, leds.lastPixelColor()));
    EXPECT_TRUE(MockLEDController::colorsEqual(green, leds.getPixel(3)));
}

/**
 * Test: MockLEDController handles clear()
 */
TEST(MockLEDControllerTest, HandlesClear) {
    MockLEDController leds;

    // Set LEDs to color, then clear
    leds.setAll(ILEDController::Color::Red());
    leds.clear();

    // Verify all LEDs are black
    EXPECT_EQ(1, leds.clearCount());
    auto black = ILEDController::Color::Black();
    for (uint8_t i = 0; i < ILEDController::LED_COUNT; i++) {
        EXPECT_TRUE(MockLEDController::colorsEqual(black, leds.getPixel(i)));
    }
}

/**
 * Test: MockLEDController tracks pattern setting
 */
TEST(MockLEDControllerTest, TracksPattern) {
    MockLEDController leds;

    // Set pulse pattern
    auto orange = ILEDController::Color::Orange();
    leds.setPattern(ILEDController::Pattern::PULSE, orange);

    // Verify tracking
    EXPECT_EQ(1, leds.setPatternCount());
    EXPECT_EQ(ILEDController::Pattern::PULSE, leds.currentPattern());
    EXPECT_TRUE(MockLEDController::colorsEqual(orange, leds.lastPatternColor()));
    EXPECT_TRUE(leds.wasPatternSet(ILEDController::Pattern::PULSE));
}

/**
 * Test: MockLEDController tracks pattern history
 */
TEST(MockLEDControllerTest, TracksPatternHistory) {
    MockLEDController leds;

    // Set sequence of patterns
    leds.setPattern(ILEDController::Pattern::SOLID, ILEDController::Color::White());
    leds.setPattern(ILEDController::Pattern::PULSE, ILEDController::Color::Red());
    leds.setPattern(ILEDController::Pattern::RAINBOW);

    // Verify history
    EXPECT_EQ(3, leds.setPatternCount());
    const auto& history = leds.patternHistory();
    ASSERT_EQ(3u, history.size());
    EXPECT_EQ(ILEDController::Pattern::SOLID, history[0]);
    EXPECT_EQ(ILEDController::Pattern::PULSE, history[1]);
    EXPECT_EQ(ILEDController::Pattern::RAINBOW, history[2]);
}

/**
 * Test: MockLEDController handles progress bar
 */
TEST(MockLEDControllerTest, HandlesProgress) {
    MockLEDController leds;

    // Set progress
    auto green = ILEDController::Color::Green();
    leds.setProgress(75, green);

    // Verify tracking
    EXPECT_EQ(1, leds.setProgressCount());
    EXPECT_EQ(75, leds.progressPercent());
    EXPECT_TRUE(MockLEDController::colorsEqual(green, leds.progressColor()));
}

/**
 * Test: MockLEDController handles brightness
 */
TEST(MockLEDControllerTest, HandlesBrightness) {
    MockLEDController leds;

    // Default brightness
    EXPECT_EQ(50, leds.getBrightness());  // Default: 50%

    // Set brightness
    leds.setBrightness(80);
    EXPECT_EQ(80, leds.getBrightness());
    EXPECT_EQ(1, leds.setBrightnessCount());

    // Brightness clamping (max 100%)
    leds.setBrightness(150);
    EXPECT_EQ(100, leds.getBrightness());
}

/**
 * Test: MockLEDController tracks show() calls
 */
TEST(MockLEDControllerTest, TracksShow) {
    MockLEDController leds;

    leds.show();
    leds.show();

    EXPECT_EQ(2, leds.showCount());
}

/**
 * Test: MockLEDController reset() clears all state
 */
TEST(MockLEDControllerTest, ResetClearsState) {
    MockLEDController leds;

    // Make some calls
    leds.setAll(ILEDController::Color::Red());
    leds.setPattern(ILEDController::Pattern::PULSE);
    leds.setBrightness(80);

    // Reset
    leds.reset();

    // Verify clean state
    EXPECT_EQ(0, leds.setAllCount());
    EXPECT_EQ(0, leds.setPatternCount());
    EXPECT_EQ(50, leds.getBrightness());  // Default brightness
    EXPECT_EQ(ILEDController::Pattern::OFF, leds.currentPattern());
    EXPECT_EQ(0u, leds.patternHistory().size());

    // All LEDs black
    auto black = ILEDController::Color::Black();
    for (uint8_t i = 0; i < ILEDController::LED_COUNT; i++) {
        EXPECT_TRUE(MockLEDController::colorsEqual(black, leds.getPixel(i)));
    }
}

/**
 * Test: Color equality helper
 */
TEST(MockLEDControllerTest, ColorEquality) {
    auto red1 = ILEDController::Color::Red();
    auto red2 = ILEDController::Color::Red();
    auto blue = ILEDController::Color::Blue();

    EXPECT_TRUE(MockLEDController::colorsEqual(red1, red2));
    EXPECT_FALSE(MockLEDController::colorsEqual(red1, blue));
}

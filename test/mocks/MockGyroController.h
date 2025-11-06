#ifndef MOCK_GYRO_CONTROLLER_H
#define MOCK_GYRO_CONTROLLER_H

#include "../../src/hardware/IGyroController.h"
#include <vector>

/**
 * Mock Gyro Controller for Unit Testing (MP-49)
 *
 * Provides fake implementation of IGyroController for testing without hardware.
 *
 * Features:
 * - Tracks all method calls for verification
 * - Simulates orientation and gestures
 * - Allows test injection of sensor data
 * - No actual MPU6886 hardware access
 *
 * Usage in Tests:
 *   MockGyroController gyro;
 *   gyro.setOrientation(IGyroController::Orientation::FACE_UP);
 *   gyro.simulateFlip();
 *
 *   ASSERT_TRUE(gyro.wasFlipped());
 *   ASSERT_EQ(IGyroController::Orientation::FACE_DOWN, gyro.getOrientation());
 */
class MockGyroController : public IGyroController {
public:
    MockGyroController() = default;

    // ========================================
    // IGyroController Interface Implementation
    // ========================================

    bool begin() override {
        begin_called_ = true;
        return begin_result_;
    }

    void update() override {
        update_count_++;
    }

    Orientation getOrientation() const override {
        return current_orientation_;
    }

    bool isFaceUp() const override {
        return current_orientation_ == Orientation::FACE_UP;
    }

    bool isFaceDown() const override {
        return current_orientation_ == Orientation::FACE_DOWN;
    }

    Gesture getLastGesture() const override {
        return last_gesture_;
    }

    bool wasFlipped() override {
        bool result = flip_flag_;
        flip_flag_ = false;  // Clear flag (non-const behavior)
        return result;
    }

    bool wasRotatedCW() override {
        bool result = rotate_cw_flag_;
        rotate_cw_flag_ = false;  // Clear flag (non-const behavior)
        return result;
    }

    bool wasRotatedCCW() override {
        bool result = rotate_ccw_flag_;
        rotate_ccw_flag_ = false;  // Clear flag (non-const behavior)
        return result;
    }

    AccelData getAccel() const override {
        return accel_;
    }

    GyroData getGyro() const override {
        return gyro_;
    }

    void calibrate() override {
        calibrated_ = true;
        calibrate_count_++;
    }

    bool isCalibrated() const override {
        return calibrated_;
    }

    void setFlipThreshold(float threshold) override {
        flip_threshold_ = threshold;
        set_flip_threshold_count_++;
    }

    void setRotateThreshold(float threshold) override {
        rotate_threshold_ = threshold;
        set_rotate_threshold_count_++;
    }

    // ========================================
    // Test Simulation Methods
    // ========================================

    /**
     * Set current orientation for testing
     */
    void setOrientation(Orientation orientation) {
        current_orientation_ = orientation;
    }

    /**
     * Simulate flip gesture
     */
    void simulateFlip() {
        flip_flag_ = true;
        last_gesture_ = Gesture::FLIP;
        // Toggle orientation
        if (current_orientation_ == Orientation::FACE_UP) {
            current_orientation_ = Orientation::FACE_DOWN;
        } else if (current_orientation_ == Orientation::FACE_DOWN) {
            current_orientation_ = Orientation::FACE_UP;
        }
    }

    /**
     * Simulate clockwise rotation
     */
    void simulateRotateCW() {
        rotate_cw_flag_ = true;
        last_gesture_ = Gesture::ROTATE_CW;
    }

    /**
     * Simulate counter-clockwise rotation
     */
    void simulateRotateCCW() {
        rotate_ccw_flag_ = true;
        last_gesture_ = Gesture::ROTATE_CCW;
    }

    /**
     * Set accelerometer data for testing
     */
    void setAccel(float x, float y, float z) {
        accel_ = {x, y, z};
    }

    /**
     * Set gyroscope data for testing
     */
    void setGyro(float x, float y, float z) {
        gyro_ = {x, y, z};
    }

    // ========================================
    // Test Inspection Methods
    // ========================================

    /**
     * Check if begin() was called
     */
    bool beginCalled() const { return begin_called_; }

    /**
     * Get number of update() calls
     */
    int updateCount() const { return update_count_; }

    /**
     * Get number of calibrate() calls
     */
    int calibrateCount() const { return calibrate_count_; }

    /**
     * Get number of setFlipThreshold() calls
     */
    int setFlipThresholdCount() const { return set_flip_threshold_count_; }

    /**
     * Get number of setRotateThreshold() calls
     */
    int setRotateThresholdCount() const { return set_rotate_threshold_count_; }

    /**
     * Get current flip threshold
     */
    float flipThreshold() const { return flip_threshold_; }

    /**
     * Get current rotate threshold
     */
    float rotateThreshold() const { return rotate_threshold_; }

    /**
     * Reset all state for next test
     */
    void reset() {
        begin_called_ = false;
        calibrated_ = false;
        current_orientation_ = Orientation::UNKNOWN;
        last_gesture_ = Gesture::NONE;
        flip_flag_ = false;
        rotate_cw_flag_ = false;
        rotate_ccw_flag_ = false;
        accel_ = {0, 0, 0};
        gyro_ = {0, 0, 0};
        flip_threshold_ = 0.7f;
        rotate_threshold_ = 100.0f;

        update_count_ = 0;
        calibrate_count_ = 0;
        set_flip_threshold_count_ = 0;
        set_rotate_threshold_count_ = 0;
    }

    /**
     * Configure begin() to return specific result
     */
    void setBeginResult(bool result) {
        begin_result_ = result;
    }

private:
    // State
    bool begin_called_ = false;
    bool begin_result_ = true;
    bool calibrated_ = false;
    Orientation current_orientation_ = Orientation::UNKNOWN;
    Gesture last_gesture_ = Gesture::NONE;

    // Gesture flags (mutable for was*() methods)
    mutable bool flip_flag_ = false;
    mutable bool rotate_cw_flag_ = false;
    mutable bool rotate_ccw_flag_ = false;

    // Sensor data
    AccelData accel_ = {0, 0, 0};
    GyroData gyro_ = {0, 0, 0};

    // Thresholds
    float flip_threshold_ = 0.7f;
    float rotate_threshold_ = 100.0f;

    // Call counters
    int update_count_ = 0;
    int calibrate_count_ = 0;
    int set_flip_threshold_count_ = 0;
    int set_rotate_threshold_count_ = 0;
};

#endif // MOCK_GYRO_CONTROLLER_H

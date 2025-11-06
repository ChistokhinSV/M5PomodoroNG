#ifndef GYRO_CONTROLLER_H
#define GYRO_CONTROLLER_H

#include "IGyroController.h"
#include <M5Unified.h>
#include <cstdint>

/**
 * Gyroscope/Accelerometer controller for MPU6886 (M5Stack Core2)
 *
 * Implements IGyroController interface for hardware abstraction (MP-49)
 *
 * Features:
 * - Rotation detection (face-up, face-down, left, right)
 * - Gesture detection (flip, rotate 90°)
 * - Accelerometer data smoothing
 * - Polling-based (MPU6886 INT not wired to GPIO on Core2)
 *
 * Coordinate system (device flat, screen up):
 * - X: Left (-) to Right (+)
 * - Y: Bottom (-) to Top (+)
 * - Z: Down (-) to Up (+) [gravity = -1.0g when flat]
 *
 * Gestures:
 * - FLIP: Z-axis inversion (face-up ↔ face-down)
 * - ROTATE_CW: 90° clockwise rotation
 * - ROTATE_CCW: 90° counter-clockwise rotation
 */
class GyroController : public IGyroController {
public:
    GyroController();

    // Initialization
    bool begin() override;

    // Must call every loop iteration (polling-based)
    void update() override;

    // Orientation detection
    Orientation getOrientation() const override { return current_orientation; }
    bool isFaceUp() const override { return current_orientation == Orientation::FACE_UP; }
    bool isFaceDown() const override { return current_orientation == Orientation::FACE_DOWN; }

    // Gesture detection
    Gesture getLastGesture() const override { return last_gesture; }
    bool wasFlipped() override;          // Check & clear flip flag (non-const)
    bool wasRotatedCW() override;        // Check & clear rotate CW flag (non-const)
    bool wasRotatedCCW() override;       // Check & clear rotate CCW flag (non-const)

    // Raw sensor data
    AccelData getAccel() const override { return accel; }
    GyroData getGyro() const override { return gyro; }

    // Calibration
    void calibrate() override;                 // Zero out gyro drift
    bool isCalibrated() const override { return calibrated; }

    // Sensitivity tuning
    void setFlipThreshold(float threshold) override { flip_threshold = threshold; }
    void setRotateThreshold(float threshold) override { rotate_threshold = threshold; }

    // Diagnostics
    void printStatus() const;

private:
    AccelData accel = {0, 0, 0};
    GyroData gyro = {0, 0, 0};

    Orientation current_orientation = Orientation::UNKNOWN;
    Orientation last_orientation = Orientation::UNKNOWN;

    Gesture last_gesture = Gesture::NONE;
    bool flip_flag = false;
    bool rotate_cw_flag = false;
    bool rotate_ccw_flag = false;

    bool calibrated = false;
    AccelData accel_offset = {0, 0, 0};
    GyroData gyro_offset = {0, 0, 0};

    // Gesture detection thresholds
    float flip_threshold = 0.7f;      // g-force for Z-axis flip
    float rotate_threshold = 100.0f;  // deg/s for rotation

    // Internal methods
    Orientation detectOrientation(const AccelData& data) const;
    Gesture detectGesture(const AccelData& accel_data, const GyroData& gyro_data);
    void applySmoothing(AccelData& data, float alpha = 0.8f);
};

#endif // GYRO_CONTROLLER_H

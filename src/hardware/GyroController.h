#ifndef GYRO_CONTROLLER_H
#define GYRO_CONTROLLER_H

#include <M5Unified.h>
#include <cstdint>

/**
 * Gyroscope/Accelerometer controller for MPU6886 (M5Stack Core2)
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
class GyroController {
public:
    enum class Orientation {
        UNKNOWN,
        FACE_UP,      // Screen facing up (normal use)
        FACE_DOWN,    // Screen facing down (sleep trigger)
        LEFT_SIDE,    // Device on left edge
        RIGHT_SIDE,   // Device on right edge
        TOP_SIDE,     // Device on top edge
        BOTTOM_SIDE   // Device on bottom edge
    };

    enum class Gesture {
        NONE,
        FLIP,          // Face-up ↔ face-down
        ROTATE_CW,     // 90° clockwise
        ROTATE_CCW,    // 90° counter-clockwise
        SHAKE          // Rapid movement (optional)
    };

    struct AccelData {
        float x;  // g-force (-2.0 to +2.0)
        float y;
        float z;
    };

    struct GyroData {
        float x;  // deg/s
        float y;
        float z;
    };

    GyroController();

    // Initialization
    bool begin();

    // Must call every loop iteration (polling-based)
    void update();

    // Orientation detection
    Orientation getOrientation() const { return current_orientation; }
    bool isFaceUp() const { return current_orientation == Orientation::FACE_UP; }
    bool isFaceDown() const { return current_orientation == Orientation::FACE_DOWN; }

    // Gesture detection
    Gesture getLastGesture() const { return last_gesture; }
    bool wasFlipped() const;          // Check & clear flip flag
    bool wasRotatedCW() const;        // Check & clear rotate CW flag
    bool wasRotatedCCW() const;       // Check & clear rotate CCW flag

    // Raw sensor data
    AccelData getAccel() const { return accel; }
    GyroData getGyro() const { return gyro; }

    // Calibration
    void calibrate();                 // Zero out gyro drift
    bool isCalibrated() const { return calibrated; }

    // Sensitivity tuning
    void setFlipThreshold(float threshold) { flip_threshold = threshold; }
    void setRotateThreshold(float threshold) { rotate_threshold = threshold; }

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

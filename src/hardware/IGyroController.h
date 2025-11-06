#ifndef I_GYRO_CONTROLLER_H
#define I_GYRO_CONTROLLER_H

#include <cstdint>

/**
 * Gyroscope/Accelerometer Controller Interface - Hardware Abstraction for Testing (MP-49)
 *
 * Provides abstract interface for gyro/accelerometer control, allowing:
 * - Unit testing with mock implementations
 * - Hardware-independent screen and gesture tests
 * - Dependency injection for better testability
 *
 * Usage:
 *   // Production code uses concrete GyroController
 *   IGyroController* gyro = new GyroController();
 *   gyro->update();
 *   if (gyro->wasFlipped()) {
 *       // Handle flip gesture
 *   }
 *
 *   // Test code uses MockGyroController
 *   MockGyroController mockGyro;
 *   mockGyro.simulateFlip();
 *   // ... verify wasFlipped() == true
 */
class IGyroController {
public:
    /**
     * Device orientation (6 possible orientations)
     */
    enum class Orientation {
        UNKNOWN,
        FACE_UP,      // Screen facing up (normal use)
        FACE_DOWN,    // Screen facing down (sleep trigger)
        LEFT_SIDE,    // Device on left edge
        RIGHT_SIDE,   // Device on right edge
        TOP_SIDE,     // Device on top edge
        BOTTOM_SIDE   // Device on bottom edge
    };

    /**
     * Detected gestures
     */
    enum class Gesture {
        NONE,
        FLIP,          // Face-up ↔ face-down
        ROTATE_CW,     // 90° clockwise
        ROTATE_CCW,    // 90° counter-clockwise
        SHAKE          // Rapid movement (optional)
    };

    /**
     * Accelerometer data (g-force)
     */
    struct AccelData {
        float x;  // g-force (-2.0 to +2.0)
        float y;
        float z;
    };

    /**
     * Gyroscope data (angular velocity)
     */
    struct GyroData {
        float x;  // deg/s
        float y;
        float z;
    };

    virtual ~IGyroController() = default;

    // ====================
    // Initialization
    // ====================

    /**
     * Initialize gyro/accelerometer hardware
     * @return true if successful, false on failure
     */
    virtual bool begin() = 0;

    // ====================
    // Update Loop
    // ====================

    /**
     * Update sensor state (must call every loop iteration)
     * Polls MPU6886 hardware and updates orientation/gestures
     */
    virtual void update() = 0;

    // ====================
    // Orientation Detection
    // ====================

    /**
     * Get current device orientation
     * @return Current orientation (FACE_UP, FACE_DOWN, etc.)
     */
    virtual Orientation getOrientation() const = 0;

    /**
     * Check if device is face-up (screen facing up)
     * @return true if face-up, false otherwise
     */
    virtual bool isFaceUp() const = 0;

    /**
     * Check if device is face-down (screen facing down)
     * @return true if face-down, false otherwise
     */
    virtual bool isFaceDown() const = 0;

    // ====================
    // Gesture Detection
    // ====================

    /**
     * Get last detected gesture
     * @return Last gesture (FLIP, ROTATE_CW, ROTATE_CCW, SHAKE, or NONE)
     */
    virtual Gesture getLastGesture() const = 0;

    /**
     * Check if flip gesture was detected since last call
     * Clears internal flip flag (non-const state modification)
     * @return true if flipped, false otherwise
     */
    virtual bool wasFlipped() = 0;

    /**
     * Check if clockwise rotation was detected since last call
     * Clears internal rotate CW flag (non-const state modification)
     * @return true if rotated CW, false otherwise
     */
    virtual bool wasRotatedCW() = 0;

    /**
     * Check if counter-clockwise rotation was detected since last call
     * Clears internal rotate CCW flag (non-const state modification)
     * @return true if rotated CCW, false otherwise
     */
    virtual bool wasRotatedCCW() = 0;

    // ====================
    // Raw Sensor Data
    // ====================

    /**
     * Get raw accelerometer data
     * @return Acceleration in g-force (x, y, z)
     */
    virtual AccelData getAccel() const = 0;

    /**
     * Get raw gyroscope data
     * @return Angular velocity in deg/s (x, y, z)
     */
    virtual GyroData getGyro() const = 0;

    // ====================
    // Calibration
    // ====================

    /**
     * Calibrate sensor (zero out gyro drift)
     * Should be called when device is stationary
     */
    virtual void calibrate() = 0;

    /**
     * Check if sensor has been calibrated
     * @return true if calibrated, false otherwise
     */
    virtual bool isCalibrated() const = 0;

    // ====================
    // Sensitivity Tuning
    // ====================

    /**
     * Set flip detection threshold
     * @param threshold Z-axis g-force threshold (default: 0.7)
     */
    virtual void setFlipThreshold(float threshold) = 0;

    /**
     * Set rotation detection threshold
     * @param threshold Angular velocity threshold in deg/s (default: 100.0)
     */
    virtual void setRotateThreshold(float threshold) = 0;
};

#endif // I_GYRO_CONTROLLER_H

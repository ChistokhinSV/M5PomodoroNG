#include "GyroController.h"
#include <Arduino.h>

GyroController::GyroController()
    : current_orientation(Orientation::UNKNOWN),
      last_orientation(Orientation::UNKNOWN),
      last_gesture(Gesture::NONE),
      calibrated(false) {
}

bool GyroController::begin() {
    // M5Unified already initialized IMU
    // Check if IMU is available
    if (!M5.Imu.isEnabled()) {
        Serial.println("[GyroController] ERROR: IMU not available");
        return false;
    }

    Serial.println("[GyroController] Initialized MPU6886");

    // Initial calibration
    calibrate();

    return true;
}

void GyroController::update() {
    // Read raw sensor data
    float ax, ay, az;
    float gx, gy, gz;

    M5.Imu.getAccel(&ax, &ay, &az);
    M5.Imu.getGyro(&gx, &gy, &gz);

    // Apply calibration offsets
    accel.x = ax - accel_offset.x;
    accel.y = ay - accel_offset.y;
    accel.z = az - accel_offset.z;

    gyro.x = gx - gyro_offset.x;
    gyro.y = gy - gyro_offset.y;
    gyro.z = gz - gyro_offset.z;

    // Apply smoothing to accelerometer (reduce noise)
    static AccelData smoothed_accel = {0, 0, 0};
    const float alpha = 0.8f;  // Smoothing factor
    smoothed_accel.x = alpha * smoothed_accel.x + (1 - alpha) * accel.x;
    smoothed_accel.y = alpha * smoothed_accel.y + (1 - alpha) * accel.y;
    smoothed_accel.z = alpha * smoothed_accel.z + (1 - alpha) * accel.z;

    // Detect orientation
    last_orientation = current_orientation;
    current_orientation = detectOrientation(smoothed_accel);

    // Detect gestures
    Gesture gesture = detectGesture(accel, gyro);
    if (gesture != Gesture::NONE) {
        last_gesture = gesture;

        switch (gesture) {
            case Gesture::FLIP:
                flip_flag = true;
                Serial.println("[GyroController] Gesture: FLIP");
                break;
            case Gesture::ROTATE_CW:
                rotate_cw_flag = true;
                Serial.println("[GyroController] Gesture: ROTATE_CW");
                break;
            case Gesture::ROTATE_CCW:
                rotate_ccw_flag = true;
                Serial.println("[GyroController] Gesture: ROTATE_CCW");
                break;
            default:
                break;
        }
    }
}

bool GyroController::wasFlipped() {
    bool result = flip_flag;
    flip_flag = false;  // Clear flag after reading (non-const behavior)
    return result;
}

bool GyroController::wasRotatedCW() {
    bool result = rotate_cw_flag;
    rotate_cw_flag = false;  // Clear flag after reading (non-const behavior)
    return result;
}

bool GyroController::wasRotatedCCW() {
    bool result = rotate_ccw_flag;
    rotate_ccw_flag = false;  // Clear flag after reading (non-const behavior)
    return result;
}

void GyroController::calibrate() {
    Serial.println("[GyroController] Calibrating... Keep device still");

    const int samples = 100;
    AccelData accel_sum = {0, 0, 0};
    GyroData gyro_sum = {0, 0, 0};

    for (int i = 0; i < samples; i++) {
        float ax, ay, az, gx, gy, gz;
        M5.Imu.getAccel(&ax, &ay, &az);
        M5.Imu.getGyro(&gx, &gy, &gz);

        accel_sum.x += ax;
        accel_sum.y += ay;
        accel_sum.z += az;

        gyro_sum.x += gx;
        gyro_sum.y += gy;
        gyro_sum.z += gz;

        delay(10);
    }

    // Average to get offset
    accel_offset.x = accel_sum.x / samples;
    accel_offset.y = accel_sum.y / samples;
    accel_offset.z = (accel_sum.z / samples) + 1.0f;  // Remove gravity (1g down)

    gyro_offset.x = gyro_sum.x / samples;
    gyro_offset.y = gyro_sum.y / samples;
    gyro_offset.z = gyro_sum.z / samples;

    calibrated = true;

    Serial.printf("[GyroController] Calibration complete\n");
    Serial.printf("  Accel offset: (%.3f, %.3f, %.3f)\n",
                  accel_offset.x, accel_offset.y, accel_offset.z);
    Serial.printf("  Gyro offset: (%.3f, %.3f, %.3f)\n",
                  gyro_offset.x, gyro_offset.y, gyro_offset.z);
}

void GyroController::printStatus() const {
    Serial.println("=== Gyro Status ===");
    Serial.printf("Orientation: ");
    switch (current_orientation) {
        case Orientation::FACE_UP: Serial.println("FACE_UP"); break;
        case Orientation::FACE_DOWN: Serial.println("FACE_DOWN"); break;
        case Orientation::LEFT_SIDE: Serial.println("LEFT_SIDE"); break;
        case Orientation::RIGHT_SIDE: Serial.println("RIGHT_SIDE"); break;
        case Orientation::TOP_SIDE: Serial.println("TOP_SIDE"); break;
        case Orientation::BOTTOM_SIDE: Serial.println("BOTTOM_SIDE"); break;
        default: Serial.println("UNKNOWN"); break;
    }

    Serial.printf("Accel: (%.2f, %.2f, %.2f) g\n", accel.x, accel.y, accel.z);
    Serial.printf("Gyro: (%.2f, %.2f, %.2f) deg/s\n", gyro.x, gyro.y, gyro.z);
    Serial.printf("Calibrated: %s\n", calibrated ? "YES" : "NO");

    if (last_gesture != Gesture::NONE) {
        Serial.printf("Last Gesture: ");
        switch (last_gesture) {
            case Gesture::FLIP: Serial.println("FLIP"); break;
            case Gesture::ROTATE_CW: Serial.println("ROTATE_CW"); break;
            case Gesture::ROTATE_CCW: Serial.println("ROTATE_CCW"); break;
            case Gesture::SHAKE: Serial.println("SHAKE"); break;
            default: break;
        }
    }

    Serial.println("===================");
}

// Private methods

GyroController::Orientation GyroController::detectOrientation(const AccelData& data) const {
    // Determine dominant axis (largest magnitude)
    float abs_x = abs(data.x);
    float abs_y = abs(data.y);
    float abs_z = abs(data.z);

    const float threshold = 0.5f;  // Minimum g-force to determine orientation

    // Z-axis dominant (face-up or face-down)
    if (abs_z > abs_x && abs_z > abs_y && abs_z > threshold) {
        return (data.z > 0) ? Orientation::FACE_UP : Orientation::FACE_DOWN;
    }

    // X-axis dominant (left or right side)
    if (abs_x > abs_y && abs_x > abs_z && abs_x > threshold) {
        return (data.x > 0) ? Orientation::RIGHT_SIDE : Orientation::LEFT_SIDE;
    }

    // Y-axis dominant (top or bottom edge)
    if (abs_y > abs_x && abs_y > abs_z && abs_y > threshold) {
        return (data.y > 0) ? Orientation::TOP_SIDE : Orientation::BOTTOM_SIDE;
    }

    return Orientation::UNKNOWN;
}

GyroController::Gesture GyroController::detectGesture(const AccelData& accel_data, const GyroData& gyro_data) {
    // Flip detection: Z-axis change (face-up ↔ face-down)
    if (last_orientation == Orientation::FACE_UP && current_orientation == Orientation::FACE_DOWN) {
        return Gesture::FLIP;
    }
    if (last_orientation == Orientation::FACE_DOWN && current_orientation == Orientation::FACE_UP) {
        return Gesture::FLIP;
    }

    // Rotation detection: Z-axis gyro (rotation around vertical axis)
    if (abs(gyro_data.z) > rotate_threshold) {
        return (gyro_data.z > 0) ? Gesture::ROTATE_CCW : Gesture::ROTATE_CW;
    }

    // Shake detection: High acceleration on any axis (optional, not used yet)
    float total_accel = sqrt(accel_data.x * accel_data.x +
                             accel_data.y * accel_data.y +
                             accel_data.z * accel_data.z);
    if (total_accel > 2.5f) {  // Threshold for shake
        return Gesture::SHAKE;
    }

    return Gesture::NONE;
}

void GyroController::applySmoothing(AccelData& data, float alpha) {
    static AccelData prev = {0, 0, 0};
    data.x = alpha * prev.x + (1 - alpha) * data.x;
    data.y = alpha * prev.y + (1 - alpha) * data.y;
    data.z = alpha * prev.z + (1 - alpha) * data.z;
    prev = data;
}

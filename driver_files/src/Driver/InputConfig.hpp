#pragma once

#include <Windows.h>
#include <Xinput.h>

#include <string>

namespace OpenVREmulatorDriver
{

// Configuration for the HMD device input mapping.
struct HMDInputConfig
{
    static constexpr float DefaultMouseSensitivity = 0.003f;  // radians per pixel
    static constexpr float DefaultLookSpeed = 1.5f;           // radians/sec for left-stick look

    float mouse_sensitivity = DefaultMouseSensitivity;  // radians per pixel
    float look_speed = DefaultLookSpeed;                // radians/sec for left-stick look
    float move_speed = 1.0f;                            // m/s for left-trigger movement
    int key_mouse_toggle = VK_SPACE;                    // toggle mouse look on/off
};

// Configuration for the left controller input mapping.
// Note: LT, LB, left stick and gamepad X/Y are consumed by the HMD and
// right-controller pose respectively and are not mapped as VR inputs.
struct KeyboardControllerConfig
{
    // Digital pose controls. Values are Windows virtual-key codes.
    int move_forward = 0;
    int move_back = 0;
    int move_left = 0;
    int move_right = 0;
    int move_up = 0;
    int move_down = 0;

    int rotate_left = 0;
    int rotate_right = 0;
    int rotate_up = 0;
    int rotate_down = 0;
    int rotate_roll_left = 0;
    int rotate_roll_right = 0;

    int joystick_left = 0;
    int joystick_right = 0;
    int joystick_up = 0;
    int joystick_down = 0;

    int reset_pose = VK_HOME;

    int a = 0;
    int b = 0;
    int x = 0;
    int y = 0;
    int trigger = 0;
    int grip = 0;
    int joystick_click = 0;
    int system = 0;
};

struct LeftControllerConfig
{
    WORD btn_joystick_click = XINPUT_GAMEPAD_LEFT_THUMB;
    KeyboardControllerConfig keyboard{
        .move_forward = 0x57,       // W
        .move_back = 0x53,          // S
        .move_left = 0x41,          // A
        .move_right = 0x44,         // D
        .move_up = 0x52,            // R
        .move_down = 0x46,          // F
        .rotate_left = 0x4A,        // J
        .rotate_right = 0x4C,       // L
        .rotate_up = 0x49,          // I
        .rotate_down = 0x4B,        // K
        .rotate_roll_left = 0x55,   // U
        .rotate_roll_right = 0x4F,  // O
        .joystick_left = VK_NUMPAD4,
        .joystick_right = VK_NUMPAD6,
        .joystick_up = VK_NUMPAD8,
        .joystick_down = VK_NUMPAD2,
        .reset_pose = VK_HOME,
        .a = 0x31,                  // 1
        .b = 0x32,                  // 2
        .x = 0x33,                  // 3
        .y = 0x34,                  // 4
        .trigger = 0x5A,            // Z
        .grip = 0x58,               // X
        .joystick_click = 0x43,     // C
        .system = 0x56              // V
    };
    // Note: BACK is reserved as the swap toggle and cannot be remapped in XInput mode.
};

// Configuration for the right controller input mapping.
struct RightControllerConfig
{
    static constexpr int DefaultKeyA = 'E';                      // VK_E (keyboard fallback for A button)
    static constexpr int DefaultKeyB = 'R';                      // VK_R (keyboard fallback for B button)
    static constexpr float DefaultTriggerClickThreshold = 0.75f;
    static constexpr float DefaultPoseMoveSpeed = 0.5f;  // m/s for keyboard/controller pose adjustment
    static constexpr float DefaultPoseRotateSpeed = 1.5f; // radians/sec for keyboard hand rotation

    int key_a = DefaultKeyA;  // E (legacy keyboard fallback for A button)
    int key_b = DefaultKeyB;  // R (legacy keyboard fallback for B button)

    KeyboardControllerConfig keyboard{
        .move_forward = 0x49,       // I
        .move_back = 0x4B,          // K
        .move_left = 0x4A,          // J
        .move_right = 0x4C,         // L
        .move_up = 0x55,            // U
        .move_down = 0x4F,          // O
        .rotate_left = 0x46,        // F
        .rotate_right = 0x48,       // H
        .rotate_up = 0x54,          // T
        .rotate_down = 0x47,        // G
        .rotate_roll_left = 0x52,   // R
        .rotate_roll_right = 0x59,  // Y
        .joystick_left = VK_NUMPAD4,
        .joystick_right = VK_NUMPAD6,
        .joystick_up = VK_NUMPAD8,
        .joystick_down = VK_NUMPAD2,
        .reset_pose = VK_HOME,
        .a = 0x31,                  // 1
        .b = 0x32,                  // 2
        .x = 0x33,                  // 3
        .y = 0x34,                  // 4
        .trigger = 0x51,            // Q
        .grip = 0x45,               // E
        .joystick_click = 0x43,     // C
        .system = 0x56               // V
    };

    WORD btn_a = XINPUT_GAMEPAD_A;
    WORD btn_b = XINPUT_GAMEPAD_B;
    WORD btn_grip = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    WORD btn_system = XINPUT_GAMEPAD_START;
    WORD btn_joystick_click = XINPUT_GAMEPAD_RIGHT_THUMB;

    float trigger_click_threshold = DefaultTriggerClickThreshold;
    float pose_move_speed = DefaultPoseMoveSpeed;  // m/s for keyboard/controller pose adjustment
    float pose_rotate_speed = DefaultPoseRotateSpeed;
};

// Top-level config loaded from resources/input_mapping.ini.
// Defaults match the original hard-coded behaviour.
struct InputConfig
{
    bool keyboard_only = false;
    HMDInputConfig hmd;
    LeftControllerConfig left_controller;
    RightControllerConfig right_controller;

    // Return a config with all defaults (no file needed).
    static InputConfig Defaults();

    // Parse an INI file at the given absolute path.
    // Missing keys fall back to struct defaults.
    static InputConfig LoadFromFile(const std::string &path);

    // Locate the driver root directory relative to this DLL and load
    // resources/input_mapping.ini from it.  Falls back to defaults if the
    // file is absent or unreadable.
    static InputConfig LoadFromDriverRoot();
};

}  // namespace OpenVREmulatorDriver

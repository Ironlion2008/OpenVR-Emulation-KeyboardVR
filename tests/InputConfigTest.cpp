#include <gtest/gtest.h>

#include <Driver/InputConfig.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using namespace OpenVREmulatorDriver;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

// Write a temporary INI file and return its path.
std::filesystem::path WriteTempIni(const std::string &content)
{
    auto path = std::filesystem::temp_directory_path() / "test_input_mapping.ini";
    std::ofstream f(path, std::ios::trunc);
    f << content;
    return path;
}

}  // namespace

// ---------------------------------------------------------------------------
// InputConfig::Defaults
// ---------------------------------------------------------------------------

TEST(InputConfigDefaults, HMDValues)
{
    auto cfg = InputConfig::Defaults();
    EXPECT_FLOAT_EQ(cfg.hmd.mouse_sensitivity, 0.003f);
    EXPECT_FLOAT_EQ(cfg.hmd.look_speed, 1.5f);
    EXPECT_FLOAT_EQ(cfg.hmd.move_speed, 1.0f);
    EXPECT_EQ(cfg.hmd.key_mouse_toggle, VK_SPACE);
}

TEST(InputConfigDefaults, LeftControllerValues)
{
    auto cfg = InputConfig::Defaults();
    EXPECT_EQ(cfg.left_controller.btn_joystick_click, static_cast<WORD>(XINPUT_GAMEPAD_LEFT_THUMB));
}

TEST(InputConfigDefaults, RightControllerValues)
{
    auto cfg = InputConfig::Defaults();
    EXPECT_EQ(cfg.right_controller.key_a, 0x45);  // E
    EXPECT_EQ(cfg.right_controller.key_b, 0x52);  // R
    EXPECT_EQ(cfg.right_controller.btn_a, static_cast<WORD>(XINPUT_GAMEPAD_A));
    EXPECT_EQ(cfg.right_controller.btn_b, static_cast<WORD>(XINPUT_GAMEPAD_B));
    EXPECT_EQ(cfg.right_controller.btn_grip, static_cast<WORD>(XINPUT_GAMEPAD_RIGHT_SHOULDER));
    EXPECT_EQ(cfg.right_controller.btn_system, static_cast<WORD>(XINPUT_GAMEPAD_START));
    EXPECT_EQ(cfg.right_controller.btn_joystick_click,
              static_cast<WORD>(XINPUT_GAMEPAD_RIGHT_THUMB));
    EXPECT_FLOAT_EQ(cfg.right_controller.trigger_click_threshold, 0.75f);
    EXPECT_FLOAT_EQ(cfg.right_controller.pose_move_speed, 0.5f);
}

// ---------------------------------------------------------------------------
// InputConfig::LoadFromFile — basic parsing
// ---------------------------------------------------------------------------

TEST(InputConfigLoadFromFile, EmptyFileReturnsDefaults)
{
    auto path = WriteTempIni("");
    auto cfg = InputConfig::LoadFromFile(path.string());
    auto def = InputConfig::Defaults();

    EXPECT_FLOAT_EQ(cfg.hmd.look_speed, def.hmd.look_speed);
    EXPECT_FLOAT_EQ(cfg.hmd.move_speed, def.hmd.move_speed);
    EXPECT_FLOAT_EQ(cfg.hmd.mouse_sensitivity, def.hmd.mouse_sensitivity);
}

TEST(InputConfigLoadFromFile, HMDFloatKeys)
{
    auto path = WriteTempIni("[hmd]\n"
                             "look_speed = 2.5\n"
                             "move_speed = 0.75\n"
                             "mouse_sensitivity = 0.001\n");
    auto cfg = InputConfig::LoadFromFile(path.string());
    EXPECT_FLOAT_EQ(cfg.hmd.look_speed, 2.5f);
    EXPECT_FLOAT_EQ(cfg.hmd.move_speed, 0.75f);
    EXPECT_FLOAT_EQ(cfg.hmd.mouse_sensitivity, 0.001f);
}

TEST(InputConfigLoadFromFile, XInputButtonByName)
{
    auto path = WriteTempIni("[right_controller]\n"
                             "btn_a = B\n"
                             "btn_grip = LEFT_SHOULDER\n");
    auto cfg = InputConfig::LoadFromFile(path.string());
    EXPECT_EQ(cfg.right_controller.btn_a, static_cast<WORD>(XINPUT_GAMEPAD_B));
    EXPECT_EQ(cfg.right_controller.btn_grip, static_cast<WORD>(XINPUT_GAMEPAD_LEFT_SHOULDER));
}

TEST(InputConfigLoadFromFile, XInputButtonCombo)
{
    auto path = WriteTempIni("[right_controller]\n"
                             "btn_a = A | B\n");
    auto cfg = InputConfig::LoadFromFile(path.string());
    EXPECT_EQ(cfg.right_controller.btn_a, static_cast<WORD>(XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B));
}

TEST(InputConfigLoadFromFile, KeyboardHexValue)
{
    auto path = WriteTempIni("[right_controller]\n"
                             "key_a = 0x47\n"  // G
    );
    auto cfg = InputConfig::LoadFromFile(path.string());
    EXPECT_EQ(cfg.right_controller.key_a, 0x47);
}

TEST(InputConfigLoadFromFile, KeyboardVKName)
{
    auto path = WriteTempIni("[hmd]\n"
                             "key_mouse_toggle = VK_RETURN\n");
    auto cfg = InputConfig::LoadFromFile(path.string());
    EXPECT_EQ(cfg.hmd.key_mouse_toggle, VK_RETURN);
}

TEST(InputConfigLoadFromFile, CommentsAreIgnored)
{
    auto path = WriteTempIni("; this whole line is a comment\n"
                             "[hmd]\n"
                             "look_speed = 3.0 ; inline comment\n"
                             "# hash comment\n");
    auto cfg = InputConfig::LoadFromFile(path.string());
    EXPECT_FLOAT_EQ(cfg.hmd.look_speed, 3.0f);
    // move_speed untouched
    EXPECT_FLOAT_EQ(cfg.hmd.move_speed, InputConfig::Defaults().hmd.move_speed);
}

TEST(InputConfigLoadFromFile, UnknownKeysAreIgnored)
{
    auto path = WriteTempIni("[hmd]\n"
                             "nonexistent_key = 99\n"
                             "look_speed = 1.0\n");
    // Should not throw or crash
    EXPECT_NO_THROW({
        auto cfg = InputConfig::LoadFromFile(path.string());
        EXPECT_FLOAT_EQ(cfg.hmd.look_speed, 1.0f);
    });
}

TEST(InputConfigLoadFromFile, MissingFileReturnsDefaults)
{
    auto cfg = InputConfig::LoadFromFile("C:/does/not/exist/input_mapping.ini");
    auto def = InputConfig::Defaults();
    EXPECT_FLOAT_EQ(cfg.hmd.look_speed, def.hmd.look_speed);
    EXPECT_FLOAT_EQ(cfg.hmd.move_speed, def.hmd.move_speed);
}

TEST(InputConfigLoadFromFile, TriggerClickThreshold)
{
    auto path = WriteTempIni("[right_controller]\n"
                             "trigger_click_threshold = 0.5\n");
    auto cfg = InputConfig::LoadFromFile(path.string());
    EXPECT_FLOAT_EQ(cfg.right_controller.trigger_click_threshold, 0.5f);
}

TEST(InputConfigLoadFromFile, LeftControllerJoystickClick)
{
    auto path = WriteTempIni("[left_controller]\n"
                             "btn_joystick_click = RIGHT_THUMB\n");
    auto cfg = InputConfig::LoadFromFile(path.string());
    EXPECT_EQ(cfg.left_controller.btn_joystick_click,
              static_cast<WORD>(XINPUT_GAMEPAD_RIGHT_THUMB));
}

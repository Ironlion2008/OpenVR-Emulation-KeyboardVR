#include "InputConfig.hpp"

#include <Windows.h>

#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace OpenVREmulatorDriver
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

// XInput button name -> mask
WORD XInputButtonFromName(const std::string &name)
{
    static const std::unordered_map<std::string, WORD> ButtonNameTable = {
        {"DPAD_UP", static_cast<WORD>(XINPUT_GAMEPAD_DPAD_UP)},
        {"DPAD_DOWN", static_cast<WORD>(XINPUT_GAMEPAD_DPAD_DOWN)},
        {"DPAD_LEFT", static_cast<WORD>(XINPUT_GAMEPAD_DPAD_LEFT)},
        {"DPAD_RIGHT", static_cast<WORD>(XINPUT_GAMEPAD_DPAD_RIGHT)},
        {"START", static_cast<WORD>(XINPUT_GAMEPAD_START)},
        {"BACK", static_cast<WORD>(XINPUT_GAMEPAD_BACK)},
        {"LEFT_THUMB", static_cast<WORD>(XINPUT_GAMEPAD_LEFT_THUMB)},
        {"RIGHT_THUMB", static_cast<WORD>(XINPUT_GAMEPAD_RIGHT_THUMB)},
        {"LEFT_SHOULDER", static_cast<WORD>(XINPUT_GAMEPAD_LEFT_SHOULDER)},
        {"RIGHT_SHOULDER", static_cast<WORD>(XINPUT_GAMEPAD_RIGHT_SHOULDER)},
        {"A", static_cast<WORD>(XINPUT_GAMEPAD_A)},
        {"B", static_cast<WORD>(XINPUT_GAMEPAD_B)},
        {"X", static_cast<WORD>(XINPUT_GAMEPAD_X)},
        {"Y", static_cast<WORD>(XINPUT_GAMEPAD_Y)},
    };
    auto it = ButtonNameTable.find(name);
    return it != ButtonNameTable.end() ? it->second : 0;
}

// Parse a pipe-separated combo like "A|B" or "X|Y" into an OR-ed bitmask.
WORD ParseXInputCombo(const std::string &value)
{
    WORD result = 0;
    std::istringstream ss(value);
    std::string token;
    while (std::getline(ss, token, '|'))
    {
        // trim whitespace
        auto first = token.find_first_not_of(" \t");
        auto last = token.find_last_not_of(" \t");
        if (first != std::string::npos)
        {
            token = token.substr(first, last - first + 1);
        }
        result |= XInputButtonFromName(token);
    }
    return result;
}

// Parse a keyboard key: named VK_* constant, 0x hex value, or decimal int.
int ParseKeyValue(const std::string &value)
{
    static const std::unordered_map<std::string, int> VkNameTable = {
        {"VK_SPACE", VK_SPACE}, {"VK_LEFT", VK_LEFT},       {"VK_RIGHT", VK_RIGHT},
        {"VK_UP", VK_UP},       {"VK_DOWN", VK_DOWN},       {"VK_RETURN", VK_RETURN},
        {"VK_SHIFT", VK_SHIFT}, {"VK_CONTROL", VK_CONTROL}, {"VK_MENU", VK_MENU},
        {"VK_TAB", VK_TAB},     {"VK_ESCAPE", VK_ESCAPE},
    };
    auto it = VkNameTable.find(value);
    if (it != VkNameTable.end())
    {
        return it->second;
    }

    try
    {
        std::size_t pos = 0;
        const int keyCode =
            static_cast<int>(std::stoul(value, &pos, 0));  // base 0: auto-detects 0x
        return keyCode;
    }
    catch (const std::exception &)
    {
        // Not a valid integer literal; return the unbound default.
        return 0;
    }
}

float ParseFloat(const std::string &value, float fallback)
{
    try
    {
        return std::stof(value);
    }
    catch (...)
    {
        return fallback;
    }
}

bool ParseBool(const std::string &value, bool fallback)
{
    if (value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "YES")
    {
        return true;
    }
    if (value == "0" || value == "false" || value == "FALSE" || value == "no" || value == "NO")
    {
        return false;
    }
    return fallback;
}

// Trim leading/trailing whitespace in-place.
void Trim(std::string &s)
{
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        s.clear();
        return;
    }
    s = s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

using IniMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

IniMap ParseIniFile(const std::string &path)
{
    IniMap result;
    std::ifstream file(path);
    if (!file.is_open())
    {
        return result;
    }

    std::string section;
    std::string line;
    while (std::getline(file, line))
    {
        // Strip inline comments (;  or  #)
        auto cpos = line.find_first_of(";#");
        if (cpos != std::string::npos)
        {
            line = line.substr(0, cpos);
        }
        Trim(line);
        if (line.empty())
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            section = line.substr(1, line.size() - 2);
            Trim(section);
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        Trim(key);
        Trim(val);
        if (!key.empty())
        {
            result[section][key] = val;
        }
    }
    return result;
}

// Walk up from this DLL's path three levels (dll -> win64 -> bin -> root).
std::string GetDriverRootPath()
{
    std::array<char, MAX_PATH> buf{};
    HMODULE hModule = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            &GetDriverRootPath),
        &hModule);
    GetModuleFileNameA(hModule, buf.data(), MAX_PATH);

    std::string path(buf.data());
    // Strip filename, then win64, then bin
    for (int i = 0; i < 3; ++i)
    {
        auto sep = path.find_last_of("\\/");
        if (sep != std::string::npos)
        {
            path = path.substr(0, sep);
        }
    }
    return path;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// InputConfig public API
// ---------------------------------------------------------------------------

InputConfig InputConfig::Defaults()
{
    return InputConfig{};
}

InputConfig InputConfig::LoadFromDriverRoot()
{
    return LoadFromFile(GetDriverRootPath() + "\\resources\\input_mapping.ini");
}

InputConfig InputConfig::LoadFromFile(const std::string &path)
{
    InputConfig cfg;
    auto ini = ParseIniFile(path);
    if (ini.empty())
    {
        return cfg;  // file absent or empty – use struct defaults
    }

    // Helper: return value for key in a section, or "" if missing.
    auto get = [&](const std::string &sec, const std::string &key) -> std::string {
        auto sit = ini.find(sec);
        if (sit == ini.end())
        {
            return {};
        }
        auto kit = sit->second.find(key);
        if (kit == sit->second.end())
        {
            return {};
        }
        return kit->second;
    };

    // ------------------------------------------------------------------
    // [input]
    // ------------------------------------------------------------------
    auto input = [&](const std::string &k) { return get("input", k); };
    if (!input("keyboard_only").empty())
    {
        cfg.keyboard_only = ParseBool(input("keyboard_only"), cfg.keyboard_only);
    }

    // Load the common keyboard mapping block into a controller config.
    auto loadKeyboard = [&](const std::string &section, KeyboardControllerConfig &k) {
        auto getKey = [&](const std::string &name) { return get(section, name); };
        auto load = [&](const std::string &name, int &dst) {
            auto value = getKey(name);
            if (!value.empty())
            {
                dst = ParseKeyValue(value);
            }
        };
        load("move_forward", k.move_forward);
        load("move_back", k.move_back);
        load("move_left", k.move_left);
        load("move_right", k.move_right);
        load("move_up", k.move_up);
        load("move_down", k.move_down);
        load("rotate_left", k.rotate_left);
        load("rotate_right", k.rotate_right);
        load("rotate_up", k.rotate_up);
        load("rotate_down", k.rotate_down);
        load("rotate_roll_left", k.rotate_roll_left);
        load("rotate_roll_right", k.rotate_roll_right);
        load("joystick_left", k.joystick_left);
        load("joystick_right", k.joystick_right);
        load("joystick_up", k.joystick_up);
        load("joystick_down", k.joystick_down);
        load("reset_pose", k.reset_pose);
        load("a", k.a);
        load("b", k.b);
        load("x", k.x);
        load("y", k.y);
        load("trigger", k.trigger);
        load("grip", k.grip);
        load("joystick_click", k.joystick_click);
        load("system", k.system);
    };

    // ------------------------------------------------------------------
    // [hmd]
    // ------------------------------------------------------------------
    auto hmd = [&](const std::string &k) { return get("hmd", k); };

    if (!hmd("mouse_sensitivity").empty())
    {
        cfg.hmd.mouse_sensitivity = ParseFloat(hmd("mouse_sensitivity"), cfg.hmd.mouse_sensitivity);
    }
    if (!hmd("look_speed").empty())
    {
        cfg.hmd.look_speed = ParseFloat(hmd("look_speed"), cfg.hmd.look_speed);
    }
    if (!hmd("move_speed").empty())
    {
        cfg.hmd.move_speed = ParseFloat(hmd("move_speed"), cfg.hmd.move_speed);
    }
    if (!hmd("key_mouse_toggle").empty())
    {
        cfg.hmd.key_mouse_toggle = ParseKeyValue(hmd("key_mouse_toggle"));
    }

    // ------------------------------------------------------------------
    // [left_controller]
    // ------------------------------------------------------------------
    auto lc = [&](const std::string &k) { return get("left_controller", k); };

    loadKeyboard("left_controller", cfg.left_controller.keyboard);

    if (!lc("btn_joystick_click").empty())
    {
        cfg.left_controller.btn_joystick_click = ParseXInputCombo(lc("btn_joystick_click"));
    }

    // ------------------------------------------------------------------
    // [right_controller]
    // ------------------------------------------------------------------
    auto rc = [&](const std::string &k) { return get("right_controller", k); };

    loadKeyboard("right_controller", cfg.right_controller.keyboard);

    if (!rc("key_a").empty())
    {
        cfg.right_controller.key_a = ParseKeyValue(rc("key_a"));
    }
    if (!rc("key_b").empty())
    {
        cfg.right_controller.key_b = ParseKeyValue(rc("key_b"));
    }
    if (!rc("btn_a").empty())
    {
        cfg.right_controller.btn_a = ParseXInputCombo(rc("btn_a"));
    }
    if (!rc("btn_b").empty())
    {
        cfg.right_controller.btn_b = ParseXInputCombo(rc("btn_b"));
    }
    if (!rc("btn_grip").empty())
    {
        cfg.right_controller.btn_grip = ParseXInputCombo(rc("btn_grip"));
    }
    if (!rc("btn_system").empty())
    {
        cfg.right_controller.btn_system = ParseXInputCombo(rc("btn_system"));
    }
    if (!rc("btn_joystick_click").empty())
    {
        cfg.right_controller.btn_joystick_click = ParseXInputCombo(rc("btn_joystick_click"));
    }
    if (!rc("trigger_click_threshold").empty())
    {
        cfg.right_controller.trigger_click_threshold =
            ParseFloat(rc("trigger_click_threshold"), cfg.right_controller.trigger_click_threshold);
    }
    if (!rc("pose_move_speed").empty())
    {
        cfg.right_controller.pose_move_speed =
            ParseFloat(rc("pose_move_speed"), cfg.right_controller.pose_move_speed);
    }
    if (!rc("pose_rotate_speed").empty())
    {
        cfg.right_controller.pose_rotate_speed =
            ParseFloat(rc("pose_rotate_speed"), cfg.right_controller.pose_rotate_speed);
    }

    return cfg;
}

}  // namespace OpenVREmulatorDriver

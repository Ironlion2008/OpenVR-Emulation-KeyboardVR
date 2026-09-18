# OpenVR-Emulation-KeyboardVR

A keyboard and mouse controlled OpenVR virtual-controller setup for Windows and SteamVR.

KeyboardVR is for VR development, testing, experimentation, and generally pretending you have controllers when you don't. The HMD can come from a real headset, a phone, or whatever VRto3D is doing this week.

> **Windows only.**
>
> The SteamVR/OpenVR runtime and the Windows input APIs used here are Windows-oriented. If you're on Linux, this is not the driver you're looking for. 👋

---

# KeyboardVR — First Version

This repo is the **first version** of KeyboardVR.

The goal is simple:

> **Use a normal keyboard and mouse to operate two virtual OpenVR controllers through SteamVR.**

The HMD and controllers are separate parts of the setup:

```text
                 HMD / Head Tracking
                        │
                Phone / VRto3D
                        │
                        ▼
                     SteamVR
                        │
          ┌─────────────┴─────────────┐
          │                           │
          ▼                           ▼
   Left Virtual Hand          Right Virtual Hand
          ▲                           ▲
          │                           │
       Keyboard                Keyboard + Mouse
Great for experimenting with VR apps when physical motion controllers are unavailable, on backorder, or currently being chewed by a pet.

The Most Important Rule
text
If it works, don't touch it.
This project follows that rule religiously. If your controllers work, congratulations — you're done. Go play something. Don't refactor it. Don't "clean it up." Don't add that one feature you saw on Reddit at 3am.

If it doesn't work, then yes, something must be touched. Carefully. With a backup. And preferably with coffee.

Base Project
KeyboardVR is based on the open-source OpenVR-Emulation-Driver.

Upstream:
https://github.com/omarekik/OpenVR-Emulation-Driver

The working codebase follows upstream revision:

text
e513a2feab6cf74cdbe83546b7e5f9546dd31199
The lineage:

text
Simple-OpenVR-Driver-Tutorial
            │
            ▼
OpenVR-Emulation-Driver by Nmzik
            │
            ▼
OpenVR-Emulation-Driver
            │
            ▼
OpenVR-Emulation-KeyboardVR  ← you are here
Original tutorial:
https://github.com/terminal29/Simple-OpenVR-Driver-Tutorial

KeyboardVR keeps the OpenVR/SteamVR driver foundation and focuses this version on keyboard and mouse controller input.

What KeyboardVR Adds
Mainly: keyboard and mouse doing controller things.

Keyboard-only controller mode
Ignore XInput entirely and use the configured keyboard/mouse mapping:

ini
[input]

keyboard_only = 1
Configurable keyboard controls
The layout lives in:

text
driver_files/driver/openvr-emulator/resources/input_mapping.ini
Change keys without recompiling. Revolutionary, I know.

Virtual controller pose controls
Keyboard inputs can nudge the virtual controllers around in position and rotation. Like puppeteering, but with a keyboard and fewer strings.

Keyboard joysticks
Groups of keys become virtual joystick directions.

Controller buttons
Mapped for:

A

B

X

Y

Trigger

Grip

System/Menu

Left joystick click

Right joystick click is intentionally unmapped. It's not a bug. It's a design choice.

Mouse input for the right controller
text
Left Mouse Button  → Right VR Trigger
Right Mouse Button → Right VR Grip
Trigger and grip without a physical controller. Beautiful.

How It Works
Keyboard/mouse inputs are read by the Windows-side driver input layer, turned into virtual OpenVR controller components, and exposed through SteamVR.

text
Keyboard / Mouse
       │
       ▼
Input configuration
       │
       ▼
Controller input layer
       │
       ▼
OpenVR virtual controller
       │
       ▼
SteamVR
       │
       ▼
VR application
The config loads from input_mapping.ini when the driver starts. That's it. That's the magic.

🎮 Controls
Default KeyboardVR First Version layout. Intentionally collision-free except for F10, which is shared as the reset key for both hands. Because sharing is caring.

LEFT CONTROLLER
Left Joystick
text
             R
             ▲
             │
        F ◄──┼──► G
             │
             ▼
             D
Key	Action
R	Joystick Up
D	Joystick Down
F	Joystick Left
G	Joystick Right
C	Joystick Click
Left Hand Position
text
             W
             ▲
             │
        A ◄──┼──► E
             │
             ▼
             Q

        T = Move Up
        B = Move Down
Key	Action
W	Move Forward
Q	Move Back
A	Move Left
E	Move Right
T	Move Up
B	Move Down
Left Hand Rotation
Key	Action
U	Rotate Left
O	Rotate Right
Y	Rotate Up
H	Rotate Down
9	Roll Left
0	Roll Right
Left Controller Buttons
Key	VR input
1	A
2	B
3	X
4	Y
Z	Trigger
X	Grip
V	System/Menu
C	Joystick Click
F10	Reset Pose
RIGHT CONTROLLER
Right Joystick
text
             I
             ▲
             │
        J ◄──┼──► L
             │
             ▼
             K
Key	Action
I	Joystick Up
K	Joystick Down
J	Joystick Left
L	Joystick Right
Right joystick click is intentionally unassigned. It's the "quiet one" of the layout.

Right Hand Position
Arrow keys do the heavy lifting:

text
             ↑
             │
      ←──────┼──────→
             │
             ↓
Key	Action
↑	Move Forward
↓	Move Back
←	Move Left
→	Move Right
]	Move Up
[	Move Down
Right Hand Rotation
Only the four primary Numpad directional keys are used:

text
             NUM 8
               ▲
               │
       NUM 4 ◄─┼─► NUM 6
               │
               ▼
             NUM 2
Key	Action
Numpad 8	Rotate Up
Numpad 2	Rotate Down
Numpad 4	Rotate Left
Numpad 6	Rotate Right
Intentionally unused:

text
Numpad 1
Numpad 3
Numpad 5
Numpad 7
Numpad 9
Right-hand roll is disabled in this first version. It'll be back. Probably.

Right Controller Buttons
Input	VR input
5	A
6	B
7	X
8	Y
Left Mouse Button	Trigger
Right Mouse Button	Grip
P	System/Menu
F10	Reset Pose
🖱️ Mouse Controls
Mouse buttons are reserved for the right virtual controller only. The left controller is a keyboard purist.

text
┌───────────────────────────────┐
│        RIGHT CONTROLLER       │
│                               │
│  LMB ─────────────── Trigger  │
│  RMB ─────────────── Grip     │
│                               │
└───────────────────────────────┘
text
LMB → RIGHT VR TRIGGER
RMB → RIGHT VR GRIP
🔒 Reserved Keys
Some inputs are deliberately unused.

Steam / SteamVR reserved keys
text
S
M
Steam likes these. We leave them alone. We've seen things.

Navigation keys
Not used by the default config:

text
Home
End
Insert
Delete
Page Up
Page Down
Unused Numpad keys
text
Numpad 1
Numpad 3
Numpad 5
Numpad 7
Numpad 9
Only Numpad 8/2/4/6 are used for right-hand rotation.

🔄 Reset Pose
text
F10 → Reset Pose
F10 resets both virtual hand poses together. One key, two hands, zero drama.

HMD / Camera
KeyboardVR doesn't require the HMD to come from the same input device. Phone-based setups can use a separate solution for head tracking.

Default mouse-look config:

ini
[hmd]

mouse_sensitivity = 0.003
key_mouse_toggle  = VK_SPACE
look_speed        = 1.5
text
SPACE → Toggle Mouse Look
Separate from the right-controller mouse-button mappings. Yes, the mouse does two jobs. It's a hardworking mouse.

Configuration
Main config file:

text
driver_files/
└── driver/
    └── openvr-emulator/
        └── resources/
            └── input_mapping.ini
Edit it to customize the layout.

After changing the INI:

text
1. Save input_mapping.ini
2. Close SteamVR completely
3. Start SteamVR again
The driver reads the config on startup. It does not do live reload. It is not psychic.

Keyboard-only Mode
Enable KeyboardVR mode:

ini
[input]

keyboard_only = 1
This uses the keyboard/mouse mapping instead of XInput.

Original XInput mode still works:

ini
keyboard_only = 0
Project Structure
text
driver_files/
│
├── driver/
│   └── openvr-emulator/
│       ├── driver.vrdrivermanifest
│       ├── driver_openvr-emulator.dll
│       └── resources/
│           └── input_mapping.ini
│
└── src/
    └── Driver/
Component	Purpose
VRDriver.cpp	Driver init and device registration
InputConfig.hpp/cpp	INI config and input mapping
HMDDevice.hpp/cpp	Virtual HMD
ControllerDevice.hpp/cpp	Virtual controller input, buttons, joysticks, pose
TrackerDevice.hpp	Generic tracker functionality
TrackingReferenceDevice.hpp	Tracking reference functionality
🛠️ Building
There's a Windows GitHub Actions workflow that:

Sets up the build environment

Grabs the OpenVR SDK

Configures the project

Compiles the driver

Packages the SteamVR driver

Great when you don't want to maintain a full C++/Visual Studio environment locally.

Traditional local build uses:

Python

Conan

CMake

Visual Studio 2022

MSVC

Windows SDK

See upstream for original local-dev docs.

📦 Installation
Drop the generated driver into:

text
C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\
Final structure:

text
SteamVR
└── drivers
    └── openvr-emulator
        ├── driver.vrdrivermanifest
        ├── driver_openvr-emulator.dll
        └── resources
            └── input_mapping.ini
Then:

text
1. Close SteamVR
2. Install/update the driver
3. Verify input_mapping.ini is present
4. Start SteamVR
5. Test the virtual controllers
If it works, stop reading and go play. Remember the rule.

🧪 Testing
KeyboardVR is for experimentation and testing.

Different VR apps may expose different controller bindings. If an app doesn't respond to a particular input, check the app's SteamVR/OpenVR bindings and the KeyboardVR config.

🐛 Troubleshooting
Keyboard controls do not work
Check:

ini
[input]

keyboard_only = 1
Then fully restart SteamVR. Not "close the window." Fully. Restart. SteamVR is thorough.

Also verify:

text
SteamVR\drivers\openvr-emulator\resources\input_mapping.ini
INI changes do not appear
The config is loaded at driver start. Close SteamVR completely and start it again. See above about SteamVR being thorough.

Right trigger does not work
Verify:

ini
[right_controller]

trigger = 0x01
This maps to the Windows left mouse button.

Right grip does not work
Verify:

ini
[right_controller]

grip = 0x02
This maps to the Windows right mouse button.

Steam keyboard conflicts
Default config leaves S and M unused on purpose. Other keys can be changed in input_mapping.ini if an app needs a different layout.

Design Goals
Simple rules. Keep it that way.

Easy controls
Inputs are grouped into recognizable keyboard areas.

Separate hands
Left and right use different key groups.

Avoid common Steam conflicts
S and M stay unused.

Keep Numpad simple
Only 8/2/4/6 for right-hand rotation. No Numpad archaeology required.

Mouse for right-hand interaction
text
LMB → Trigger
RMB → Grip
Editable configuration
The layout lives in an INI file. Change keys without recompiling.

If it works, don't touch it
The most important rule. If your setup works, walk away. Slowly. No sudden moves.

What This Fork Changes
text
Upstream OpenVR driver
        │
        ├── Existing SteamVR/OpenVR device framework
        │
        ▼
KeyboardVR input configuration
        │
        ├── Keyboard-only mode
        ├── Per-controller keyboard mappings
        ├── Virtual joystick controls
        ├── Virtual hand pose controls
        ├── Controller buttons
        └── Right-hand mouse trigger/grip
The layout stays in input_mapping.ini so it can change independently from the binary.

Scope of the First Version
KeyboardVR is an experimental virtual-controller solution for testing and experimentation with common PC input devices.

It does not try to reproduce every capability of physical VR controllers.

It does not replace:

Physical 6DOF controller tracking

Real controller ergonomics

Hardware haptics

Physical controller sensors

The complete behavior of a physical VR controller

Compatibility varies between SteamVR, OpenVR/OpenXR layers, and individual VR apps.

Frequently Unasked Questions
Q: Will you fix my specific edge case?
A: Probably not. It's a hobby project. But if you know what you're doing, PRs are welcome.

Q: Why doesn't feature X exist?
A: Because the first version doesn't need it, and the first version works.

Q: Should I refactor this?
A: See the Most Important Rule.

Q: Is this affiliated with Valve/Steam?
A: No. They have no idea we exist. Let's keep it that way.

Q: Will you add right-hand roll?
A: It's on the list. The list is long.

Credits and Attribution
Based on the open-source OpenVR-Emulation-Driver project.

OpenVR-Emulation-Driver
https://github.com/omarekik/OpenVR-Emulation-Driver

Nmzik
Contributor to the OpenVR-Emulation-Driver project lineage.

Terminal29
Original Simple-OpenVR-Driver-Tutorial:
https://github.com/terminal29/Simple-OpenVR-Driver-Tutorial

The upstream projects provide the foundation. This repo focuses on the keyboard/mouse virtual-controller configuration described above.

Disclaimer
KeyboardVR is an experimental project for development, testing, and VR input experimentation.

Not affiliated with or endorsed by Valve, Steam, or the developers of individual VR applications.

Application compatibility may vary. Results may vary. Your mileage may vary. The vibe is usually fine though.

License
Follows the upstream license.

MIT License — Copyright (c) 2020 Jacob Hilton (Terminal29), portions Copyright (c) 2026 omarekik

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, subject to the conditions of the MIT License.

The Software is provided "AS IS", without warranty of any kind.

And remember:

text
If it works, don't touch it.

If it doesn't work, read the README again.

If it still doesn't work, you're now qualified to touch it.

Good luck. Have fun. yaa bye 👋
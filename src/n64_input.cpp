#include "n64_input.h"

#include "pch.h"

unsigned int n64_input::n64ButtonsDown = 0;
unsigned int n64_input::n64ButtonsUp = 0;
unsigned int n64_input::n64ButtonsHeld = 0;
bool n64_input::n64LeftTriggerDown = false;
bool n64_input::n64LeftTriggerUp = false;
bool n64_input::n64RightTriggerDown = false;
bool n64_input::n64RightTriggerUp = false;

namespace
{
	joypad_buttons_t Pressed{};
	joypad_buttons_t Released{};
	joypad_buttons_t Held{};
	joypad_inputs_t Inputs{};
	bool LastLT = false;
	bool LastRT = false;
	constexpr int TriggerThreshold = 48;
}

void n64_input::Initialize()
{
	Clear();
}

void n64_input::ScanPads()
{
	joypad_poll();
	Pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
	Released = joypad_get_buttons_released(JOYPAD_PORT_1);
	Held = joypad_get_buttons_held(JOYPAD_PORT_1);
	Inputs = joypad_get_inputs(JOYPAD_PORT_1);

	n64ButtonsDown = Pressed.raw;
	n64ButtonsUp = Released.raw;
	n64ButtonsHeld = Held.raw;

	bool curLT = Inputs.analog_l >= TriggerThreshold;
	bool curRT = Inputs.analog_r >= TriggerThreshold;
	n64LeftTriggerDown = curLT && !LastLT;
	n64LeftTriggerUp = !curLT && LastLT;
	n64RightTriggerDown = curRT && !LastRT;
	n64RightTriggerUp = !curRT && LastRT;
	LastLT = curLT;
	LastRT = curRT;
}

void n64_input::Clear()
{
	n64ButtonsDown = 0;
	n64ButtonsUp = 0;
	n64ButtonsHeld = 0;
	n64LeftTriggerDown = false;
	n64LeftTriggerUp = false;
	n64RightTriggerDown = false;
	n64RightTriggerUp = false;
	LastLT = false;
	LastRT = false;
	Pressed.raw = 0;
	Released.raw = 0;
	Held.raw = 0;
}

bool n64_input::Exit()
{
	return Held.start && Held.b;
}

bool n64_input::Pause()
{
	return Pressed.start;
}

bool n64_input::NewGame()
{
	return Pressed.b && !Held.start;
}

bool n64_input::ToggleMusic()
{
	return Pressed.z;
}

bool n64_input::LaunchBallDown()
{
	return Pressed.a;
}

bool n64_input::LaunchBallUp()
{
	return Released.a;
}

bool n64_input::MoveLeftPaddleDown()
{
	return Pressed.l;
}

bool n64_input::MoveLeftPaddleUp()
{
	return Released.l;
}

bool n64_input::MoveRightPaddleDown()
{
	return Pressed.r;
}

bool n64_input::MoveRightPaddleUp()
{
	return Released.r;
}

bool n64_input::NudgeLeftDown()
{
	return Pressed.d_left;
}

bool n64_input::NudgeLeftUp()
{
	return Released.d_left;
}

bool n64_input::NudgeRightDown()
{
	return Pressed.d_right;
}

bool n64_input::NudgeRightUp()
{
	return Released.d_right;
}

bool n64_input::NudgeUpDown()
{
	return Pressed.d_up;
}

bool n64_input::NudgeUpUp()
{
	return Released.d_up;
}

bool n64_input::Button1()
{
	return Pressed.a;
}

bool n64_input::Button2()
{
	return Pressed.b;
}

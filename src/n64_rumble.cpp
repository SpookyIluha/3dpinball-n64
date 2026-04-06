#include "n64_rumble.h"

#include "pch.h"

namespace
{
	struct RumblePortState
	{
		int FramesLeft;
		bool Active;
		bool Supported;
	};

	RumblePortState PortStates[JOYPAD_PORT_COUNT]{};
	bool Initialized = false;
	int PulseCooldown = 0;

	bool QueryRumbleSupported(joypad_port_t port)
	{
		return joypad_is_connected(port) && joypad_get_rumble_supported(port);
	}

	void ApplyPortState(joypad_port_t port, RumblePortState& state)
	{
		if (state.FramesLeft > 0)
		{
			if (!state.Active)
			{
				joypad_set_rumble_active(port, true);
				state.Active = true;
			}
			state.FramesLeft--;
		}
		else if (state.Active)
		{
			joypad_set_rumble_active(port, false);
			state.Active = false;
		}
	}
}

void n64_rumble::Initialize()
{
	Initialized = true;
	PulseCooldown = 0;
	joypad_poll();

	int supportedCount = 0;
	for (int portIndex = 0; portIndex < JOYPAD_PORT_COUNT; portIndex++)
	{
		const auto port = static_cast<joypad_port_t>(portIndex);
		auto& state = PortStates[port];
		state.FramesLeft = 0;
		state.Active = false;
		state.Supported = QueryRumbleSupported(port);
		if (state.Supported)
			supportedCount++;
	}

	debugf("n64_rumble: initialized, supported_ports=%d\n", supportedCount);
}

void n64_rumble::Update()
{
	if (!Initialized)
		return;

	if (PulseCooldown > 0)
		PulseCooldown--;

	for (int portIndex = 0; portIndex < JOYPAD_PORT_COUNT; portIndex++)
	{
		const auto port = static_cast<joypad_port_t>(portIndex);
		auto& state = PortStates[port];
		const bool supportedNow = QueryRumbleSupported(port);
		if (!supportedNow)
		{
			state.FramesLeft = 0;
			if (state.Active)
			{
				joypad_set_rumble_active(port, false);
				state.Active = false;
			}
		}
		state.Supported = supportedNow;

		if (state.Supported)
		{
			ApplyPortState(port, state);
		}
	}
}

void n64_rumble::Shutdown()
{
	if (!Initialized)
		return;

	for (int portIndex = 0; portIndex < JOYPAD_PORT_COUNT; portIndex++)
	{
		const auto port = static_cast<joypad_port_t>(portIndex);
		auto& state = PortStates[port];
		if (state.Active)
			joypad_set_rumble_active(port, false);
		state = {};
	}

	PulseCooldown = 0;
	Initialized = false;
}

void n64_rumble::PulseFrames(int frames)
{
	if (!Initialized)
		return;

	if (frames < 1)
		frames = 1;
	if (frames > 6)
		frames = 6;

	// Coalesce very chatty impact sounds so the motor does not buzz constantly.
	if (PulseCooldown > 0)
		frames = std::min(frames, 2);
	else
		PulseCooldown = 1;

	for (int portIndex = 0; portIndex < JOYPAD_PORT_COUNT; portIndex++)
	{
		const auto port = static_cast<joypad_port_t>(portIndex);
		auto& state = PortStates[port];
		if (!state.Supported)
			continue;

		if (state.FramesLeft < frames)
			state.FramesLeft = frames;
	}
}

void n64_rumble::PulseFromSound(float durationSec)
{
	int frames = 1;
	if (durationSec >= 2.5f)
		frames = 4;
	else if (durationSec >= 1.0f)
		frames = 3;
	else if (durationSec >= 0.25f)
		frames = 2;

	PulseFrames(frames);
}
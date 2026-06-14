#pragma once

#include "REL/Relocation.h"

// Address Library IDs used by the plugin. All are resolved at runtime against
// the installed Address Library database, so they stay valid across game builds
// as long as the IDs themselves remain stable.
namespace RE
{
	namespace Offset
	{
		// BSPCGamepadDevice::Poll. Contains `mov byte ptr [rbx+8], 1`
		// (C6 43 08 01) which latches the active input device to the gamepad
		// when the stick is moved. The plugin clears the immediate of every
		// match in the first 0x800 bytes so the stick stops claiming the device.
		namespace BSPCGamepadDevice
		{
			constexpr REL::ID Poll{ 124384 };
		}

		// UserEvents::QLook — getter for the interned "Look" string the engine
		// compares user-event tags against to classify an event as a look event.
		namespace UserEvents
		{
			constexpr REL::ID QLook{ 74548 };
		}

		// PlayerControls::LookHandler vtable. Slot 1 is ShouldHandleEvent.
		namespace PlayerControls
		{
			namespace LookHandler
			{
				constexpr REL::ID Vtbl{ 433589 };
			}
		}
	}
}

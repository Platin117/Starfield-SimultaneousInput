// SimultaneousInput — allow mouse look and gamepad input at the same time.
//
// Starfield switches the whole input system between "mouse/keyboard" and
// "gamepad" mode and discards the inactive device. That makes it impossible to
// walk with an analog stick (which the game sees as a gamepad) while aiming with
// the mouse. This plugin keeps both alive:
//
//   1. LookHandler::ShouldHandleEvent (vtable slot 1) is wrapped so a "Look"
//      event from EITHER the mouse or a thumbstick is accepted, instead of only
//      the currently-active device's.
//   2. BSPCGamepadDevice::Poll is patched so moving the (left) stick no longer
//      latches the active input device to the gamepad. The game therefore stays
//      in mouse mode for look processing — mouse aiming keeps mouse sensitivity
//      and orientation — while the stick still drives analog movement.
//   3. While the mouse is driving look, the input-mode byte is held to
//      "mouse/keyboard" so a gamepad *button* press (which the Poll patch does
//      not cover) can no longer flip look processing into gamepad mode.
//
// Engine functions are resolved through the SFSE Address Library at runtime. The
// only build-specific detail is the input-mode object global (see below).

#include "RE/Offset.Ext.h"

#include "SFSE/SFSE.h"

#include "REL/Relocation.h"
#include "REX/LOG.h"
#include "REX/W32/KERNEL32.h"

#include "RE/B/BSInputEventUser.h"

#include <atomic>
#include <cstdint>
#include <exception>

using namespace std::string_view_literals;

#define DLLEXPORT __declspec(dllexport)
#define SFSEAPI   __cdecl

namespace Plugin
{
	inline constexpr auto NAME = "SimultaneousInput"sv;
	inline constexpr auto VERSION = REL::Version(1, 2, 1);
}

extern "C" DLLEXPORT constinit auto SFSEPlugin_Version = []() {
	SFSE::PluginVersionData v{};
	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.AuthorName("Parapets");  // original author; maintainers are credited in the README

	// Resolve everything through the Address Library at runtime and let SFSE
	// grant the load on any layout-compatible runtime the AL DB covers, rather
	// than pinning a single game version.
	v.UsesAddressLibrary(true);
	v.IsLayoutDependent(true);
	return v;
}();

namespace RE
{
	// Opaque to us — the shim only forwards it to the original implementation.
	namespace PlayerControls
	{
		class LookHandler;
	}
}

namespace
{
	using ShouldHandleEvent_t = bool (*)(RE::PlayerControls::LookHandler*, const RE::InputEvent*);
	using QLook_t = const char* const* (*)();
	using QUserEvent_t = const char* const* (*)(const RE::InputEvent*);

	ShouldHandleEvent_t g_origShouldHandleEvent = nullptr;
	QLook_t             g_qLook = nullptr;

	// Address of the global that holds the BSInputDeviceManager pointer. The
	// current input mode is the byte at [*g_modeObjectPtr + 0x60]: 0xFF = mouse/
	// keyboard, 0 = gamepad. The Poll patch keeps the stick from flipping it, but
	// a gamepad *button* still flips it to gamepad (set deep in a vfunc-driven
	// arbiter, no clean in-place patch). So while the mouse drives look we force
	// it back to mouse/keyboard. Raw RVA is build-specific (1.16.244).
	std::uintptr_t* g_modeObjectPtr = nullptr;
	inline constexpr std::uint8_t kModeMouseKeyboard = 0xFF;

	// True when `a_event` is a "Look" event. The engine classifies this by
	// comparing the event's user-event tag (vtable slot 2) against the interned
	// "Look" string returned by QLook(); interned strings compare by pointer
	// identity. Returns false (so the shim simply defers to the original) until
	// QLook has resolved.
	bool IsLookEvent(const RE::InputEvent* a_event)
	{
		if (!g_qLook || !a_event) {
			return false;
		}
		const auto vtbl = *reinterpret_cast<void* const* const*>(a_event);
		const auto getTag = reinterpret_cast<QUserEvent_t>(vtbl[2]);
		const auto tag = getTag(a_event);
		const auto look = g_qLook();
		return tag && look && *tag == *look;
	}

	// Wraps LookHandler::ShouldHandleEvent: accept a Look event from either the
	// mouse or a thumbstick; defer everything else (buttons, movement, cursor
	// moves) to the original so their normal handling is unchanged.
	bool ShouldHandleEvent_Shim(RE::PlayerControls::LookHandler* a_self, const RE::InputEvent* a_event)
	{
		if (IsLookEvent(a_event)) {
			switch (a_event->eventType) {
			case RE::InputEvent::EventType::kMouseMove:
				// The mouse is driving look: keep the game in mouse/keyboard mode so
				// the camera uses mouse sensitivity/orientation, even if a gamepad
				// button just flipped the active device to gamepad.
				if (g_modeObjectPtr) {
					const auto obj = *g_modeObjectPtr;
					if (obj) {
						*reinterpret_cast<std::uint8_t*>(obj + 0x60) = kModeMouseKeyboard;
					}
				}
				return true;
			case RE::InputEvent::EventType::kThumbstick:
				return true;
			default:
				break;
			}
		}
		return g_origShouldHandleEvent(a_self, a_event);
	}

	// Overwrite the immediate of `mov byte ptr [rbx+8], 1` (C6 43 08 01) with 0
	// at every match inside BSPCGamepadDevice::Poll. That store latches the
	// active input device to the gamepad when the stick crosses its activation
	// threshold; clearing it keeps the game in mouse mode for look processing
	// while the stick still drives analog movement.
	unsigned PatchGamepadDeviceLatch()
	{
		REL::Relocation<std::uintptr_t> poll(RE::Offset::BSPCGamepadDevice::Poll);
		auto* const base = reinterpret_cast<std::uint8_t*>(poll.address());

		unsigned patched = 0;
		for (std::size_t i = 0; i < 0x800; ++i) {
			if (base[i] == 0xC6 && base[i + 1] == 0x43 && base[i + 2] == 0x08 && base[i + 3] == 0x01) {
				std::uint32_t oldProtect = 0;
				REX::W32::VirtualProtect(base + i + 3, 1, 0x40 /* PAGE_EXECUTE_READWRITE */, &oldProtect);
				base[i + 3] = 0x00;
				REX::W32::VirtualProtect(base + i + 3, 1, oldProtect, &oldProtect);
				++patched;
			}
		}
		return patched;
	}
}

extern "C" DLLEXPORT bool SFSEAPI SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse, SFSE::InitInfo{ .trampoline = false });
	REX::INFO("{} v{} loaded", Plugin::NAME, Plugin::VERSION.string("."sv));

	try {
		REL::Relocation<QLook_t> qLook(RE::Offset::UserEvents::QLook);
		g_qLook = qLook.get();

		g_modeObjectPtr = reinterpret_cast<std::uintptr_t*>(
			REL::Relocation<std::uintptr_t>(REL::Offset(0x5fd9cd8)).address());

		REL::Relocation<std::uintptr_t> vtbl(RE::Offset::PlayerControls::LookHandler::Vtbl);
		g_origShouldHandleEvent = reinterpret_cast<ShouldHandleEvent_t>(
			vtbl.write_vfunc(1, &ShouldHandleEvent_Shim));

		const auto patched = PatchGamepadDeviceLatch();
		if (patched == 0) {
			REX::WARN("gamepad device latch pattern not found — mouse look may fight gamepad mode");
		}
		REX::INFO("ready: look shim installed, gamepad device latch cleared at {} site(s)", patched);
	} catch (const std::exception& e) {
		REX::ERROR("setup failed: {}", e.what());
	}

	return true;
}

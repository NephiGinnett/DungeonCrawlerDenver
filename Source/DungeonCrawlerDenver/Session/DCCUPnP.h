#pragma once

#include "CoreMinimal.h"

/**
 * Internet play with no accounts (2026-11-15, Zoe: package for itch.io "as long as that's a good option for
 * multiplayer"). When someone hosts, this asks the home router (UPnP, which most routers have on) to forward the
 * game's port to this PC, and asks it for the house's public address. That address becomes an "Internet" invite in
 * the party menu, so friends anywhere can join without Tailscale.
 *
 * It all happens on a background thread and talks only to the router on the local network: nothing is sent to any
 * outside service. When the router says no, or the internet provider shares one public address between many homes
 * (carrier-grade NAT, common on mobile and satellite), the menu says so and the Tailscale invite still works.
 */
namespace DCCUPnP
{
	enum class EState : uint8 { Idle, Working, Open, Failed };

	/** Start asking the router to open Port (UDP) to this PC. Safe to call again; it does nothing while working. */
	void Start(int32 Port);

	EState GetState();
	/** The public IPv4 address (host order) once the state is Open. */
	uint32 GetPublicIp();
	/** One line for the party menu: what happened, in plain words. */
	FString Describe();
	/** Bumped every time the state changes, so a menu knows to redraw. */
	int32 GetVersion();
	/** Close the port again (on quit). Quick, and does nothing if it was never opened. */
	void Stop();
}

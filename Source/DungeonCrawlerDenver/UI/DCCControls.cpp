#include "UI/DCCControls.h"

#include "Characters/DCCReactions.h"
#include "Misc/ConfigCacheIni.h"

namespace DCCControls
{
	namespace
	{
		FKey& Slot(uint8 Action)
		{
			static FKey Keys[Count];
			static bool bLoaded = false;
			if (!bLoaded)
			{
				bLoaded = true;
				for (uint8 a = 0; a < Count; ++a)
				{
					Keys[a] = Default(a);
					FString Saved;
					if (GConfig && GConfig->GetString(TEXT("DCCKeys"), *FString::Printf(TEXT("Action%d"), a), Saved, GGameUserSettingsIni))
					{
						Keys[a] = Saved.IsEmpty() ? FKey() : FKey(*Saved);
					}
				}
			}
			return Keys[FMath::Min<uint8>(Action, Count - 1)];
		}
	}

	const TCHAR* Name(uint8 Action)
	{
		switch (Action)
		{
		case Grapple: return TEXT("Grappling hook (on the belt now)");
		case Spray: return TEXT("Spray your tag from a truck");
		case Reactions: return TEXT("Reactions and moves menu");
		default: break;
		}
		static FString Names[8];
		const int32 n = Action - QuickReact1;
		if (n >= 0 && n < 8)
		{
			Names[n] = FString::Printf(TEXT("Quick reaction: %s"), DCCReactions::Info(uint8(n + 1)).Name);
			return *Names[n];
		}
		return TEXT("");
	}

	FKey Default(uint8 Action)
	{
		switch (Action)
		{
		case Grapple: return EKeys::G;
		case Spray: return EKeys::Z;
		case Reactions: return EKeys::B;
		default: return FKey(); // quick reactions start unbound
		}
	}

	FKey Get(uint8 Action) { return Slot(Action); }

	void Set(uint8 Action, const FKey& Key)
	{
		if (Action >= Count)
		{
			return;
		}
		Slot(Action) = Key;
		if (GConfig)
		{
			GConfig->SetString(TEXT("DCCKeys"), *FString::Printf(TEXT("Action%d"), Action), Key.IsValid() ? *Key.GetFName().ToString() : TEXT(""), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	FString TakenBy(const FKey& Key)
	{
		// The fixed controls a remappable key must not land on (on foot and in the truck).
		static const TMap<FKey, FString> Fixed = {
			{ EKeys::W, TEXT("moving") }, { EKeys::A, TEXT("moving") }, { EKeys::S, TEXT("moving") }, { EKeys::D, TEXT("moving") },
			{ EKeys::SpaceBar, TEXT("jumping") }, { EKeys::LeftShift, TEXT("sprinting") }, { EKeys::LeftControl, TEXT("flying down") },
			{ EKeys::E, TEXT("interacting") }, { EKeys::Tab, TEXT("the menu") }, { EKeys::Escape, TEXT("closing things") },
			{ EKeys::Enter, TEXT("chat") }, { EKeys::N, TEXT("saying no") }, { EKeys::Y, TEXT("saying yes") },
			{ EKeys::M, TEXT("mounting (dungeon runs)") }, { EKeys::P, TEXT("pausing (dungeon runs)") },
			{ EKeys::One, TEXT("the belt") }, { EKeys::Two, TEXT("the belt") }, { EKeys::Three, TEXT("the belt") },
			{ EKeys::Four, TEXT("the belt") }, { EKeys::Five, TEXT("the belt") }, { EKeys::Six, TEXT("the belt") },
			{ EKeys::Seven, TEXT("the belt") }, { EKeys::Eight, TEXT("the belt") }, { EKeys::Nine, TEXT("the belt") },
			// The truck's own keys.
			{ EKeys::Q, TEXT("the truck's tool") }, { EKeys::F, TEXT("the truck's next tool") }, { EKeys::R, TEXT("the truck window") },
			{ EKeys::T, TEXT("the truck radio") }, { EKeys::C, TEXT("the radio channel") }, { EKeys::X, TEXT("jumping out of the truck") },
			{ EKeys::F1, TEXT("the DM panel") }, { EKeys::F2, TEXT("the party menu") }, { EKeys::F3, TEXT("the gear bench") },
		};
		const FString* Found = Fixed.Find(Key);
		return Found ? *Found : FString();
	}

	uint8 BoundTo(const FKey& Key, uint8 Except)
	{
		for (uint8 a = 0; a < Count; ++a)
		{
			if (a != Except && Get(a) == Key)
			{
				return a;
			}
		}
		return Count;
	}
}

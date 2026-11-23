#include "UI/DCCDamageSkins.h"

namespace DCCDamageSkins
{
	const TArray<FSkin>& All()
	{
		static const TArray<FSkin> Skins = {
			{ TEXT("SkinInferno"), TEXT("Numbers: Inferno"), TEXT("Your damage comes out on fire. The fire is decorative. Mostly."),
				300, FLinearColor(1.0f, 0.62f, 0.1f), FLinearColor(1.0f, 0.12f, 0.02f), 1.1f, TEXT(""), TEXT("!"), EMotion::Flicker },
			{ TEXT("SkinFrostbite"), TEXT("Numbers: Frostbite"), TEXT("Cold, slow, and somehow smug about it."),
				300, FLinearColor(0.75f, 0.95f, 1.0f), FLinearColor(0.15f, 0.45f, 1.0f), 1.05f, TEXT("*"), TEXT("*"), EMotion::Drift },
			{ TEXT("SkinToxic"), TEXT("Numbers: Toxic Waste"), TEXT("Glows like the Rocky Flats gift shop. Do not lick."),
				250, FLinearColor(0.6f, 1.0f, 0.15f), FLinearColor(0.45f, 0.05f, 0.6f), 1.0f, TEXT(""), TEXT("~"), EMotion::Wobble },
			{ TEXT("SkinGold"), TEXT("Numbers: Gold Standard"), TEXT("Every hit a little invoice. Every invoice paid in full."),
				450, FLinearColor(1.0f, 0.85f, 0.3f), FLinearColor(0.45f, 0.25f, 0.02f), 1.15f, TEXT("$"), TEXT(""), EMotion::Pop },
			{ TEXT("SkinNeon"), TEXT("Numbers: Neon Mile"), TEXT("Pink, with a cyan ghost a frame behind. Very 1986."),
				350, FLinearColor(1.0f, 0.35f, 0.85f), FLinearColor(0.1f, 0.95f, 1.0f), 1.05f, TEXT(""), TEXT(""), EMotion::Rise },
			{ TEXT("SkinStatic"), TEXT("Numbers: Dead Air"), TEXT("The signal is weak. The damage is not."),
				350, FLinearColor(0.95f, 0.95f, 1.0f), FLinearColor(1.0f, 0.1f, 0.25f), 1.0f, TEXT(""), TEXT(""), EMotion::Glitch },
			{ TEXT("SkinRepo"), TEXT("Numbers: Past Due"), TEXT("Every number arrives stamped. The System considers this a reminder."),
				500, FLinearColor(1.0f, 0.18f, 0.15f), FLinearColor(0.2f, 0.02f, 0.02f), 1.2f, TEXT("PAST DUE "), TEXT(""), EMotion::Stamp },
		};
		return Skins;
	}

	const FSkin* Find(FName Id)
	{
		return Id.IsNone() ? nullptr : All().FindByPredicate([Id](const FSkin& Skin) { return Skin.Id == Id; });
	}

	FLinearColor PlayerColour(const FString& Name)
	{
		struct FPair { const TCHAR* Who; FLinearColor Colour; };
		static const FPair Roster[] = {
			{ TEXT("CJ"), FLinearColor(1.0f, 0.55f, 0.15f) },
			{ TEXT("Scott"), FLinearColor(0.35f, 0.65f, 1.0f) },
			{ TEXT("Marcel"), FLinearColor(0.35f, 1.0f, 0.5f) },
			{ TEXT("Vaughn"), FLinearColor(0.75f, 0.45f, 1.0f) },
			{ TEXT("Kyle"), FLinearColor(1.0f, 0.92f, 0.3f) },
			{ TEXT("Zoe"), FLinearColor(1.0f, 0.45f, 0.75f) },
		};
		for (const FPair& Pair : Roster)
		{
			if (Name.Contains(Pair.Who))
			{
				return Pair.Colour;
			}
		}
		// Anyone else: a bright colour from their name, the same every time.
		const float Hue = float(GetTypeHash(Name) % 360);
		return FLinearColor(Hue, 0.55f, 1.0f).HSVToLinearRGB();
	}
}

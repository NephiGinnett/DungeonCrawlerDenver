#include "Characters/DCCAttackStyle.h"

namespace DCCAttackStyle
{
	static const TCHAR* SlashPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_ChargedSlash.A_DCC_DW_ChargedSlash");
	static const TCHAR* PunchPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_BoxingWarmup.A_DCC_OA_BoxingWarmup");
	static const TCHAR* ShootPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_ArcheryShot.A_DCC_AO_ArcheryShot");

	EStyle FromId(FName ItemId)
	{
		const FString Id = ItemId.ToString().ToLower();
		if (Id.IsEmpty() || Id == TEXT("none")) { return Punch; }
		// Small overrides where the row name would lie.
		if (Id.Contains(TEXT("flaregun")) || Id.Contains(TEXT("nailgun")) || Id.Contains(TEXT("crossbow"))) { return Shoot; }
		if (Id.Contains(TEXT("knife")) || Id.Contains(TEXT("opener")) || Id.Contains(TEXT("dagger"))) { return Stab; }
		if (Id.Contains(TEXT("axe")) || Id.Contains(TEXT("sledge")) || Id.Contains(TEXT("hammer")) || Id.Contains(TEXT("mallet"))) { return Overhead; }
		if (Id.Contains(TEXT("sword")) || Id.Contains(TEXT("trimmer")) || Id.Contains(TEXT("machete"))) { return Slash; }
		if (Id.Contains(TEXT("gun")) || Id.Contains(TEXT("bow"))) { return Shoot; }
		if (Id.Contains(TEXT("club")) || Id.Contains(TEXT("bat")) || Id.Contains(TEXT("pan")) || Id.Contains(TEXT("pipe"))) { return Blunt; }
		if (Id.Contains(TEXT("throw")) || Id.Contains(TEXT("rock")) || Id.Contains(TEXT("bottle"))) { return Throw; }
		return Blunt;
	}

	const TCHAR* Name(EStyle Style)
	{
		switch (Style)
		{
		case Stab: return TEXT("Stab");
		case Slash: return TEXT("Slash");
		case Overhead: return TEXT("Overhead");
		case Blunt: return TEXT("Blunt");
		case Punch: return TEXT("Punch");
		case Shoot: return TEXT("Shoot");
		case Throw: return TEXT("Throw");
		default: return TEXT("Punch");
		}
	}

	void ClipFor(EStyle Style, FString& OutPath, float& OutRate, bool& bBorrowed)
	{
		bBorrowed = true;
		OutRate = 1.0f;
		switch (Style)
		{
		case Stab:
			OutPath = SlashPath;
			OutRate = 1.45f;
			break;
		case Slash:
		case Overhead:
		case Blunt:
			OutPath = SlashPath;
			break;
		case Shoot:
		case Throw:
			OutPath = ShootPath;
			break;
		case Punch:
		default:
			OutPath = PunchPath;
			break;
		}
	}
}

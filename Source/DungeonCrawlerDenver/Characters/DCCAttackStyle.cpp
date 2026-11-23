#include "Characters/DCCAttackStyle.h"

#include "Animation/AnimationAsset.h"

namespace DCCAttackStyle
{
	static const TCHAR* StabPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_Stab.A_DCC_Stab");
	static const TCHAR* SlashPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_Slash.A_DCC_Slash");
	static const TCHAR* OverheadPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_Overhead.A_DCC_Overhead");
	static const TCHAR* BluntPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_Blunt.A_DCC_Blunt");
	static const TCHAR* PunchPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_BoxingWarmup.A_DCC_OA_BoxingWarmup");
	static const TCHAR* ShootPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_ArcheryShot.A_DCC_AO_ArcheryShot");
	static const TCHAR* SNSIdle = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_SNS_Idle.A_DCC_SNS_Idle");
	static const TCHAR* SNSWalk = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_SNS_Walk.A_DCC_SNS_Walk");
	static const TCHAR* SNSRun = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_SNS_Run.A_DCC_SNS_Run");
	static const TCHAR* HitPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_Hit.A_DCC_Hit");
	static const TCHAR* CastPath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_Cast.A_DCC_Cast");
	static const TCHAR* WavePath = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_Wave.A_DCC_Wave");

	EStyle FromId(FName ItemId)
	{
		const FString Id = ItemId.ToString().ToLower();
		if (Id.IsEmpty() || Id == TEXT("none")) { return Punch; }
		auto Has = [&Id](std::initializer_list<const TCHAR*> Words)
		{
			for (const TCHAR* Word : Words) { if (Id.Contains(Word)) { return true; } }
			return false;
		};
		// 2026-11-22 (Zoe: "some one handed weapons used two handed swings"). Overhead is the Mixamo greatsword
		// attack and Blunt is a baseball swing, both two-handed. Only genuinely two-handed gear gets them now.
		// Every other melee weapon swings one-handed: Stab for points, Slash for blades, Chop for the rest.
		if (Has({ TEXT("flaregun"), TEXT("nailgun"), TEXT("crossbow"), TEXT("sodagun"), TEXT("rifle"), TEXT("shotgun"),
			TEXT("smg"), TEXT("sidearm"), TEXT("pistol"), TEXT("gun"), TEXT("bow") })) { return Shoot; }
		if (Id.Contains(TEXT("rockies"))) { return Blunt; } // the giveaway bat, not a rock
		if (Has({ TEXT("throw"), TEXT("rock"), TEXT("bottle"), TEXT("grenade"), TEXT("bomb"), TEXT("molotov"),
			TEXT("dynamite"), TEXT("pineapple"), TEXT("firework"), TEXT("napalm"), TEXT("charge"), TEXT("satchel") })) { return Throw; }
		// Two-handed.
		if (Has({ TEXT("sledge"), TEXT("thunderhammer"), TEXT("fireaxe"), TEXT("halberd"), TEXT("great"), TEXT("maul"),
			TEXT("longsword"), TEXT("pickaxe") })) { return Overhead; }
		if (Has({ TEXT("bat"), TEXT("lacrosse"), TEXT("shovel"), TEXT("broom"), TEXT("rake"), TEXT("bungee") })) { return Blunt; }
		// One-handed.
		if (Has({ TEXT("knife"), TEXT("opener"), TEXT("dagger"), TEXT("chisel"), TEXT("spear"), TEXT("switchblade"),
			TEXT("star"), TEXT("screwdriver") })) { return Stab; }
		if (Has({ TEXT("sword"), TEXT("blade"), TEXT("machete"), TEXT("trimmer"), TEXT("sickle"), TEXT("saw") })) { return Slash; }
		return Chop;
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
		case Chop: return TEXT("Chop");
		default: return TEXT("Punch");
		}
	}

	void ClipFor(EStyle Style, FString& OutPath, float& OutRate, bool& bBorrowed)
	{
		bBorrowed = false;
		OutRate = 1.0f;
		switch (Style)
		{
		case Stab:
			OutPath = StabPath;
			break;
		case Slash:
		case Chop:
			OutPath = SlashPath;
			break;
		case Overhead:
			OutPath = OverheadPath;
			break;
		case Blunt:
			OutPath = BluntPath;
			break;
		case Shoot:
		case Throw:
			bBorrowed = true;
			OutPath = ShootPath;
			break;
		case Punch:
		default:
			bBorrowed = true;
			OutPath = PunchPath;
			break;
		}
	}

	UAnimationAsset* FindClip(const TCHAR* Path)
	{
		// No static TObjectPtr cache. That map is invisible to GC, so after a
		// travel the old clip can be collected and the next lookup hands back
		// whatever reused the slot (a nameplate, the capsule). LoadObject is
		// already cheap once the package is in memory.
		return LoadObject<UAnimationAsset>(nullptr, Path);
	}

	void WeaponLoops(EStyle Style, UAnimationAsset*& Idle, UAnimationAsset*& Walk, UAnimationAsset*& Run)
	{
		Idle = Walk = Run = nullptr;
		// The Mixamo greatsword idle, walk, and run put the hands in a bad hold
		// on this body. Axes and hammers go back to the loops they used before
		// those clips existed. The overhead swing is still A_DCC_Overhead.
		if (Style != Slash)
		{
			return;
		}
		const TCHAR* IdlePath = SNSIdle;
		const TCHAR* WalkPath = SNSWalk;
		const TCHAR* RunPath = SNSRun;
		Idle = FindClip(IdlePath);
		Walk = FindClip(WalkPath);
		Run = FindClip(RunPath);
	}

	UAnimationAsset* HitClip() { return FindClip(HitPath); }
	UAnimationAsset* CastClip() { return FindClip(CastPath); }
	UAnimationAsset* WaveClip() { return FindClip(WavePath); }
}

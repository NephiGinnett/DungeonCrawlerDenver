#include "UI/DCCChatterTypes.h"

#include "Engine/Texture2D.h"

namespace
{
	// Not literal matches for what each token name describes - there is no
	// "debt stonks" or "smug repo man" icon in any pack this project has, and
	// chasing one would be the same trap as trying to license a real Twitch
	// emote. Picked for visual variety (different silhouettes/colours across
	// this RPGClassIcons pack's categories) the same way the invented emote
	// NAMES themselves are pure flavour, not literal references.
	const TMap<FString, FSoftObjectPath>& GetEmoteIconMap()
	{
		static const TMap<FString, FSoftObjectPath> Map = {
			{ TEXT(":BORANT_W:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C01_Assassination_Critical_Strikes/ROG_0080_Countering_Deadly_Precision.ROG_0080_Countering_Deadly_Precision")) },
			{ TEXT(":COLLAPSE_SOB:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/PSI_Psion_Mindweaver/C03_Mind_Control_Fear_Hypnosis/PSI_0224_Fracturing_Dominion_Gaze.PSI_0224_Fracturing_Dominion_Gaze")) },
			{ TEXT(":DEBT_STONKS:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C05_Tricks_Combo_Points_Utility/ROG_0483_Masterful_Distraction_Coin.ROG_0483_Masterful_Distraction_Coin")) },
			{ TEXT(":RATIO:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C03_Poisons_Bleeds_Venoms/ROG_0230_Lingering_Corrosive_Cut.ROG_0230_Lingering_Corrosive_Cut")) },
			{ TEXT(":FLOOR_KEKW:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C02_Stealth_Shadow_Ambush/ROG_0177_Lingering_Umbral_Decoy.ROG_0177_Lingering_Umbral_Decoy")) },
			{ TEXT(":SPONSOR_HEART:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C01_Assassination_Critical_Strikes/ROG_0003_Focused_Heartseeker.ROG_0003_Focused_Heartseeker")) },
			{ TEXT(":SYNDICATE_POG:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/PSI_Psion_Mindweaver/C01_Psychic_Blasts_Psionic_Energy/PSI_0079_Fracturing_Psychic_Nova.PSI_0079_Fracturing_Psychic_Nova")) },
			{ TEXT(":GG_CRAWLER:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C01_Assassination_Critical_Strikes/ROG_0075_Countering_Critical_Lunge.ROG_0075_Countering_Critical_Lunge")) },
			{ TEXT(":REPO_SMUG:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C02_Stealth_Shadow_Ambush/ROG_0176_Lingering_Cloak_Of_Silence.ROG_0176_Lingering_Cloak_Of_Silence")) },
			{ TEXT(":AUDIT_STARE:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/PSI_Psion_Mindweaver/C03_Mind_Control_Fear_Hypnosis/PSI_0209_Fracturing_Fear_Gaze.PSI_0209_Fracturing_Fear_Gaze")) },
			{ TEXT(":TIER_UP:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/PSI_Psion_Mindweaver/C01_Psychic_Blasts_Psionic_Energy/PSI_0099_Fracturing_Psionic_Nova.PSI_0099_Fracturing_Psionic_Nova")) },
			{ TEXT(":FORECLOSED_F:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C03_Poisons_Bleeds_Venoms/ROG_0228_Lingering_Bleeding_Needle.ROG_0228_Lingering_Bleeding_Needle")) },
			{ TEXT(":CAR_GO_ZOOM:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C02_Stealth_Shadow_Ambush/ROG_0172_Lingering_Shadowstep.ROG_0172_Lingering_Shadowstep")) },
			{ TEXT(":RING_W:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C01_Assassination_Critical_Strikes/ROG_0093_Ultimate_Heartseeker.ROG_0093_Ultimate_Heartseeker")) },
			{ TEXT(":CLEAN_PASS:"), FSoftObjectPath(TEXT("/Game/RPGClassIcons/ROG_Rogue/C05_Tricks_Combo_Points_Utility/ROG_0433_Quick_Distraction_Coin.ROG_0433_Quick_Distraction_Coin")) },
		};
		return Map;
	}
}

UTexture2D* DCCChatter::FindEmoteIcon(const FString& Token)
{
	const FSoftObjectPath* Found = GetEmoteIconMap().Find(Token);
	return Found ? Cast<UTexture2D>(Found->TryLoad()) : nullptr;
}

FText DCCChatter::SubstituteTokens(const FText& Template, const FString& Name,
	const FString& Item, const FString& Event)
{
	FString Result = Template.ToString();

	if (!Name.IsEmpty())
	{
		Result = Result.Replace(TEXT("{Name}"), *Name);
	}
	if (!Item.IsEmpty())
	{
		Result = Result.Replace(TEXT("{Item}"), *Item);
	}
	if (!Event.IsEmpty())
	{
		Result = Result.Replace(TEXT("{Event}"), *Event);
	}

	return FText::FromString(Result);
}

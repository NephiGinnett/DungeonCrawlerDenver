#include "Characters/DCCReactions.h"

#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ConfigCacheIni.h"

namespace DCCReactions
{
	namespace
	{
		// 10x10 glyphs. 'x' is the reaction's colour; everything else is the bubble behind it.
		const TCHAR* const Glyphs[Count][10] = {
			{ nullptr },
			{ // star
				TEXT("....x....."), TEXT("....x....."), TEXT("...xxx...."), TEXT("xxxxxxxxx."), TEXT(".xxxxxxx.."),
				TEXT("..xxxxx..."), TEXT("..xx.xx..."), TEXT(".xx...xx.."), TEXT(".x.....x.."), TEXT("..........") },
			{ // !
				TEXT("....xx...."), TEXT("....xx...."), TEXT("....xx...."), TEXT("....xx...."), TEXT("....xx...."),
				TEXT("....xx...."), TEXT(".........."), TEXT("....xx...."), TEXT("....xx...."), TEXT("..........") },
			{ // ?
				TEXT("...xxxx..."), TEXT("..xx..xx.."), TEXT("......xx.."), TEXT(".....xx..."), TEXT("....xx...."),
				TEXT("....xx...."), TEXT(".........."), TEXT("....xx...."), TEXT("....xx...."), TEXT("..........") },
			{ // heart
				TEXT(".........."), TEXT(".xx...xx.."), TEXT("xxxx.xxxx."), TEXT("xxxxxxxxx."), TEXT("xxxxxxxxx."),
				TEXT(".xxxxxxx.."), TEXT("..xxxxx..."), TEXT("...xxx...."), TEXT("....x....."), TEXT("..........") },
			{ // sweat drop
				TEXT("....x....."), TEXT("....x....."), TEXT("...xxx...."), TEXT("...xxx...."), TEXT("..xxxxx..."),
				TEXT("..xxxxx..."), TEXT(".xxxxxxx.."), TEXT(".xxxxxxx.."), TEXT("..xxxxx..."), TEXT("...xxx....") },
			{ // skull
				TEXT("..xxxxxx.."), TEXT(".xxxxxxxx."), TEXT(".x..xx..x."), TEXT(".x..xx..x."), TEXT(".xxxxxxxx."),
				TEXT("..xxxxxx.."), TEXT("..x.x.x..."), TEXT("..xxxxx..."), TEXT(".........."), TEXT("..........") },
			{ // note
				TEXT("...xxxxxx."), TEXT("...x....x."), TEXT("...x....x."), TEXT("...x....x."), TEXT("...x....x."),
				TEXT(".xxx..xxx."), TEXT("xxxx.xxxx."), TEXT(".xx...xx.."), TEXT(".........."), TEXT("..........") },
			{ // snowflake
				TEXT("....x....."), TEXT(".x..x..x.."), TEXT("..x.x.x..."), TEXT("...xxx...."), TEXT("xxxxxxxxx."),
				TEXT("...xxx...."), TEXT("..x.x.x..."), TEXT(".x..x..x.."), TEXT("....x....."), TEXT("..........") },
		};
	}

	const FInfo& Info(uint8 Reaction)
	{
		static const FInfo Table[Count] = {
			{ TEXT(""), FLinearColor::White, nullptr, 0.0f },
			{ TEXT("Cheer"), FLinearColor(1.0f, 0.78f, 0.15f), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_SpinJump.A_DCC_AO_SpinJump"), 0.0f },
			{ TEXT("Alert"), FLinearColor(0.95f, 0.18f, 0.2f), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_Alert.A_DCC_OA_Alert"), 0.9f },
			{ TEXT("Confused"), FLinearColor(0.35f, 0.6f, 1.0f), nullptr, 0.0f },
			{ TEXT("Love"), FLinearColor(1.0f, 0.3f, 0.55f), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_AgreeGesture.A_DCC_HW_AgreeGesture"), 0.0f },
			{ TEXT("Tired"), FLinearColor(0.35f, 0.75f, 1.0f), nullptr, 0.0f },
			{ TEXT("Down"), FLinearColor(0.2f, 0.2f, 0.24f), nullptr, 0.0f },
			{ TEXT("Party"), FLinearColor(0.8f, 0.35f, 1.0f), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_BoomDance.A_DCC_HW_BoomDance"), 0.0f },
			{ TEXT("Cold"), FLinearColor(0.55f, 0.9f, 1.0f), nullptr, 0.0f },
		};
		return Table[Reaction < Count ? Reaction : 0];
	}

	UTexture2D* Icon(uint8 Reaction)
	{
		if (Reaction == None || Reaction >= Count)
		{
			return nullptr;
		}
		static TStrongObjectPtr<UTexture2D> Cache[Count];
		if (Cache[Reaction].IsValid())
		{
			return Cache[Reaction].Get();
		}
		constexpr int32 S = 32;
		UTexture2D* Texture = UTexture2D::CreateTransient(S, S, PF_B8G8R8A8);
		if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
		{
			return nullptr;
		}
		Texture->Filter = TF_Nearest;
		Texture->SRGB = true;
		const FColor Ink = Info(Reaction).Colour.ToFColor(true);
		const FColor Fill(250, 248, 240, 255), Line(24, 20, 34, 255), Clear(0, 0, 0, 0);
		TArray<FColor> Pixels;
		Pixels.Init(Clear, S * S);
		// The bubble: a disc with a dark rim, and a little tail at the bottom pointing down at the head.
		const FVector2D C(15.5f, 13.5f);
		for (int32 Y = 0; Y < S; ++Y)
		{
			for (int32 X = 0; X < S; ++X)
			{
				const float D = FVector2D::Distance(FVector2D(X, Y), C);
				const bool bTail = Y >= 24 && Y <= 29 && FMath::Abs(X - 15.5f) <= (29 - Y) * 0.6f;
				if (D <= 12.5f || bTail)
				{
					Pixels[Y * S + X] = Fill;
				}
				if ((D > 12.5f && D <= 14.2f && Y < 26) || (bTail && FMath::Abs(X - 15.5f) > (29 - Y) * 0.6f - 1.2f))
				{
					Pixels[Y * S + X] = Line;
				}
			}
		}
		// The glyph, doubled, in the middle of the disc.
		for (int32 GY = 0; GY < 10; ++GY)
		{
			const TCHAR* Row = Glyphs[Reaction][GY];
			for (int32 GX = 0; Row && GX < 10 && Row[GX]; ++GX)
			{
				if (Row[GX] != TEXT('x'))
				{
					continue;
				}
				for (int32 dy = 0; dy < 2; ++dy)
				{
					for (int32 dx = 0; dx < 2; ++dx)
					{
						const int32 X = 6 + GX * 2 + dx, Y = 4 + GY * 2 + dy;
						Pixels[Y * S + X] = Ink;
					}
				}
			}
		}
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Pixels.GetData(), S * S * sizeof(FColor));
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		Cache[Reaction].Reset(Texture);
		return Texture;
	}

	const TCHAR* MomentName(uint8 Moment)
	{
		static const TCHAR* Names[MomentCount] = {
			TEXT("A kill"), TEXT("A big win"), TEXT("Spotted"), TEXT("Revived"), TEXT("Hurt or winded"), TEXT("Going down"),
			TEXT("A tag sprayed"), TEXT("Cold up high"),
		};
		return Moment < MomentCount ? Names[Moment] : TEXT("");
	}

	const TArray<FMove>& Moves()
	{
		static const TArray<FMove> List = {
			{ TEXT("No move"), nullptr },
			{ TEXT("Spin Jump"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_SpinJump.A_DCC_AO_SpinJump") },
			{ TEXT("Startled"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_Alert.A_DCC_OA_Alert") },
			{ TEXT("Nod"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_AgreeGesture.A_DCC_HW_AgreeGesture") },
			{ TEXT("Big Nod"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AgreeGesture.A_DCC_AO_AgreeGesture") },
			{ TEXT("Boom Dance"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_BoomDance.A_DCC_HW_BoomDance") },
			{ TEXT("All-Night Dance"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AllNightDance.A_DCC_AO_AllNightDance") },
			{ TEXT("Cardio Dance"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_CardioDance.A_DCC_DW_CardioDance") },
			{ TEXT("Air Squats"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AirSquat.A_DCC_AO_AirSquat") },
			{ TEXT("Boxing Warm-up"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_BoxingWarmup.A_DCC_OA_BoxingWarmup") },
			{ TEXT("Call Out"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_CallGesture.A_DCC_DW_CallGesture") },
			{ TEXT("Axe Stance"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_AxeStance.A_DCC_OA_AxeStance") },
			{ TEXT("Charged Slash"), TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_ChargedSlash.A_DCC_DW_ChargedSlash") },
		};
		return List;
	}

	FChoice DefaultFor(uint8 Moment)
	{
		switch (Moment)
		{
		case MomentKill:    return { Cheer, 1 };
		case MomentWin:     return { Cheer, 1 };
		case MomentSpotted: return { Alert, 2 };
		case MomentRevived: return { Love, 3 };
		case MomentHurt:    return { Tired, 0 };
		case MomentDowned:  return { Down, 0 };
		case MomentParty:   return { Party, 0 };
		case MomentCold:    return { Cold, 0 };
		default:            return { None, 0 };
		}
	}

	uint8 DefaultMoveFor(uint8 Reaction)
	{
		switch (Reaction)
		{
		case Cheer: return 1;
		case Alert: return 2;
		case Love:  return 3;
		case Party: return 5;
		default:    return 0;
		}
	}

	TArray<uint8> LoadChoices()
	{
		TArray<uint8> Out;
		for (uint8 m = 0; m < MomentCount; ++m)
		{
			const FChoice Default = DefaultFor(m);
			int32 Bubble = Default.Bubble, Move = Default.Move;
			if (GConfig)
			{
				GConfig->GetInt(TEXT("DCCReactions"), *FString::Printf(TEXT("Moment%dBubble"), m), Bubble, GGameUserSettingsIni);
				GConfig->GetInt(TEXT("DCCReactions"), *FString::Printf(TEXT("Moment%dMove"), m), Move, GGameUserSettingsIni);
			}
			Out.Add(uint8(FMath::Clamp(Bubble, 0, Count - 1)));
			Out.Add(uint8(FMath::Clamp(Move, 0, Moves().Num() - 1)));
		}
		return Out;
	}

	void SaveChoices(const TArray<uint8>& Choices)
	{
		if (!GConfig || Choices.Num() != MomentCount * 2)
		{
			return;
		}
		for (uint8 m = 0; m < MomentCount; ++m)
		{
			GConfig->SetInt(TEXT("DCCReactions"), *FString::Printf(TEXT("Moment%dBubble"), m), Choices[m * 2], GGameUserSettingsIni);
			GConfig->SetInt(TEXT("DCCReactions"), *FString::Printf(TEXT("Moment%dMove"), m), Choices[m * 2 + 1], GGameUserSettingsIni);
		}
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

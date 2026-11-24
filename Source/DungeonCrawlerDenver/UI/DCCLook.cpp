#include "UI/DCCLook.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace DCCLook
{
	namespace
	{
		const FLook Looks[] = {
			{ TEXT("SYSTEM GLASS"), nullptr, nullptr, 1.0f,
				FLinearColor(0.02f, 0.035f, 0.07f, 0.94f), FLinearColor(0.05f, 0.08f, 0.14f, 0.9f), FLinearColor(0.07f, 0.11f, 0.18f, 1.0f),
				FLinearColor(0.35f, 0.9f, 1.0f), FLinearColor(1.0f, 0.82f, 0.4f), FLinearColor(0.94f, 0.97f, 1.0f), FLinearColor(0.6f, 0.68f, 0.8f), 8.0f, false },
			{ TEXT("NEON ARCADE"), TEXT("Oxanium-Variable.ttf"), TEXT("ChakraPetch-Medium.ttf"), 1.0f,
				FLinearColor(0.05f, 0.01f, 0.09f, 0.95f), FLinearColor(0.1f, 0.03f, 0.16f, 0.92f), FLinearColor(0.14f, 0.04f, 0.22f, 1.0f),
				FLinearColor(1.0f, 0.3f, 0.78f), FLinearColor(0.25f, 0.95f, 1.0f), FLinearColor(1.0f, 0.94f, 1.0f), FLinearColor(0.72f, 0.58f, 0.86f), 3.0f, false },
			{ TEXT("BROADCAST"), TEXT("BebasNeue-Regular.ttf"), TEXT("BarlowCondensed-Regular.ttf"), 1.3f,
				FLinearColor(0.03f, 0.03f, 0.035f, 0.96f), FLinearColor(0.08f, 0.08f, 0.09f, 0.95f), FLinearColor(0.12f, 0.12f, 0.13f, 1.0f),
				FLinearColor(1.0f, 0.78f, 0.12f), FLinearColor(0.95f, 0.22f, 0.2f), FLinearColor(1.0f, 1.0f, 1.0f), FLinearColor(0.68f, 0.68f, 0.7f), 0.0f, true },
		};

		TSharedPtr<const FCompositeFont> FontFile(const TCHAR* File)
		{
			static TMap<FString, TSharedPtr<const FCompositeFont>> Cache;
			if (!File)
			{
				return nullptr;
			}
			if (const TSharedPtr<const FCompositeFont>* Found = Cache.Find(File))
			{
				return *Found;
			}
			const FString Path = FPaths::ProjectContentDir() / TEXT("DCC/UI/Fonts") / File;
			TSharedPtr<const FCompositeFont> Font;
			if (FPaths::FileExists(Path))
			{
				Font = MakeShared<FStandaloneCompositeFont>(NAME_None, Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
			Cache.Add(File, Font);
			return Font;
		}

		int32 Cached = -1;
	}

	int32 Index()
	{
		if (Cached < 0)
		{
			int32 Style = 0;
			if (GConfig)
			{
				GConfig->GetInt(TEXT("DCCUi"), TEXT("Style"), Style, GGameUserSettingsIni);
			}
			Cached = FMath::Clamp(Style, 0, 2);
		}
		return Cached;
	}

	void Set(int32 Look)
	{
		Cached = FMath::Clamp(Look, 0, 2);
		if (GConfig)
		{
			GConfig->SetInt(TEXT("DCCUi"), TEXT("Style"), Cached, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	const FLook& Get()
	{
		return Looks[Index()];
	}

	const TCHAR* Name(int32 Look)
	{
		return Looks[FMath::Clamp(Look, 0, 2)].Name;
	}

	FSlateFontInfo Head(float Size, int32 Spacing)
	{
		const FLook& K = Get();
		if (TSharedPtr<const FCompositeFont> F = FontFile(K.HeadFile))
		{
			FSlateFontInfo Info(F, Size * K.HeadScale);
			Info.LetterSpacing = Spacing;
			return Info;
		}
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FSlateFontInfo Body(float Size, int32 Spacing)
	{
		const FLook& K = Get();
		if (TSharedPtr<const FCompositeFont> F = FontFile(K.BodyFile))
		{
			FSlateFontInfo Info(F, Size * (K.HeadScale > 1.0f ? 1.15f : 1.0f));
			Info.LetterSpacing = Spacing;
			return Info;
		}
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle("Regular", Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FSlateFontInfo FromFile(const TCHAR* File, float Size, int32 Spacing)
	{
		if (TSharedPtr<const FCompositeFont> F = FontFile(File))
		{
			FSlateFontInfo Info(F, Size);
			Info.LetterSpacing = Spacing;
			return Info;
		}
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FSlateFontInfo Caps(float Size)
	{
		return Head(Size, Get().HeadFile ? 120 : 160);
	}

	const FSlateBrush* RoundedWhite(float Radius)
	{
		static TMap<int32, TUniquePtr<FSlateRoundedBoxBrush>> Cache;
		const int32 R = FMath::Clamp(FMath::RoundToInt(Radius), 0, 64);
		if (TUniquePtr<FSlateRoundedBoxBrush>* Found = Cache.Find(R))
		{
			return Found->Get();
		}
		return Cache.Add(R, MakeUnique<FSlateRoundedBoxBrush>(FLinearColor::White, float(R))).Get();
	}

	const FSlateBrush* Outline(float Radius, const FLinearColor& Colour, float Width)
	{
		static TMap<uint32, TUniquePtr<FSlateRoundedBoxBrush>> Cache;
		const FColor Q = Colour.ToFColor(false);
		const uint32 K = HashCombine(HashCombine(GetTypeHash(Q), GetTypeHash(FMath::RoundToInt(Radius))), GetTypeHash(FMath::RoundToInt(Width * 4.0f)));
		if (TUniquePtr<FSlateRoundedBoxBrush>* Found = Cache.Find(K))
		{
			return Found->Get();
		}
		return Cache.Add(K, MakeUnique<FSlateRoundedBoxBrush>(FLinearColor::Transparent, Radius, Colour, Width)).Get();
	}

	const FSlateBrush* Framed(float Radius, const FLinearColor& Colour, float Width)
	{
		static TMap<uint32, TUniquePtr<FSlateRoundedBoxBrush>> Cache;
		const FColor Q = Colour.ToFColor(false);
		const uint32 K = HashCombine(HashCombine(GetTypeHash(Q), GetTypeHash(FMath::RoundToInt(Radius))), GetTypeHash(FMath::RoundToInt(Width * 4.0f)));
		if (TUniquePtr<FSlateRoundedBoxBrush>* Found = Cache.Find(K))
		{
			return Found->Get();
		}
		return Cache.Add(K, MakeUnique<FSlateRoundedBoxBrush>(FLinearColor::White, Radius, Colour, Width)).Get();
	}

	FVoice VoiceFor(const FString& Speaker, const FLinearColor& Fallback)
	{
		const FLook& K = Get();
		const FString S = Speaker.ToLower();
		FVoice V;
		V.Box = K.Fill;
		V.Edge = FLinearColor(Fallback.R, Fallback.G, Fallback.B, 0.8f);
		V.Name = Fallback;
		V.Text = K.Ink;
		V.Tag = TEXT("");
		V.bLowerThird = false;
		if (S.Contains(TEXT("hiss")))
		{
			// The talk show: gold on black, a TV lower third.
			V.Box = FLinearColor(0.02f, 0.02f, 0.025f, 0.94f);
			V.Edge = FLinearColor(1.0f, 0.78f, 0.12f);
			V.Name = FLinearColor(1.0f, 0.78f, 0.12f);
			V.Text = FLinearColor::White;
			V.Tag = TEXT("LIVE  ON AIR");
			V.bLowerThird = true;
		}
		else if (S.Contains(TEXT("system")) || S == TEXT("ai") || S.Contains(TEXT("announcer")))
		{
			// The System: cold cyan glass.
			V.Box = FLinearColor(0.01f, 0.05f, 0.08f, 0.92f);
			V.Edge = FLinearColor(0.35f, 0.95f, 1.0f);
			V.Name = FLinearColor(0.35f, 0.95f, 1.0f);
			V.Text = FLinearColor(0.85f, 0.98f, 1.0f);
			V.Tag = TEXT("SYSTEM MESSAGE");
		}
		else if (S.Contains(TEXT("cillinan")))
		{
			// The alchemist: bottle green and brass.
			V.Box = FLinearColor(0.02f, 0.07f, 0.04f, 0.93f);
			V.Edge = FLinearColor(0.85f, 0.7f, 0.35f);
			V.Name = FLinearColor(0.6f, 1.0f, 0.7f);
			V.Text = FLinearColor(0.92f, 1.0f, 0.94f);
			V.Tag = TEXT("PRIZE COUNTER");
		}
		else if (S.Contains(TEXT("rhee")) || S.Contains(TEXT("jan")) || S.Contains(TEXT("diaz")))
		{
			// The impound yard: hazard orange.
			V.Box = FLinearColor(0.07f, 0.04f, 0.01f, 0.93f);
			V.Edge = FLinearColor(1.0f, 0.55f, 0.1f);
			V.Name = FLinearColor(1.0f, 0.62f, 0.2f);
			V.Tag = TEXT("THE YARD");
		}
		else if (S.Contains(TEXT("bartender")))
		{
			V.Box = FLinearColor(0.07f, 0.035f, 0.02f, 0.93f);
			V.Edge = FLinearColor(0.95f, 0.6f, 0.3f);
			V.Name = FLinearColor(1.0f, 0.75f, 0.45f);
			V.Tag = TEXT("THE BAR");
		}
		return V;
	}
}

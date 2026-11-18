#include "UI/DCCHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"

void ADCCHUD::PushAchievement(const FString& Title, const FString& Reward)
{
	FAchieveToast Toast;
	Toast.Title = Title.IsEmpty() ? TEXT("Achievement") : Title;
	Toast.Reward = Reward.IsEmpty() ? TEXT("on the record") : Reward;
	AchieveToasts.Add(Toast);
	UE_LOG(LogTemp, Log, TEXT("[DCC Achieve] queued '%s' (%s)  depth %d"), *Toast.Title, *Toast.Reward, AchieveToasts.Num());
}

void ADCCHUD::DrawAchievementPopups()
{
	if (!Canvas || AchieveToasts.Num() == 0)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	FAchieveToast& Toast = AchieveToasts[0];
	if (Toast.StartedAt < 0.0)
	{
		Toast.StartedAt = Now;
	}

	const double Age = Now - Toast.StartedAt;
	constexpr double SlideIn = 0.35;
	constexpr double HoldUntil = 4.0;
	constexpr double SlideOut = 4.40;

	float Alpha = 1.0f;
	float Drop = 0.0f;
	if (Age < SlideIn)
	{
		const float T = static_cast<float>(Age / SlideIn);
		Alpha = T;
		Drop = -78.0f * (1.0f - T);
	}
	else if (Age > HoldUntil)
	{
		const float T = FMath::Clamp(static_cast<float>((Age - HoldUntil) / (SlideOut - HoldUntil)), 0.0f, 1.0f);
		Alpha = 1.0f - T;
		Drop = -78.0f * T;
	}

	if (Age >= SlideOut)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Achieve] done '%s'"), *Toast.Title);
		AchieveToasts.RemoveAt(0);
		return;
	}

	const float W = 440.0f;
	const float H = 78.0f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = 28.0f + Drop;

	int32 Style = 0;
	if (GConfig)
	{
		GConfig->GetInt(TEXT("DCCUi"), TEXT("Style"), Style, GGameUserSettingsIni);
	}
	const FLinearColor Glass = Style == 2
		? FLinearColor(0.76f, 0.62f, 0.40f, 0.94f * Alpha)
		: Style == 1
		? FLinearColor(0.08f, 0.02f, 0.14f, 0.90f * Alpha)
		: FLinearColor(0.04f, 0.07f, 0.13f, 0.88f * Alpha); // dark frosted glass, like the menu
	const FLinearColor Edge = Style == 2
		? FLinearColor(0.25f, 0.14f, 0.05f, Alpha)
		: FLinearColor(0.95f, 0.78f, 0.28f, Alpha);
	DrawRect(Glass, X, Y, W, H);
	DrawRect(Edge, X, Y, W, 2.0f);
	DrawRect(Edge, X + 14.0f, Y + 28.0f, 16.0f, 16.0f);

	const FLinearColor Gold = Style == 2 ? FLinearColor(0.22f, 0.12f, 0.05f, Alpha) : FLinearColor(0.95f, 0.82f, 0.40f, Alpha);
	const FLinearColor Ink = Style == 2 ? FLinearColor(0.12f, 0.08f, 0.04f, Alpha) : FLinearColor(0.94f, 0.94f, 0.96f, Alpha);
	const FLinearColor Dim = Style == 2 ? FLinearColor(0.35f, 0.24f, 0.12f, Alpha) : FLinearColor(0.75f, 0.74f, 0.70f, Alpha);
	DrawText(TEXT("ACHIEVEMENT UNLOCKED"), Gold, X + 40.0f, Y + 10.0f, nullptr, 0.72f);
	DrawText(Toast.Title, Ink, X + 40.0f, Y + 30.0f, nullptr, 1.15f);
	DrawText(Toast.Reward, Dim, X + 40.0f, Y + 54.0f, nullptr, 0.78f);
}

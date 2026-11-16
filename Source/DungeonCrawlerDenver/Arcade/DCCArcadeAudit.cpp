#include "Arcade/DCCArcadeAudit.h"

#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Session/DCCProfile.h"
#include "UObject/ConstructorHelpers.h"

ADCCAuditConstruct::ADCCAuditConstruct()
{
	// A target, not a fighter. Aggro 0 means the chase never acquires anyone
	// (it only matches a pawn standing on this exact point), and no controller
	// means there is nothing to path with.
	AggroRadius = 0.0f;
	HitStaggerStrength = 0.0f;
	AutoPossessAI = EAutoPossessAI::Disabled;
	DisplayName = FText::FromString(TEXT("The Quarterly"));

	// Big enough that a click on the body connects, on every machine. Aim
	// traces use Visibility, which a default capsule does not block.
	GetCapsuleComponent()->InitCapsuleSize(60.0f, 110.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(
			TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		if (Manny.Succeeded())
		{
			Body->SetSkeletalMesh(Manny.Object);
		}
		Body->SetRelativeLocation(FVector(0.0f, 0.0f, -110.0f));
		Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

void ADCCAuditConstruct::BeginPlay()
{
	Super::BeginPlay();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->DisableMovement();
	}
}

void ADCCArcadeAudit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Construct && Construct->Stats)
	{
		Construct->Stats->OnHealthChanged.RemoveDynamic(this, &ADCCArcadeAudit::OnConstructHurt);
	}
	ADCCCharacterBase::OnAnyDamageLanded.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

ADCCArcadeArena::FSky ADCCArcadeAudit::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -75.0f;
	Sky.SunIntensity = 1.6f;
	Sky.SunColour = FLinearColor(1.0f, 0.75f, 0.45f);
	Sky.SkyIntensity = 0.15f;
	Sky.FogColour = FLinearColor(0.08f, 0.05f, 0.07f);
	Sky.FogDensity = 0.02f;
	return Sky;
}

void ADCCArcadeAudit::GetStartTransforms(TArray<FTransform>& Out) const
{
	// An arc south of the construct, close enough to walk into fist range
	// and far enough that a ranged shot has somewhere to travel.
	const FVector Mark(0.0f, 500.0f, 0.0f);
	for (int32 i = 0; i < 6; ++i)
	{
		const float Angle = FMath::DegreesToRadians(-50.0f + i * 20.0f);
		const FVector Feet = Mark + FVector(FMath::Sin(Angle), -FMath::Cos(Angle), 0.0f) * 780.0f;
		const float Yaw = (Mark - Feet).Rotation().Yaw;
		Out.Add(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Feet.X, Feet.Y, 120.0f)));
	}
}

void ADCCArcadeAudit::BuildLocal()
{
	const FLinearColor Floor(0.04f, 0.035f, 0.04f);
	const FLinearColor Wall(0.07f, 0.05f, 0.06f);
	const FLinearColor Amber(1.0f, 0.5f, 0.12f);

	Block(FVector(0, 200, -20), FVector(4200, 3400, 40), Floor);
	Block(FVector(0, 1880, 280), FVector(4200, 40, 560), Wall);
	Block(FVector(0, -1480, 280), FVector(4200, 40, 560), Wall);
	Block(FVector(2080, 200, 280), FVector(40, 3400, 560), Wall);
	Block(FVector(-2080, 200, 280), FVector(40, 3400, 560), Wall);

	// The mark. Non-colliding, so an aim trace reaches the construct and not the paint.
	const FVector Mark(0.0f, 500.0f, 0.0f);
	Shape(TEXT("Cylinder"), Mark + FVector(0, 0, 3), FVector(360, 360, 6), FLinearColor(0.75f, 0.08f, 0.06f), FRotator::ZeroRotator, false);
	Shape(TEXT("Cylinder"), Mark + FVector(0, 0, 5), FVector(200, 200, 6), FLinearColor(0.95f, 0.92f, 0.85f), FRotator::ZeroRotator, false);
	Shape(TEXT("Cylinder"), Mark + FVector(0, 0, 7), FVector(70, 70, 6), Amber, FRotator::ZeroRotator, false);

	Light(Mark + FVector(0, 0, 520), Amber, 14000.0f, 1400.0f);
	Light(FVector(0, -400, 480), FLinearColor(0.55f, 0.5f, 0.6f), 4000.0f, 1800.0f);

	Sign(FVector(0, 1840, 460), -90.0f, TEXT("STRESS TEST"), FColor(255, 150, 50), 90.0f);
	Sign(FVector(0, 1840, 370), -90.0f, TEXT("Forty seconds. Fists, weapons, spells. The Quarterly writes it all down."), FColor(220, 210, 200), 22.0f);
	Sign(FVector(0, 1840, 310), -90.0f, TEXT("Per crawler:  80 meets     180 exceeds     340 standing ovation"), FColor(230, 190, 130), 20.0f);
}

void ADCCArcadeAudit::SpawnConstruct()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || Construct)
	{
		return;
	}

	const FTransform At(FRotator(0.0f, -90.0f, 0.0f), FVector(0.0f, 500.0f, 110.0f));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Construct = World->SpawnActor<ADCCAuditConstruct>(ADCCAuditConstruct::StaticClass(), At, Params);
	if (!Construct)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Audit] The Quarterly did not spawn."));
		return;
	}

	KeepConstructStanding();
	if (Construct->Stats)
	{
		Construct->Stats->OnHealthChanged.AddDynamic(this, &ADCCArcadeAudit::OnConstructHurt);
	}
}

void ADCCArcadeAudit::KeepConstructStanding()
{
	UDCCStatsComponent* Stats = Construct ? Construct->Stats : nullptr;
	if (!Stats || !HasAuthority())
	{
		return;
	}
	if (Stats->MaxHitPoints != ConstructHitPoints)
	{
		Stats->MaxHitPoints = ConstructHitPoints;
	}
	if (Stats->CurrentHitPoints < ConstructHitPoints / 2)
	{
		Stats->CurrentHitPoints = ConstructHitPoints;
	}
}

void ADCCArcadeAudit::OnConstructHurt(UDCCStatsComponent* Stats, int32 NewHitPoints, int32 Delta)
{
	if (bClockRunning && Delta < 0)
	{
		PartyDamage += -Delta;
		Recent.Add({ ServerTime(), -Delta });
	}
	// ApplyDamage broadcasts this before it declares a death. Putting the
	// points back here means a single enormous hit cannot end the round.
	if (Stats && Stats->CurrentHitPoints < ConstructHitPoints / 2)
	{
		Stats->MaxHitPoints = ConstructHitPoints;
		Stats->CurrentHitPoints = ConstructHitPoints;
	}
}

void ADCCArcadeAudit::OnDamageLanded(ADCCCharacterBase* Dealer, ADCCCharacterBase* Target, int32 Amount)
{
	if (!bClockRunning || !Dealer || Target != Construct || Amount <= 0)
	{
		return;
	}

	FString Name;
	if (const APlayerState* State = Dealer->GetPlayerState())
	{
		Name = State->GetPlayerName();
	}
	if (Name.IsEmpty())
	{
		Name = Dealer->DisplayName.IsEmpty() ? Dealer->GetName() : Dealer->DisplayName.ToString();
	}
	ByPlayer.FindOrAdd(Name) += Amount;
}

int32 ADCCArcadeAudit::Tier() const
{
	if (PartyDamage >= Goal(OvationEach))
	{
		return 3;
	}
	if (PartyDamage >= Goal(ExceedsEach))
	{
		return 2;
	}
	if (PartyDamage >= Goal(MeetsEach))
	{
		return 1;
	}
	return 0;
}

void ADCCArcadeAudit::ServerBegin()
{
	SpawnConstruct();
	ADCCCharacterBase::OnAnyDamageLanded.AddUObject(this, &ADCCArcadeAudit::OnDamageLanded);

	Title = TEXT("STRESS TEST");
	Accent = FLinearColor(1.0f, 0.55f, 0.15f);
	LiveAt = ServerTime() + StandbySeconds;
	EndsAt = LiveAt + RoundSeconds;
	RefreshStatus();
}

void ADCCArcadeAudit::RefreshStatus()
{
	const double Now = ServerTime();
	const int32 Meets = Goal(MeetsEach);
	const int32 Exceeds = Goal(ExceedsEach);
	const int32 Ovation = Goal(OvationEach);

	const TCHAR* Rating = TEXT("BELOW EXPECTATIONS");
	int32 Next = Meets;
	switch (Tier())
	{
	case 1: Rating = TEXT("MEETS EXPECTATIONS"); Next = Exceeds; break;
	case 2: Rating = TEXT("EXCEEDS EXPECTATIONS"); Next = Ovation; break;
	case 3: Rating = TEXT("STANDING OVATION"); Next = Ovation; break;
	default: break;
	}

	int32 Window = 0;
	for (int32 i = Recent.Num() - 1; i >= 0; --i)
	{
		if (Now - Recent[i].Time > 3.0)
		{
			Recent.RemoveAt(0, i + 1, EAllowShrinking::No);
			break;
		}
		Window += Recent[i].Amount;
	}

	StatusLines.Reset();
	if (!bClockRunning && !bWrapped)
	{
		const float Wait = FMath::Max(0.0f, static_cast<float>(LiveAt - Now));
		StatusLines.Add(FString::Printf(TEXT("The focus group is sitting down. %.0f"), Wait));
		StatusLines.Add(TEXT("Hit The Quarterly. Fists, weapons and spells all count."));
		return;
	}

	const float Left = FMath::Max(0.0f, static_cast<float>(EndsAt - Now));
	StatusLines.Add(FString::Printf(TEXT("%d:%02d     %s"), FMath::FloorToInt(Left / 60.0f), FMath::FloorToInt(Left) % 60, Rating));
	if (Tier() < 3)
	{
		StatusLines.Add(FString::Printf(TEXT("PARTY  %d / %d     %.1f / sec"), PartyDamage, Next, Window / 3.0f));
	}
	else
	{
		StatusLines.Add(FString::Printf(TEXT("PARTY  %d     %.1f / sec     the sponsor is on its feet"), PartyDamage, Window / 3.0f));
	}

	TArray<TPair<FString, int32>> Rows;
	for (const TPair<FString, int32>& Pair : ByPlayer)
	{
		Rows.Add(Pair);
	}
	Rows.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });

	FString Names;
	for (int32 i = 0; i < Rows.Num() && i < 5; ++i)
	{
		Names += FString::Printf(TEXT("%s%s %d"), Names.IsEmpty() ? TEXT("") : TEXT("    "), *Rows[i].Key, Rows[i].Value);
	}
	if (!Names.IsEmpty())
	{
		StatusLines.Add(Names);
	}
}

void ADCCArcadeAudit::Wrap()
{
	if (bWrapped)
	{
		return;
	}
	bWrapped = true;
	bClockRunning = false;

	const int32 Rating = Tier();
	const int32 BaseTickets = Rating == 3 ? 55 : Rating == 2 ? 32 : Rating == 1 ? 16 : 6;
	const TCHAR* RatingName = Rating == 3 ? TEXT("STANDING OVATION")
		: Rating == 2 ? TEXT("EXCEEDS EXPECTATIONS")
		: Rating == 1 ? TEXT("MEETS EXPECTATIONS")
		: TEXT("BELOW EXPECTATIONS");

	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		if (!PC || !PC->PlayerState)
		{
			continue;
		}
		const FString Name = PC->PlayerState->GetPlayerName();
		const int32 Personal = ByPlayer.FindRef(Name);
		const int32 Tickets = BaseTickets + FMath::Clamp(Personal / 20, 0, 24);

		DCCProfile::Add(PC->PlayerState, TEXT("Audit"), TEXT("Runs"), 1.0f);
		if (Personal > 0)
		{
			DCCProfile::RecordBest(PC->PlayerState, TEXT("Audit"), TEXT("BestDamage"), static_cast<float>(Personal));
		}
		DCCProfile::RecordBest(PC->PlayerState, TEXT("Audit"), TEXT("BestParty"), static_cast<float>(PartyDamage));
		UDCCWalletComponent::AwardTickets(PC->PlayerState, Tickets, TEXT("stress test"));
	}

	StatusLines.Reset();
	StatusLines.Add(FString::Printf(TEXT("Notes filed. %s. Party %d."), RatingName, PartyDamage));
	FString Line;
	TArray<TPair<FString, int32>> Rows;
	for (const TPair<FString, int32>& Pair : ByPlayer)
	{
		Rows.Add(Pair);
	}
	Rows.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
	for (const TPair<FString, int32>& Row : Rows)
	{
		Line += FString::Printf(TEXT("%s%s %d"), Line.IsEmpty() ? TEXT("") : TEXT("    "), *Row.Key, Row.Value);
	}
	if (!Line.IsEmpty())
	{
		StatusLines.Add(Line);
	}
	StatusLines.Add(FString::Printf(TEXT("%d tickets for the rating, plus a little for what you landed."), BaseTickets));
	ReturnToLobbyIn(20.0f);
}

void ADCCArcadeAudit::ServerTick(float DeltaSeconds)
{
	const double Now = ServerTime();
	KeepConstructStanding();

	if (!bWrapped && !bRosterLocked && Now >= LiveAt)
	{
		TArray<APlayerController*> Players;
		GetPlayers(Players);
		RatedPlayers = FMath::Max(1, Players.Num());
		bRosterLocked = true;
	}
	bClockRunning = !bWrapped && bRosterLocked && Now < EndsAt;

	Title = TEXT("STRESS TEST");
	Accent = FLinearColor(1.0f, 0.55f, 0.15f);

	if (!bWrapped && Now >= EndsAt)
	{
		Wrap();
		return;
	}
	if (!bWrapped)
	{
		RefreshStatus();
	}
}

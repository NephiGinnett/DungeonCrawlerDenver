#include "Arcade/DCCArcadeRing.h"

#include "Arcade/DCCArcade.h"
#include "Arcade/DCCWallTV.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCEnemyCharacter_Corporate.h"
#include "Characters/DCCEnemyCharacter_DeadAir.h"
#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Characters/DCCEnemyCharacter_Gate.h"
#include "Characters/DCCEnemyCharacter_Unified.h"
#include "Characters/DCCEnemyCharacter_Wildlife.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Items/DCCLootTypes.h"
#include "Items/DCCPickup_Equipment.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/DCCProfile.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"

namespace
{
	UClass* ClassForKind(const FString& Kind)
	{
		if (Kind == TEXT("brute")) return ADCCEnemy_DemonBull::StaticClass();
		if (Kind == TEXT("stalker")) return ADCCEnemy_MushroomDemon::StaticClass();
		if (Kind == TEXT("hound")) return ADCCEnemyCharacter_SlagDog::StaticClass();
		if (Kind == TEXT("clerk")) return ADCCEnemyCharacter_Intern::StaticClass();
		if (Kind == TEXT("boss")) return ADCCEnemyCharacter_GateChampion::StaticClass();
		if (Kind == TEXT("notary")) return ADCCEnemyCharacter_Notary::StaticClass();
		if (Kind == TEXT("auditor")) return ADCCEnemyCharacter_EscalationSpecialist::StaticClass();
		if (Kind == TEXT("gatekeeper")) return ADCCEnemyCharacter_Gatekeeper::StaticClass();
		if (Kind == TEXT("thornback")) return ADCCEnemyCharacter_Thornback::StaticClass();
		return ADCCEnemyCharacter_DebtEater::StaticClass();
	}

	int32 JsonInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, int32 Default)
	{
		double Value = Default;
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(Key, Value);
		}
		return FMath::RoundToInt(Value);
	}

	FString JsonString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		FString Value;
		if (Obj.IsValid())
		{
			Obj->TryGetStringField(Key, Value);
		}
		return Value;
	}
}

ADCCArcadeRing::ADCCArcadeRing()
{
	Title = TEXT("THE CHAMPIONS' RING");
	Accent = FLinearColor(0.85f, 0.70f, 0.25f);
}

FString ADCCArcadeRing::GetTip() const
{
	return TEXT("E the center podium for the next stage. E the east pad for a tournament. E the south pad to leave.");
}

void ADCCArcadeRing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeRing, bPvpLive);
	DOREPLIFETIME(ADCCArcadeRing, FighterA);
	DOREPLIFETIME(ADCCArcadeRing, FighterB);
}

bool ADCCArcadeRing::PlayersMayFight(const ADCCCharacterBase* A, const ADCCCharacterBase* B) const
{
	if (!bPvpLive || !A || !B || A == B)
	{
		return false;
	}
	if (!A->IsPlayerControlled() || !B->IsPlayerControlled())
	{
		return false;
	}
	const ADCCCharacterBase* First = FighterA.Get();
	const ADCCCharacterBase* Second = FighterB.Get();
	return (A == First && B == Second) || (A == Second && B == First);
}

ADCCArcadeArena::FSky ADCCArcadeRing::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -18.0f;
	Sky.SunIntensity = 2.4f;
	Sky.SunColour = FLinearColor(1.0f, 0.78f, 0.45f);
	Sky.FogColour = FLinearColor(0.18f, 0.10f, 0.22f);
	Sky.FogDensity = 0.018f;
	return Sky;
}

void ADCCArcadeRing::GetStartTransforms(TArray<FTransform>& Out) const
{
	const FVector O = GetActorLocation();
	for (int32 i = 0; i < 5; ++i)
	{
		const FVector At = O + FVector((i - 2) * 180.0f, 1100.0f, 140.0f);
		Out.Add(FTransform(FRotator(0.0f, -90.0f, 0.0f), At));
	}
}

void ADCCArcadeRing::BuildLocal()
{
	const FVector O = GetActorLocation();
	const FLinearColor Ink(0.07f, 0.05f, 0.08f);
	const FLinearColor Gold(0.72f, 0.55f, 0.16f);
	const FLinearColor Rope(0.45f, 0.12f, 0.14f);
	if (AActor* Floor = Block(O + FVector(0, 0, -40), FVector(5200, 5200, 50), Ink))
	{
		Paint(Floor, TEXT("/Game/Synthwave_city/Material/MI_synthwave_wall2"));
	}
	Block(O + FVector(0, 0, 8), FVector(1400, 1400, 16), Gold * 0.35f, 0.0f, false);
	Block(O + FVector(0, 0, 24), FVector(180, 180, 28), Gold);
	Sign(O + FVector(0, -40, 90), 90.0f, TEXT("NEXT STAGE"), FColor(255, 214, 120), 28.0f);
	Block(O + FVector(0, -1600, 12), FVector(220, 220, 16), FLinearColor(0.15f, 0.16f, 0.2f));
	Sign(O + FVector(0, -1500, 90), 90.0f, TEXT("LEAVE"), FColor(200, 210, 230), 32.0f);
	Block(O + FVector(1600, 0, 12), FVector(220, 220, 16), Rope);
	Sign(O + FVector(1480, 0, 90), 0.0f, TEXT("TOURNAMENT"), FColor(255, 180, 140), 28.0f);
	for (int32 i = 0; i < 8; ++i)
	{
		const float Ang = i * (2.0f * PI / 8.0f);
		const FVector At = O + FVector(FMath::Cos(Ang) * 700.0f, FMath::Sin(Ang) * 700.0f, 40.0f);
		Block(At, FVector(40, 40, 80), Rope, 0.0f, true);
	}
	Block(O + FVector(0, 2300, 80), FVector(2200, 280, 160), FLinearColor(0.12f, 0.08f, 0.14f));
	Sign(O + FVector(0, 2140, 220), -90.0f, TEXT("THE CHAMPIONS' RING"), FColor(255, 220, 140), 64.0f);
	Light(O + FVector(0, 0, 520), Gold, 12000.0f, 1600.0f);
	Light(O + FVector(0, 1800, 420), FLinearColor(0.8f, 0.4f, 0.9f), 5000.0f, 900.0f);
	Prop(TEXT("/Game/Ice_Station/Meshes/Crates/SM_crate_01"), O + FVector(1900, 900, 0), 15.0f, FVector(140, 140, 140), false);
	Prop(TEXT("/Game/Ice_Station/Meshes/Computer/SM_computer_01"), O + FVector(-1900, 700, 0), -20.0f, FVector(80, 80, 80), false);
	// Fitted to 3 m and moved beside the banner (2026-11-25): at full size, 4 m from the podium, it swallowed the camera.
	Prop(TEXT("/Game/Ice_Station/Meshes/Hologram/SM_planet_hologram"), O + FVector(-1500, 2000, 40), 0.0f, FVector(300, 300, 300), false);
}

void ADCCArcadeRing::LoadLadder()
{
	Ladder.Reset();
	const FString Path = FPaths::ProjectContentDir() / TEXT("DCC/Ring/ladder.json");
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Text, *Path)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Stages = nullptr;
		if (Root->TryGetArrayField(TEXT("stages"), Stages))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Stages)
			{
				const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!Obj.IsValid())
				{
					continue;
				}
				FDCCRingStage Step;
				Step.Stage = JsonInt(Obj, TEXT("stage"), Ladder.Num() + 1);
				Step.Name = JsonString(Obj, TEXT("name"));
				Step.Kind = JsonString(Obj, TEXT("kind"));
				Step.Hp = FMath::Max(8, JsonInt(Obj, TEXT("hp"), 80));
				Step.Count = FMath::Max(1, JsonInt(Obj, TEXT("count"), 1));
				const FString Skill = JsonString(Obj, TEXT("skill"));
				Step.Skill = Skill.IsEmpty() ? NAME_None : FName(*Skill);
				if (Step.Kind.IsEmpty())
				{
					Step.Kind = TEXT("debtor");
				}
				if (Step.Name.IsEmpty())
				{
					Step.Name = FString::Printf(TEXT("Stage %d"), Step.Stage);
				}
				Ladder.Add(Step);
			}
		}
	}
	if (Ladder.Num() == 0)
	{
		BuildFallback();
	}
}

void ADCCArcadeRing::BuildFallback()
{
	Ladder.Reset();
	const TCHAR* Kinds[] = { TEXT("debtor"), TEXT("clerk"), TEXT("hound"), TEXT("stalker"), TEXT("brute") };
	for (int32 S = 1; S <= 50; ++S)
	{
		FDCCRingStage Step;
		Step.Stage = S;
		const int32 Total = FMath::Max(20, FMath::RoundToInt(80.0f * FMath::Pow(1.12f, S - 1)));
		if (S == 10) { Step.Name = TEXT("The Notary"); Step.Kind = TEXT("notary"); Step.Skill = TEXT("StaticBurst"); Step.Count = 1; Step.Hp = Total; }
		else if (S == 20) { Step.Name = TEXT("The Bull"); Step.Kind = TEXT("brute"); Step.Skill = TEXT("FireFingers"); Step.Count = 1; Step.Hp = Total; }
		else if (S == 30) { Step.Name = TEXT("The Auditor"); Step.Kind = TEXT("auditor"); Step.Skill = TEXT("FirstAid"); Step.Count = 1; Step.Hp = Total; }
		else if (S == 40) { Step.Name = TEXT("The Gatekeeper"); Step.Kind = TEXT("boss"); Step.Skill = TEXT("Whiteout"); Step.Count = 1; Step.Hp = Total; }
		else if (S == 50) { Step.Name = TEXT("The House"); Step.Kind = TEXT("gatekeeper"); Step.Count = 1; Step.Hp = Total; }
		else
		{
			Step.Kind = Kinds[(S - 1) % 5];
			Step.Count = 1 + (S - 1) / 8;
			Step.Hp = FMath::Max(20, Total / Step.Count);
			Step.Name = FString::Printf(TEXT("Stage %d"), S);
		}
		Ladder.Add(Step);
	}
}

void ADCCArcadeRing::ServerBegin()
{
	if (!HasAuthority())
	{
		return;
	}
	LoadLadder();
	Title = TEXT("THE CHAMPIONS' RING");
	BossName = FString();
	BossHealth = -1.0f;
	StatusLines = { TEXT("The ring is empty."), TEXT("E the center podium.") };
	Multicast_Announce(TEXT("Start"));
	UE_LOG(LogTemp, Log, TEXT("[DCC Ring] ready. %d stages."), Ladder.Num());
}

ADCCEnemyCharacter* ADCCArcadeRing::SpawnKind(const FString& Kind, const FVector& At, int32 Hp, const FString& Name)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const FVector O = GetActorLocation();
	const FRotator Face = (O - At).GetSafeNormal2D().Rotation();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADCCEnemyCharacter* Enemy = World->SpawnActor<ADCCEnemyCharacter>(ClassForKind(Kind), At + FVector(0, 0, 100), Face, Params);
	if (!Enemy)
	{
		return nullptr;
	}
	Enemy->DisplayName = FText::FromString(Name.IsEmpty() ? Kind : Name);
	Enemy->CorpseLifeSpan = 0.4f;
	Enemy->AggroRadius = 2500.0f;
	Enemy->LeashRadius = 4000.0f;
	if (Enemy->Stats)
	{
		Enemy->Stats->Level = FMath::Clamp(1 + Hp / 40, 1, 20);
		Enemy->Stats->RefreshDerivedStats(false);
		Enemy->Stats->MaxHitPoints = FMath::Max(1, Hp);
		Enemy->Stats->CurrentHitPoints = Enemy->Stats->MaxHitPoints;
	}
	Enemy->OnCharacterDied.AddDynamic(this, &ADCCArcadeRing::OnFighterDied);
	Live.Add(Enemy);
	return Enemy;
}

void ADCCArcadeRing::ClearStageEnemies()
{
	for (const TWeakObjectPtr<ADCCEnemyCharacter>& Enemy : Live)
	{
		if (Enemy.IsValid())
		{
			Enemy->Destroy();
		}
	}
	Live.Reset();
}

bool ADCCArcadeRing::StageClear() const
{
	for (const TWeakObjectPtr<ADCCEnemyCharacter>& Enemy : Live)
	{
		if (Enemy.IsValid() && Enemy->IsAlive())
		{
			return false;
		}
	}
	return true;
}

const FDCCRingStage* FindStage(const TArray<FDCCRingStage>& Ladder, int32 Stage)
{
	for (const FDCCRingStage& Step : Ladder)
	{
		if (Step.Stage == Stage)
		{
			return &Step;
		}
	}
	return nullptr;
}

void ADCCArcadeRing::StartStage(int32 Next)
{
	if (bEnded || bTournament || bMatchLive || Next < 1)
	{
		return;
	}
	if (Ladder.Num() == 0)
	{
		LoadLadder();
	}
	const FDCCRingStage* Step = FindStage(Ladder, Next);
	if (!Step)
	{
		StatusLines = { TEXT("The ladder is done.") };
		return;
	}
	ClearStageEnemies();
	Stage = Next;
	const FVector O = GetActorLocation();
	int32 Spawned = 0;
	for (int32 i = 0; i < Step->Count; ++i)
	{
		const float Ang = (Step->Count <= 1) ? -HALF_PI : (i / float(Step->Count)) * 2.0f * PI;
		const FVector At = O + FVector(FMath::Cos(Ang) * 480.0f, FMath::Sin(Ang) * 480.0f, 0.0f);
		const FString BodyName = Step->Count > 1 ? FString::Printf(TEXT("%s %d"), *Step->Name, i + 1) : Step->Name;
		if (SpawnKind(Step->Kind, At, Step->Hp, BodyName))
		{
			++Spawned;
		}
	}
	if (Spawned == 0)
	{
		bStageLive = false;
		StatusLines = { TEXT("The ring could not raise anyone.") };
		UE_LOG(LogTemp, Warning, TEXT("[DCC Ring] stage %d spawned nothing (%s)."), Next, *Step->Kind);
		return;
	}
	bStageLive = true;
	BossName = Step->Name;
	BossHealth = 1.0f;
	Multicast_Announce(Step->Skill.IsNone() ? TEXT("Fight") : TEXT("Boss"));
	UE_LOG(LogTemp, Log, TEXT("[DCC Ring] stage %d  %s  x%d  hp %d"), Next, *Step->Name, Spawned, Step->Hp);
	RefreshStatus();
}

void ADCCArcadeRing::GrantChampionSkill()
{
	const FDCCRingStage* Step = FindStage(Ladder, Stage);
	if (!Step || Step->Skill.IsNone())
	{
		return;
	}
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		ADCCCharacterBase* Pawn = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
		if (Pawn && Pawn->IsAlive() && Pawn->Skills)
		{
			Pawn->Skills->GrantSkill(Step->Skill, 1);
			if (Pawn->Wallet)
			{
				Pawn->Wallet->Tell(FString::Printf(TEXT("The ring taught you %s."), *Step->Skill.ToString()));
			}
		}
	}
}

void ADCCArcadeRing::OnStageClear()
{
	bStageLive = false;
	BossName = FString();
	BossHealth = -1.0f;
	BestBanked = FMath::Max(BestBanked, Stage);
	const FDCCRingStage* Step = FindStage(Ladder, Stage);
	// A full climb (stages 1-50) pays 958 tickets: 10 + Stage/3 on the card (908)
	// and a 50 ticket purse when the House falls. Worth a ten-pull at the tubes.
	const int32 Tickets = 10 + Stage / 3;
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		APlayerState* State = PC ? PC->PlayerState : nullptr;
		if (!State)
		{
			continue;
		}
		UDCCWalletComponent::AwardTickets(State, Tickets, FString::Printf(TEXT("ring stage %d"), Stage));
		DCCProfile::RecordBest(State, TEXT("Ring"), TEXT("BestStage"), float(Stage), true);
		if (Stage == 1 && DCCArcade::CompleteSeriesQuest(State, TEXT("Ring"), DCCArcade::SeriesQuestXp()))
		{
			UE_LOG(LogTemp, Log, TEXT("[DCC Ring] series quest paid %d XP to %s"), DCCArcade::SeriesQuestXp(), *State->GetPlayerName());
		}
		if (Stage == 50)
		{
			UDCCWalletComponent::AwardTickets(State, 50, TEXT("the house"));
			const FString Key = DCCProfile::KeyFor(State);
			DCCProfile::SetForKey(Key, TEXT("Titles"), TEXT("CHAMPION OF THE RING"), 1.0f);
			if (ADCCCharacterBase* Pawn = Cast<ADCCCharacterBase>(PC->GetPawn()))
			{
				if (Pawn->Wallet)
				{
					Pawn->Wallet->RefreshFromProfile();
				}
				Pawn->RefreshNameplateNow();
			}
		}
	}
	GrantChampionSkill();
	if (Stage == 50)
	{
		FDCCRolledLoot Rolled;
		Rolled.PickupClass = ADCCPickup_Equipment::StaticClass();
		Rolled.EquipmentItemId = TEXT("TenderiserMallet");
		DCCLoot::SpawnRolledLoot(GetWorld(), Rolled, GetActorLocation() + FVector(0, 220, 40), nullptr);
		DCCNews::Post(GetWorld(), TEXT("The House fell. New champions of the ring."));
		Multicast_Announce(TEXT("Win"));
	}
	else
	{
		Multicast_Announce(TEXT("Win"));
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Ring] cleared stage %d. Tickets %d each.%s"), Stage, Tickets,
		Stage == 50 ? TEXT(" Title and a mallet.") : TEXT(""));
	(void)Step;
	RefreshStatus();
}

void ADCCArcadeRing::PlaceFighter(APlayerController* PC, const FVector& At)
{
	ADCCCharacterBase* Pawn = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Pawn)
	{
		return;
	}
	const FRotator Face = (GetActorLocation() - At).GetSafeNormal2D().Rotation();
	Pawn->SetActorLocation(At + FVector(0, 0, 40), false, nullptr, ETeleportType::TeleportPhysics);
	Pawn->SetActorRotation(Face);
	if (AController* Controller = Pawn->GetController())
	{
		Controller->SetControlRotation(Face);
	}
	if (UCharacterMovementComponent* Move = Pawn->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	if (Pawn->Stats)
	{
		Pawn->Stats->CurrentHitPoints = FMath::Max(1, Pawn->Stats->MaxHitPoints);
	}
	Pawn->ForgetFall(4.0);
}

void ADCCArcadeRing::SendToStands(APlayerController* ExceptA, APlayerController* ExceptB)
{
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	int32 Seat = 0;
	const FVector O = GetActorLocation();
	for (APlayerController* PC : Players)
	{
		if (!PC || PC == ExceptA || PC == ExceptB)
		{
			continue;
		}
		const FVector At = O + FVector(-900.0f + (Seat % 5) * 220.0f, 2100.0f, 80.0f);
		PlaceFighter(PC, At);
		++Seat;
	}
}

int32 ADCCArcadeRing::SeedOf(const ADCCCharacterBase* Who) const
{
	if (!Who)
	{
		return INDEX_NONE;
	}
	for (int32 i = 0; i < Seeds.Num(); ++i)
	{
		const APlayerController* PC = Seeds[i].PC.Get();
		if (PC && PC->GetPawn() == Who)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void ADCCArcadeRing::BeginTournament()
{
	if (bEnded || bStageLive || bMatchLive)
	{
		StatusLines = { TEXT("Finish what is in the ring first.") };
		return;
	}
	Seeds.Reset();
	RoundWinners.Reset();
	Matches.Reset();
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		if (!PC)
		{
			continue;
		}
		FDCCRingSeed Seed;
		Seed.Name = PC->PlayerState ? PC->PlayerState->GetPlayerName() : TEXT("Crawler");
		Seed.PC = PC;
		Seeds.Add(Seed);
	}
	Humans = Seeds.Num();
	const int32 Bracket = Humans <= 3 ? 4 : 8;
	while (Seeds.Num() < Bracket - 1)
	{
		FDCCRingSeed Bot;
		Bot.Name = FString::Printf(TEXT("House staff %d"), Seeds.Num());
		Bot.bBot = true;
		Seeds.Add(Bot);
	}
	if (Seeds.Num() > Bracket - 1)
	{
		Seeds.SetNum(Bracket - 1);
		Humans = FMath::Min(Humans, Bracket - 1);
	}
	FDCCRingSeed Champ;
	Champ.Name = TEXT("The House");
	Champ.bBot = true;
	Champ.bChampion = true;
	Seeds.Add(Champ);
	bTournament = true;
	Matches.Reset();
	RoundWinners.Reset();
	for (int32 i = 0; i + 1 < Seeds.Num(); i += 2)
	{
		Matches.Add(TPair<int32, int32>(i, i + 1));
	}
	MatchCursor = 0;
	Multicast_Announce(TEXT("Start"));
	UE_LOG(LogTemp, Log, TEXT("[DCC Ring] tournament. humans %d bracket %d."), Humans, Seeds.Num());
	AdvanceBracket();
}

void ADCCArcadeRing::AdvanceBracket()
{
	if (bEnded || !bTournament)
	{
		return;
	}
	if (MatchCursor < Matches.Num())
	{
		const TPair<int32, int32>& Pair = Matches[MatchCursor];
		StartMatch(Pair.Key, Pair.Value);
		return;
	}
	if (RoundWinners.Num() <= 1)
	{
		EndTournament(RoundWinners.Num() ? RoundWinners[0] : INDEX_NONE);
		return;
	}
	const TArray<int32> Next = RoundWinners;
	RoundWinners.Reset();
	Matches.Reset();
	for (int32 i = 0; i + 1 < Next.Num(); i += 2)
	{
		Matches.Add(TPair<int32, int32>(Next[i], Next[i + 1]));
	}
	if (Next.Num() % 2 == 1)
	{
		RoundWinners.Add(Next.Last());
	}
	MatchCursor = 0;
	if (Matches.Num() == 0)
	{
		EndTournament(RoundWinners.Num() ? RoundWinners[0] : INDEX_NONE);
		return;
	}
	StartMatch(Matches[0].Key, Matches[0].Value);
}

void ADCCArcadeRing::StartMatch(int32 SeedA, int32 SeedB)
{
	if (!Seeds.IsValidIndex(SeedA) || !Seeds.IsValidIndex(SeedB))
	{
		return;
	}
	MatchA = SeedA;
	MatchB = SeedB;
	ClearStageEnemies();
	FighterA = nullptr;
	FighterB = nullptr;
	bPvpLive = false;
	const FDCCRingSeed& SideA = Seeds[SeedA];
	const FDCCRingSeed& SideB = Seeds[SeedB];
	if (SideA.bBot && SideB.bBot)
	{
		bAwaitingBodies = false;
		bMatchLive = true;
		int32 Winner = SeedA < SeedB ? SeedA : SeedB;
		if (SideA.bChampion) { Winner = SeedA; }
		else if (SideB.bChampion) { Winner = SeedB; }
		FinishMatch(Winner);
		return;
	}
	APlayerController* HumanA = SideA.bBot ? nullptr : SideA.PC.Get();
	APlayerController* HumanB = SideB.bBot ? nullptr : SideB.PC.Get();
	if ((HumanA && !HumanA->GetPawn()) || (HumanB && !HumanB->GetPawn()))
	{
		bMatchLive = false;
		bAwaitingBodies = true;
		StatusLines = { TEXT("Waiting on a fighter.") };
		return;
	}
	bAwaitingBodies = false;
	bMatchLive = true;
	MatchEndsAt = ServerTime() + 90.0;
	const FVector O = GetActorLocation();
	const FVector West = O + FVector(0, -420, 0);
	const FVector East = O + FVector(0, 420, 0);
	SendToStands(HumanA, HumanB);
	if (HumanA) { PlaceFighter(HumanA, West); }
	if (HumanB) { PlaceFighter(HumanB, East); }
	auto RaiseBot = [this](const FDCCRingSeed& Bot, const FVector& At) -> ADCCCharacterBase*
	{
		const FString Kind = Bot.bChampion ? TEXT("gatekeeper") : TEXT("brute");
		const int32 Hp = Bot.bChampion ? 420 : 180;
		return SpawnKind(Kind, At, Hp, Bot.Name);
	};
	FighterA = HumanA ? Cast<ADCCCharacterBase>(HumanA->GetPawn()) : RaiseBot(SideA, West);
	FighterB = HumanB ? Cast<ADCCCharacterBase>(HumanB->GetPawn()) : RaiseBot(SideB, East);
	bPvpLive = HumanA && HumanB;
	BossName = FString::Printf(TEXT("%s vs %s"), *SideA.Name, *SideB.Name);
	BossHealth = 1.0f;
	Multicast_Announce(TEXT("Fight"));
	UE_LOG(LogTemp, Log, TEXT("[DCC Ring] match %s vs %s"), *SideA.Name, *SideB.Name);
	RefreshStatus();
}

void ADCCArcadeRing::FinishMatch(int32 WinnerSeed)
{
	if (!bMatchLive)
	{
		return;
	}
	bMatchLive = false;
	bPvpLive = false;
	bAwaitingBodies = false;
	FighterA = nullptr;
	FighterB = nullptr;
	ClearStageEnemies();
	const int32 Loser = WinnerSeed == MatchA ? MatchB : MatchA;
	if (Seeds.IsValidIndex(Loser))
	{
		Seeds[Loser].bOut = true;
	}
	RoundWinners.Add(WinnerSeed);
	++MatchCursor;
	if (Seeds.IsValidIndex(WinnerSeed) && !Seeds[WinnerSeed].bBot)
	{
		PlaceFighter(Seeds[WinnerSeed].PC.Get(), GetActorLocation() + FVector(0, 1100, 0));
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Ring] match won by seed %d"), WinnerSeed);
	AdvanceBracket();
}

void ADCCArcadeRing::EndTournament(int32 WinnerSeed)
{
	bTournament = false;
	bMatchLive = false;
	bPvpLive = false;
	bAwaitingBodies = false;
	FighterA = nullptr;
	FighterB = nullptr;
	ClearStageEnemies();
	BossName = FString();
	BossHealth = -1.0f;
	if (!Seeds.IsValidIndex(WinnerSeed) || Seeds[WinnerSeed].bBot)
	{
		Multicast_Announce(TEXT("Lose"));
		StatusLines = { TEXT("The house keeps the belt."), TEXT("E the center podium to go back to the ladder.") };
		UE_LOG(LogTemp, Log, TEXT("[DCC Ring] tournament to the house."));
		return;
	}
	APlayerController* PC = Seeds[WinnerSeed].PC.Get();
	APlayerState* State = PC ? PC->PlayerState : nullptr;
	if (Humans >= 2)
	{
		if (State)
		{
			UDCCWalletComponent::AwardTickets(State, 750, TEXT("ring tournament"));
			const FString Key = DCCProfile::KeyFor(State);
			DCCProfile::SetForKey(Key, TEXT("Titles"), TEXT("CHAMPION OF THE RING"), 1.0f);
			if (ADCCCharacterBase* Pawn = Cast<ADCCCharacterBase>(PC->GetPawn()))
			{
				if (Pawn->Wallet)
				{
					Pawn->Wallet->RefreshFromProfile();
				}
				Pawn->RefreshNameplateNow();
			}
			DCCNews::Post(GetWorld(), FString::Printf(TEXT("%s is CHAMPION OF THE RING."), *State->GetPlayerName()));
		}
		Multicast_Announce(TEXT("Win"));
		StatusLines = { TEXT("CHAMPION OF THE RING."), TEXT("750 tickets.") };
	}
	else
	{
		if (State)
		{
			UDCCWalletComponent::AwardTickets(State, 200, TEXT("ring exhibition"));
		}
		FDCCRolledLoot Rolled;
		Rolled.PickupClass = ADCCPickup_Equipment::StaticClass();
		Rolled.EquipmentItemId = TEXT("FlareGunEmergency");
		DCCLoot::SpawnRolledLoot(GetWorld(), Rolled, GetActorLocation() + FVector(0, 220, 40), nullptr);
		Multicast_Announce(TEXT("Win"));
		StatusLines = { TEXT("Exhibition closed."), TEXT("200 tickets and a flare gun.") };
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Ring] tournament won by %s. humans %d."), *Seeds[WinnerSeed].Name, Humans);
}

void ADCCArcadeRing::OnFighterDied(ADCCCharacterBase* Who)
{
	if (!bMatchLive || !Who)
	{
		return;
	}
	if (Who == FighterA.Get())
	{
		FinishMatch(MatchB);
	}
	else if (Who == FighterB.Get())
	{
		FinishMatch(MatchA);
	}
}

void ADCCArcadeRing::OnRespawned(APlayerController* PC)
{
	if (!HasAuthority() || !bTournament || !PC)
	{
		return;
	}
	ADCCCharacterBase* Pawn = Cast<ADCCCharacterBase>(PC->GetPawn());
	if (bMatchLive && Pawn && (Pawn == FighterA.Get() || Pawn == FighterB.Get()))
	{
		return;
	}
	PlaceFighter(PC, GetActorLocation() + FVector(-400.0f, 2100.0f, 80.0f));
}

void ADCCArcadeRing::ServerTick(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (bEnded || bRoundOver)
	{
		return;
	}
	if (bAwaitingBodies && !bMatchLive)
	{
		StartMatch(MatchA, MatchB);
	}
	if (bMatchLive)
	{
		ADCCCharacterBase* A = FighterA.Get();
		ADCCCharacterBase* B = FighterB.Get();
		const bool bADead = !A || !A->IsAlive();
		const bool bBDead = !B || !B->IsAlive();
		if (bADead || bBDead)
		{
			if (bADead && bBDead)
			{
				FinishMatch(MatchA < MatchB ? MatchA : MatchB);
			}
			else
			{
				FinishMatch(bADead ? MatchB : MatchA);
			}
		}
		else if (ServerTime() >= MatchEndsAt)
		{
			const float PctA = A->Stats && A->Stats->MaxHitPoints > 0
				? float(A->Stats->CurrentHitPoints) / float(A->Stats->MaxHitPoints) : 0.0f;
			const float PctB = B->Stats && B->Stats->MaxHitPoints > 0
				? float(B->Stats->CurrentHitPoints) / float(B->Stats->MaxHitPoints) : 0.0f;
			FinishMatch(PctA >= PctB ? MatchA : MatchB);
		}
	}
	if (bStageLive && StageClear())
	{
		OnStageClear();
	}
	if (bStageLive)
	{
		float Sum = 0.0f;
		int32 N = 0;
		for (const TWeakObjectPtr<ADCCEnemyCharacter>& Enemy : Live)
		{
			if (Enemy.IsValid() && Enemy->IsAlive() && Enemy->Stats && Enemy->Stats->MaxHitPoints > 0)
			{
				Sum += float(Enemy->Stats->CurrentHitPoints) / float(Enemy->Stats->MaxHitPoints);
				++N;
			}
		}
		BossHealth = N > 0 ? Sum / float(N) : -1.0f;
	}
	RefreshStatus();
}

void ADCCArcadeRing::RefreshStatus()
{
	if (bMatchLive && Seeds.IsValidIndex(MatchA) && Seeds.IsValidIndex(MatchB))
	{
		StatusLines = {
			FString::Printf(TEXT("%s vs %s"), *Seeds[MatchA].Name, *Seeds[MatchB].Name),
			TEXT("First down, or the healthier one at 90 seconds."),
			bPvpLive ? TEXT("Only you two can hit each other.") : TEXT("The house sent someone in.")
		};
		return;
	}
	if (bTournament)
	{
		StatusLines = { TEXT("Tournament."), TEXT("Next pair is coming.") };
		return;
	}
	if (bStageLive)
	{
		const FDCCRingStage* Step = FindStage(Ladder, Stage);
		// A placeholder name ("Stage 3") would read "Stage 3  Stage 3"; show a real name only (2026-11-25).
		const FString Named = Step && Step->Name != FString::Printf(TEXT("Stage %d"), Stage) ? Step->Name : FString();
		StatusLines = {
			Named.IsEmpty() ? FString::Printf(TEXT("Stage %d"), Stage) : FString::Printf(TEXT("Stage %d  %s"), Stage, *Named),
			TEXT("Clear the ring."),
			FString::Printf(TEXT("Best stage %d"), BestBanked)
		};
		return;
	}
	StatusLines = {
		Stage <= 0 ? TEXT("Stage 0") : FString::Printf(TEXT("Cleared %d"), Stage),
		Stage >= 50 ? TEXT("The ladder is done.") : TEXT("E the center podium for the next stage."),
		TEXT("East pad: tournament. South pad: leave.")
	};
}

bool ADCCArcadeRing::CanUse(const ADCCCharacterBase* Who) const
{
	if (!Who)
	{
		return false;
	}
	const FVector At = Who->GetActorLocation();
	const FVector O = GetActorLocation();
	auto Near = [&At](const FVector& Mark)
	{
		return FVector::Dist2D(At, Mark) <= 200.0f && FMath::Abs(At.Z - Mark.Z) < 220.0f;
	};
	return Near(O) || Near(O + FVector(0, -1600, 0)) || Near(O + FVector(1600, 0, 0));
}

void ADCCArcadeRing::Use(ADCCCharacterBase* Who)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	const FVector At = Who->GetActorLocation();
	const FVector O = GetActorLocation();
	auto Dist = [&At](const FVector& Mark) { return FVector::Dist2D(At, Mark); };
	const float Center = Dist(O);
	const float South = Dist(O + FVector(0, -1600, 0));
	const float East = Dist(O + FVector(1600, 0, 0));
	if (South <= Center && South <= East)
	{
		if (bMatchLive)
		{
			StatusLines = { TEXT("The match is live."), TEXT("Leave when it ends.") };
			return;
		}
		bEnded = true;
		bStageLive = false;
		bTournament = false;
		bPvpLive = false;
		ClearStageEnemies();
		StatusLines = {
			FString::Printf(TEXT("Banked at stage %d."), BestBanked),
			TEXT("Back to the lobby.")
		};
		ReturnToLobbyIn(8.0f);
		return;
	}
	if (East <= Center)
	{
		BeginTournament();
		return;
	}
	if (bMatchLive || bTournament || bStageLive)
	{
		StatusLines = { TEXT("Finish what is in the ring first.") };
		return;
	}
	if (Stage >= 50)
	{
		StatusLines = { TEXT("The ladder is done."), TEXT("East pad: tournament. South pad: leave.") };
		return;
	}
	StartStage(Stage + 1);
}

void ADCCArcadeRing::DevStage(int32 Next)
{
	if (!HasAuthority() || bMatchLive)
	{
		return;
	}
	bTournament = false;
	bAwaitingBodies = false;
	bPvpLive = false;
	FighterA = nullptr;
	FighterB = nullptr;
	ClearStageEnemies();
	bStageLive = false;
	const int32 Clamped = FMath::Clamp(Next, 1, 50);
	Stage = Clamped - 1;
	StartStage(Clamped);
}

void ADCCArcadeRing::Multicast_Announce_Implementation(const FString& Moment)
{
	DCCAnnouncer::PlayLocal(this, Moment);
}

static void RingStageCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<ADCCArcadeRing> It(World); It; ++It)
	{
		It->DevStage(Args.Num() ? FCString::Atoi(*Args[0]) : 1);
		return;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRingStage(
	TEXT("DCC.RingStage"),
	TEXT("DCC.RingStage <n>: start ladder stage n."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RingStageCmd));

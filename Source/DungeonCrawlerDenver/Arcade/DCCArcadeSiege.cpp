#include "Arcade/DCCArcadeSiege.h"

#include "Arcade/DCCSiegeHooks.h"
#include "Arcade/DCCWallTV.h"
#include "Arcade/DCCArcade.h"
#include "Arcade/DCCSets.h"
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
#include "AIController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Items/DCCLootTypes.h"
#include "Items/DCCPickupVariants.h"
#include "Items/DCCPickup_Equipment.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/DCCProfile.h"
#include "Skills/DCCSkillComponent.h"
#include "Skills/DCCSkillTypes.h"
#include "Stats/DCCStatsComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	float IncomingPerSecond(int32 Wave)
	{
		if (Wave <= 12) return 8.0f + Wave * 0.9f;
		if (Wave == 13) return 130.0f;
		if (Wave <= 19) return 36.0f + (Wave - 14) * 2.0f;
		if (Wave <= 29) return 55.0f + (Wave - 20) * 6.0f;
		return 120.0f + (Wave - 30) * 8.0f;
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

	const TCHAR* BuffNames[] = { TEXT("Amplified"), TEXT("Large"), TEXT("Cold"), TEXT("Commission") };
	const TCHAR* BuffLines[] = {
		TEXT("Amplified. Spells hit harder for 5 waves."),
		TEXT("Comically Large. Your weapon is a problem for 5 waves."),
		TEXT("Cold Call. Hits slow them. Every fifth hit freezes."),
		TEXT("Commission. Kills pay double for 5 waves.")
	};
}

ADCCArcadeSiege::ADCCArcadeSiege()
{
	Title = TEXT("DUNGEON CRAWLER TOWER DEFENSE");
	Accent = FLinearColor(0.95f, 0.45f, 0.2f);
	BossName = TEXT("THE WALL");
	BossHealth = 1.0f;
}

FString ADCCArcadeSiege::GetTip() const
{
	return TEXT("E on a tower charges the crystal. E on a post plants an ally. Step into the road and they fight you.");
}

ADCCArcadeArena::FSky ADCCArcadeSiege::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -25.0f;
	Sky.SunIntensity = 3.5f;
	Sky.SunColour = FLinearColor(1.0f, 0.75f, 0.55f);
	Sky.FogColour = FLinearColor(0.25f, 0.18f, 0.22f);
	Sky.FogDensity = 0.006f; // indoors now (the Power Plant hall): 0.02 turned the far end to soup (2026-11-25)
	return Sky;
}

ADCCArcadeSiege* ADCCArcadeSiege::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADCCArcadeSiege> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADCCArcadeSiege::GetStartTransforms(TArray<FTransform>& Out) const
{
	// Beside the Wall, either side of the road's end (2026-11-25). Not on the road: marchers stop to fight anyone within
	// 6 m of their path, so a crowd standing on it would hold every wave at the door without meaning to.
	const FVector O = GetActorLocation();
	const float Xs[] = { -800.0f, 800.0f, -1000.0f, 1000.0f, -900.0f };
	const float Ys[] = { 1850.0f, 1850.0f, 1650.0f, 1650.0f, 1450.0f };
	for (int32 i = 0; i < 5; ++i)
	{
		Out.Add(FTransform(FRotator(0.0f, -90.0f, 0.0f), O + FVector(Xs[i], Ys[i], 140.0f)));
	}
}

// The map is the Abandoned Power Plant sample level, streamed in as a set (Arcade/DCCSets.h, Docs/kits.md), Claude
// 2026-11-25. It is a walled annex (solid end facade, a doorway in each long side at U 1250) running straight into a
// 32 x 22 m roofed hall. The road comes in from the yard through the annex's side doorway and snakes through the hall to the Wall at the far end; two towers stand in the
// annex and two in the hall; one camp is the annex's dead end, the other is out through its far doorway. Points were picked
// from DCC.SetProbe / DCC.SetProbeWalls maps of the set: U runs along the set (0 = the annex's open end is at U 300),
// V across it (the hall spans V 100-2300, the annex V 100-1500, a row of pillars stands at V 600, U 3000-3900).
namespace
{
	// Set coordinates (U along, V across, cm from the set's bounds corner) to arena offsets.
	FVector HallPoint(float U, float V, float Z = 0.0f)
	{
		return FVector(-(V - 1200.0f), U - 3000.0f, Z);
	}
}

void ADCCArcadeSiege::BuildLocal()
{
	const FVector O = GetActorLocation();
	auto P = [&](float X, float Y, float Z) { return O + FVector(X, Y, Z); };
	auto H = [&](float U, float V, float Z) { return O + HallPoint(U, V, Z); };
	if (Road.Num() == 0)
	{
		// In through the annex's side doorway (U 1250; its far end is a solid facade), across the annex, then a snake
		// through the hall to the Wall. Doorways are 2 m: marchers file in one at a time.
		Road = { H(1250, -1300, 0), H(1250, 100, 0), H(1250, 800, 0), H(2200, 450, 0), H(3000, 1900, 0), H(3900, 1300, 0),
			H(4600, 700, 0), H(5150, 1200, 0) };
		TowerTops = { H(700, 450, 280), H(1900, 1250, 280), H(3000, 1050, 280), H(4300, 2050, 280) };
		Posts = { H(900, 450, 40), H(1900, 950, 40), H(3000, 1400, 40), H(4300, 1750, 40) };
		Camps.SetNum(2);
		Camps[0].At = H(650, 1150, 0);   // the annex's dead end
		Camps[1].At = H(1000, 2350, 0);  // the yard nook through the annex's other doorway
	}

	// The hall. Turned 90 degrees so its length runs along the road; its own sun and sky are dropped for the arena's.
	DCCSets::FOptions HallOptions;
	// Its doors work (DCCSets' default): they open as a wave or a player walks up and shut behind them.
	// A lockdown (DriveLockdown) locks the annex's side door and turns that wave back to the yard.
	DCCSets::Load(GetWorld(), TEXT("PowerPlant"), O + FVector(1061.0f, -1853.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f),
		TEXT("DCCSiegeHall"), HallOptions);
	// The hall is dark inside: work lights down its length, warm over the road, cold at the Wall.
	Light(H(1000, 800, 650), FLinearColor(1.0f, 0.7f, 0.45f), 9000.0f, 1500.0f);
	Light(H(2700, 1200, 900), FLinearColor(1.0f, 0.7f, 0.45f), 14000.0f, 2000.0f);
	Light(H(4000, 1200, 900), FLinearColor(1.0f, 0.7f, 0.45f), 14000.0f, 2000.0f);
	Light(H(1250, -900, 700), FLinearColor(1.0f, 0.55f, 0.35f), 9000.0f, 2400.0f);

	// The yard outside, under the hall's floor so the two never fight.
	const FLinearColor Dirt(0.18f, 0.14f, 0.1f);
	Block(P(0, -1200, -40), FVector(9800, 9800, 50), Dirt);
	for (int32 i = 0; i < Road.Num() - 1; ++i)
	{
		const FVector Delta = Road[i + 1] - Road[i];
		Block((Road[i] + Road[i + 1]) * 0.5f + FVector(0, 0, 10), FVector(Delta.Size2D(), 460, 18),
			FLinearColor(0.1f, 0.09f, 0.08f), Delta.Rotation().Yaw);
	}
	if (AActor* Wall = Block(H(5330, 1200, 220), FVector(2200, 140, 460), FLinearColor(0.35f, 0.12f, 0.1f)))
	{
		Paint(Wall, TEXT("/Game/Synthwave_city/Material/MI_synthwave_wall1"));
	}
	Sign(H(5230, 1200, 420), -90.0f, TEXT("THE WALL"), FColor(255, 180, 120), 70.0f);
	Sign(H(5000, 2100, 160), -90.0f, TEXT("COMMAND"), FColor(220, 210, 190), 28.0f);
	Prop(TEXT("/Game/Ice_Station/Meshes/Computer/SM_computer_01"), H(5220, 2150, 0), -90.0f, FVector(90, 50, 70));
	Prop(TEXT("/Game/Ice_Station/Meshes/interior/SM_generator"), H(5200, 300, 0), 20.0f, FVector(140, 80, 100));
	Light(H(5100, 1200, 520), FLinearColor(1.0f, 0.55f, 0.3f), 8000.0f, 1600.0f);

	for (int32 i = 0; i < TowerTops.Num(); ++i)
	{
		const FVector Top = TowerTops[i];
		const FVector Post = Posts[i];
		for (int32 Step = 0; Step < 4; ++Step)
		{
			const FVector At = FMath::Lerp(Post, Top, Step / 3.0f);
			Block(FVector(At.X, At.Y, 40.0f + Step * 70.0f), FVector(240, 240, 50), FLinearColor(0.22f, 0.2f, 0.18f));
		}
		Block(Top, FVector(280, 280, 36), FLinearColor(0.2f, 0.4f, 0.62f));
		Shape(TEXT("Cone"), Top + FVector(0, 0, 70), FVector(36, 36, 80), FLinearColor(0.55f, 0.82f, 1.0f), FRotator::ZeroRotator, false);
		Sign(Top + FVector(0, -90, 150), 0.0f, TEXT("CRYSTAL"), FColor(180, 225, 255), 26.0f);
		Sign(Post + FVector(0, 90, 90), 0.0f, TEXT("POST"), FColor(255, 214, 140), 24.0f);
		Light(Top + FVector(0, 0, 180), FLinearColor(0.45f, 0.7f, 1.0f), 5000.0f, 800.0f);
		Prop(TEXT("/Game/Ice_Station/Meshes/Crates/SM_crate_01"), Post + FVector(160, 0, 0), 15.0f, FVector(70, 70, 70));
	}
	for (const FDCCSiegeCamp& Camp : Camps)
	{
		Block(Camp.At + FVector(0, 0, 8), FVector(700, 700, 16), FLinearColor(0.15f, 0.22f, 0.12f));
		Sign(Camp.At + FVector(0, -200, 140), 0.0f, TEXT("CAMP"), FColor(170, 220, 140), 30.0f);
		Prop(TEXT("/Game/Ice_Station/Meshes/Crates/SM_crate_01_group"), Camp.At + FVector(180, 80, 0), 30.0f, FVector(160, 120, 90));
		Light(Camp.At + FVector(0, 0, 220), FLinearColor(0.4f, 0.7f, 0.3f), 3000.0f, 700.0f);
	}

	FxCharge = LoadFx(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Lightning.NS_Free_Spells_Aura_Lightning"));
	FxRelease = LoadFx(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave.NS_Free_Spells_Shockwave"));
	FxImpact = LoadFx(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion.NS_Free_Spells_Explosion"));
}

UNiagaraSystem* ADCCArcadeSiege::LoadFx(const TCHAR* Path)
{
	return LoadObject<UNiagaraSystem>(nullptr, Path);
}

void ADCCArcadeSiege::BuildFallback(int32 Count)
{
	Waves.Reset();
	WallMax = 2000;
	for (int32 Wave = 1; Wave <= Count; ++Wave)
	{
		FWave Spec;
		Spec.Wave = Wave;
		Spec.Speed = 320.0f;
		Spec.Gap = 0.65f;
		const int32 Total = FMath::Max(1, FMath::RoundToInt(IncomingPerSecond(Wave) * 20.0f));
		const int32 N = 5 + Wave;
		const TCHAR* Cycle[] = { TEXT("debtor"), TEXT("clerk"), TEXT("hound"), TEXT("stalker"), TEXT("brute") };
		if (Wave % 10 == 0)
		{
			const int32 BossHp = FMath::Max(40, FMath::RoundToInt(Total * 0.4f));
			const int32 Fodder = FMath::Max(1, N - 1);
			FBody Boss;
			Boss.Kind = TEXT("boss");
			Boss.Name = TEXT("The House");
			Boss.Hp = BossHp;
			Boss.Count = 1;
			FBody Rest;
			Rest.Kind = Cycle[(Wave / 5) % 5];
			Rest.Name = TEXT("Debtor");
			Rest.Hp = FMath::Max(8, (Total - BossHp) / Fodder);
			Rest.Count = Fodder;
			Spec.Bodies.Add(Boss);
			Spec.Bodies.Add(Rest);
		}
		else
		{
			FBody Body;
			Body.Kind = Cycle[(Wave - 1) % 5];
			Body.Name = Body.Kind;
			Body.Hp = FMath::Max(8, Total / N);
			Body.Count = N;
			Spec.Bodies.Add(Body);
		}
		Waves.Add(Spec);
	}
}

void ADCCArcadeSiege::LoadWaves()
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("DCC/Siege/waves.json");
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		BuildFallback(40);
		WallHp = WallMax;
		return;
	}
	double Wall = 2000;
	Root->TryGetNumberField(TEXT("wall"), Wall);
	WallMax = FMath::Max(1, FMath::RoundToInt(Wall));
	WallHp = WallMax;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("waves"), Arr))
	{
		BuildFallback(40);
		return;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Arr)
	{
		const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Obj.IsValid())
		{
			continue;
		}
		FWave Spec;
		Spec.Wave = JsonInt(Obj, TEXT("wave"), Waves.Num() + 1);
		Spec.Speed = JsonInt(Obj, TEXT("speed"), 320);
		Spec.Gap = JsonInt(Obj, TEXT("gap"), 1) > 0 ? (float)JsonInt(Obj, TEXT("gap"), 1) : 0.65f;
		double Gap = 0.65;
		if (Obj->TryGetNumberField(TEXT("gap"), Gap))
		{
			Spec.Gap = (float)Gap;
		}
		const TArray<TSharedPtr<FJsonValue>>* Bodies = nullptr;
		if (Obj->TryGetArrayField(TEXT("bodies"), Bodies))
		{
			for (const TSharedPtr<FJsonValue>& BodyValue : *Bodies)
			{
				const TSharedPtr<FJsonObject> BodyObj = BodyValue.IsValid() ? BodyValue->AsObject() : nullptr;
				if (!BodyObj.IsValid())
				{
					continue;
				}
				FBody Body;
				Body.Kind = BodyObj->GetStringField(TEXT("kind"));
				Body.Name = BodyObj->GetStringField(TEXT("name"));
				Body.Hp = JsonInt(BodyObj, TEXT("hp"), 40);
				Body.Count = FMath::Max(1, JsonInt(BodyObj, TEXT("count"), 1));
				Spec.Bodies.Add(Body);
			}
		}
		if (Spec.Bodies.Num() > 0)
		{
			Waves.Add(Spec);
		}
	}
	if (Waves.Num() == 0)
	{
		BuildFallback(40);
	}
}

void ADCCArcadeSiege::ServerBegin()
{
	if (Road.Num() == 0)
	{
		BuildLocal();
	}
	LoadWaves();
	BossName = TEXT("THE WALL");
	BossHealth = 1.0f;
	NextWaveAt = ServerTime() + 3.0;
	Title = TEXT("DUNGEON CRAWLER TOWER DEFENSE");
	StatusLines = { TEXT("The road is quiet."), TEXT("The wall is up.") };
	UE_LOG(LogTemp, Log, TEXT("[DCC Siege] ready. %d waves, wall %d. DPS was not measured with DCC.Fight."), Waves.Num(), WallMax);
}

ADCCEnemyCharacter* ADCCArcadeSiege::SpawnKind(const FString& Kind, const FVector& At, int32 Hp, const FString& Name, bool bMarcher)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADCCEnemyCharacter* Enemy = World->SpawnActor<ADCCEnemyCharacter>(ClassForKind(Kind), At + FVector(0, 0, 100), FRotator(0.0f, 90.0f, 0.0f), Params);
	if (!Enemy)
	{
		return nullptr;
	}
	Enemy->DisplayName = FText::FromString(Name.IsEmpty() ? Kind : Name);
	Enemy->CorpseLifeSpan = bMarcher ? 0.2f : 4.0f;
	Enemy->bUseSkills = !bMarcher;
	if (Enemy->Stats)
	{
		Enemy->Stats->Level = FMath::Clamp(1 + Hp / 50, 1, 20);
		Enemy->Stats->RefreshDerivedStats(false);
		Enemy->Stats->MaxHitPoints = FMath::Max(1, Hp);
		Enemy->Stats->CurrentHitPoints = Enemy->Stats->MaxHitPoints;
	}
	Enemy->OnCharacterDied.AddDynamic(this, &ADCCArcadeSiege::OnFighterDied);
	return Enemy;
}

void ADCCArcadeSiege::StartWave(int32 Wave)
{
	if (bEnded || Wave < 1)
	{
		return;
	}
	const FWave* Spec = nullptr;
	for (const FWave& One : Waves)
	{
		if (One.Wave == Wave)
		{
			Spec = &One;
			break;
		}
	}
	FWave Extra;
	if (!Spec)
	{
		Extra.Wave = Wave;
		Extra.Speed = 320.0f;
		Extra.Gap = 0.6f;
		const int32 Total = FMath::Max(1, FMath::RoundToInt(IncomingPerSecond(Wave) * 20.0f));
		const int32 N = 5 + Wave;
		FBody Body;
		Body.Kind = TEXT("debtor");
		Body.Name = TEXT("Debtor");
		Body.Hp = FMath::Max(8, Total / N);
		Body.Count = N;
		Extra.Bodies.Add(Body);
		Spec = &Extra;
	}
	CurrentWave = Wave;
	WaveStartedAt = ServerTime();
	bWaveArmed = true;
	Queue.Reset();
	for (const FBody& Body : Spec->Bodies)
	{
		for (int32 i = 0; i < Body.Count; ++i)
		{
			FDCCSiegeSpawn Spawn;
			Spawn.Kind = Body.Kind;
			Spawn.Name = Body.Count > 1 && i > 0 ? Body.Name : Body.Name;
			Spawn.Hp = Body.Hp;
			Spawn.Wave = Wave;
			Spawn.Speed = Spec->Speed;
			Spawn.Gap = Spec->Gap;
			Queue.Add(Spawn);
		}
	}
	NextSpawnAt = ServerTime() + 0.35;
	Multicast_Announce(Wave % 10 == 0 ? TEXT("Boss") : TEXT("Wave"));
	if (Wave % 10 == 0)
	{
		DCCNews::Post(GetWorld(), FString::Printf(TEXT("Siege wave %d is on the road."), Wave));
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Siege] wave %d  bodies %d"), Wave, Queue.Num());
}

void ADCCArcadeSiege::SpawnNext()
{
	if (Queue.Num() == 0 || Road.Num() == 0)
	{
		return;
	}
	const FDCCSiegeSpawn Spawn = Queue[0];
	Queue.RemoveAt(0);
	NextSpawnAt = ServerTime() + FMath::Max(0.25f, Spawn.Gap);
	ADCCEnemyCharacter* Enemy = SpawnKind(Spawn.Kind, Road[0], Spawn.Hp, Spawn.Name, true);
	if (!Enemy)
	{
		return;
	}
	Enemy->bSiegeMarch = true;
	Enemy->bSiegeFight = false;
	Enemy->SiegeMarchIndex = 1;
	Enemy->SiegeWave = Spawn.Wave;
	Enemy->SiegeMarchSpeed = Spawn.Speed;
	Enemy->bSiegePaid = false;
	if (USkeletalMeshComponent* Mesh = Enemy->GetMesh())
	{
		const float Scale = Spawn.Wave >= 10 ? FMath::Min(2.0f, 1.0f + 0.03f * (Spawn.Wave - 10)) : 1.0f;
		Mesh->SetRelativeScale3D(Mesh->GetRelativeScale3D() * Scale);
	}
	Marchers.Add(Enemy);
}

void ADCCArcadeSiege::ServerTick(float DeltaSeconds)
{
	if (bEnded || bRoundOver)
	{
		return;
	}
	const double Now = ServerTime();
	if (Queue.Num() > 0 && Now >= NextSpawnAt && !bRouteLocked)
	{
		SpawnNext();
	}
	DriveLockdown(Now);
	SyncRouteDoors();

	bool bLive = false;
	for (const TWeakObjectPtr<ADCCEnemyCharacter>& Marcher : Marchers)
	{
		if (Marcher.IsValid() && Marcher->IsAlive())
		{
			bLive = true;
			break;
		}
	}
	if (bWaveArmed && Queue.Num() == 0 && !bLive)
	{
		WavesCleared = CurrentWave;
		bWaveArmed = false;
		NextWaveAt = Now + 3.0;
		if (CurrentWave > 0 && CurrentWave % 10 == 0)
		{
			Multicast_Announce(TEXT("Good"));
		}
	}
	else if (bWaveArmed && Queue.Num() == 0 && Now - WaveStartedAt > 25.0)
	{
		StartWave(CurrentWave + 1);
	}
	else if (!bWaveArmed && Now >= NextWaveAt)
	{
		StartWave(FMath::Max(1, CurrentWave + 1));
	}

	DriveMarchers(DeltaSeconds);
	DriveCrystal(DeltaSeconds);
	DriveCamps();
	UpdateBuffScales();
	if (WallMax > 0 && WallHp > 0 && WallHp * 100 / WallMax <= 30 && !bWallWarned)
	{
		bWallWarned = true;
		Multicast_Announce(TEXT("Wall"));
		DCCNews::Post(GetWorld(), TEXT("The wall is under attack."));
	}
	if (WallHp <= 0)
	{
		EndRound();
		return;
	}
	RefreshStatus();
}

bool ADCCArcadeSiege::PlayerNearRoad(const ADCCEnemyCharacter* Marcher) const
{
	if (!Marcher || Road.Num() == 0)
	{
		return false;
	}
	const int32 Index = FMath::Clamp(Marcher->SiegeMarchIndex, 0, Road.Num() - 1);
	const FVector A = Marcher->GetActorLocation();
	const FVector B = Road[Index];
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const FVector At = Pawn->GetActorLocation();
		if (FVector::Dist2D(At, A) > 600.0f)
		{
			continue;
		}
		const FVector AB = B - A;
		const float Len2 = AB.SizeSquared2D();
		float T = 0.0f;
		if (Len2 > 1.0f)
		{
			T = FMath::Clamp(FVector::DotProduct(FVector(At.X - A.X, At.Y - A.Y, 0), FVector(AB.X, AB.Y, 0)) / Len2, 0.0f, 1.0f);
		}
		if (FVector::Dist2D(At, A + AB * T) <= 300.0f)
		{
			return true;
		}
	}
	return false;
}

void ADCCArcadeSiege::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeSiege, bRouteLocked);
}

void ADCCArcadeSiege::ClientTick(float DeltaSeconds)
{
	Super::ClientTick(DeltaSeconds);
	SyncRouteDoors();
}

void ADCCArcadeSiege::DriveLockdown(double Now)
{
	if (Road.Num() < 2 || bEnded)
	{
		return;
	}
	if (bRouteLocked)
	{
		if (Now >= LockdownUntil)
		{
			bRouteLocked = false;
			DCCNews::Post(GetWorld(), TEXT("The side door is open."));
			UE_LOG(LogTemp, Log, TEXT("[DCC Siege] lockdown over"));
		}
		return;
	}
	// Waves 2, 6, 10, 14. Once each, when the first body reaches the side door.
	if (!bWaveArmed || CurrentWave < 2 || (CurrentWave % 4) != 2 || LockdownWave == CurrentWave)
	{
		return;
	}
	const FVector DoorAt = Road[1];
	bool bClose = false;
	for (const TWeakObjectPtr<ADCCEnemyCharacter>& Marcher : Marchers)
	{
		const ADCCEnemyCharacter* Body = Marcher.Get();
		if (!Body || !Body->IsAlive() || Body->SiegeMarchIndex > 1)
		{
			continue;
		}
		if (FVector::Dist2D(Body->GetActorLocation(), DoorAt) < 700.0f)
		{
			bClose = true;
			break;
		}
	}
	if (!bClose)
	{
		return;
	}
	LockdownWave = CurrentWave;
	LockdownUntil = Now + 15.0;
	bRouteLocked = true;
	UE_LOG(LogTemp, Log, TEXT("[DCC Siege] lockdown. side door, 15 s"));
	DCCNews::Post(GetWorld(), TEXT("LOCKDOWN. The side door is staying shut."));
}

void ADCCArcadeSiege::SyncRouteDoors()
{
	UWorld* World = GetWorld();
	if (!World || Road.Num() < 2)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (bRouteLocked)
	{
		if (Now < NextLockLook)
		{
			return;
		}
		NextLockLook = Now + 0.5f;
		const int32 N = DCCSets::LockDoorsNear(World, Road[1], 650.0f, true);
		if (N > 0 && !bDoorsLatched)
		{
			bDoorsLatched = true;
			UE_LOG(LogTemp, Log, TEXT("[DCC Siege] locked %d doors by the side door"), N);
		}
		return;
	}
	if (bDoorsLatched)
	{
		const int32 N = DCCSets::LockDoorsNear(World, Road[1], 650.0f, false);
		bDoorsLatched = false;
		NextLockLook = 0.0f;
		UE_LOG(LogTemp, Log, TEXT("[DCC Siege] opened %d doors by the side door"), N);
	}
}

void ADCCArcadeSiege::DriveMarchers(float DeltaSeconds)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (int32 i = Marchers.Num() - 1; i >= 0; --i)
	{
		ADCCEnemyCharacter* Marcher = Marchers[i].Get();
		if (!Marcher || !Marcher->IsAlive())
		{
			Marchers.RemoveAt(i);
			continue;
		}
		const bool bFight = PlayerNearRoad(Marcher);
		Marcher->bSiegeFight = bFight;
		const bool bFreeze = Now < Marcher->SiegeFreezeUntil;
		const bool bSlow = Now < Marcher->SiegeSlowUntil;
		if (bFight)
		{
			Marcher->SetMovementSpeedOverride(bFreeze ? 1.0f : bSlow ? 160.0f : 0.0f);
			continue;
		}
		if (AAIController* AI = Cast<AAIController>(Marcher->GetController()))
		{
			AI->StopMovement();
		}
		Marcher->ForgetTarget();
		// Still outside the side door: walk back to the yard until the lockdown ends.
		if (bRouteLocked && Marcher->SiegeMarchIndex <= 1 && Road.Num() > 0)
		{
			FVector Back = Road[0] - Marcher->GetActorLocation();
			Back.Z = 0.0f;
			if (Back.Size() < 160.0f)
			{
				Marcher->SetMovementSpeedOverride(1.0f);
				continue;
			}
			float Speed = Marcher->SiegeMarchSpeed;
			if (bFreeze) Speed = 1.0f;
			else if (bSlow) Speed *= 0.6f;
			Marcher->SetMovementSpeedOverride(Speed);
			Marcher->AddMovementInput(Back.GetSafeNormal(), 1.0f);
			Marcher->SetActorRotation(FRotator(0.0f, Back.Rotation().Yaw, 0.0f));
			continue;
		}
		if (Marcher->SiegeMarchIndex >= Road.Num())
		{
			if (Marcher->Stats)
			{
				Marcher->SiegeSoak += FMath::Max(1.0f, Marcher->Stats->CurrentHitPoints / 4.0f) * DeltaSeconds;
				const int32 Dump = FMath::FloorToInt(Marcher->SiegeSoak);
				if (Dump > 0)
				{
					Marcher->SiegeSoak -= Dump;
					WallHp = FMath::Max(0, WallHp - Dump);
					BossHealth = WallMax > 0 ? (float)WallHp / WallMax : 0.0f;
					const TWeakObjectPtr<ADCCCharacterBase> Last = Marcher->SiegeLastHit;
					Marcher->Stats->ApplyDamage(Dump);
					Marcher->SiegeLastHit = Last;
				}
			}
			continue;
		}
		const FVector Next = Road[Marcher->SiegeMarchIndex];
		FVector To = Next - Marcher->GetActorLocation();
		To.Z = 0.0f;
		if (To.Size() < 120.0f)
		{
			Marcher->SiegeMarchIndex++;
			continue;
		}
		float Speed = Marcher->SiegeMarchSpeed;
		if (bFreeze) Speed = 1.0f;
		else if (bSlow) Speed *= 0.6f;
		Marcher->SetMovementSpeedOverride(Speed);
		Marcher->AddMovementInput(To.GetSafeNormal(), 1.0f);
		Marcher->SetActorRotation(FRotator(0.0f, To.Rotation().Yaw, 0.0f));
	}
}

int32 ADCCArcadeSiege::NearestTower(const FVector& At, float Radius) const
{
	int32 Best = INDEX_NONE;
	float BestD = Radius;
	for (int32 i = 0; i < TowerTops.Num(); ++i)
	{
		if (FMath::Abs(At.Z - TowerTops[i].Z) > 140.0f)
		{
			continue;
		}
		const float D = FVector::Dist2D(At, TowerTops[i]);
		if (D < BestD)
		{
			BestD = D;
			Best = i;
		}
	}
	return Best;
}

int32 ADCCArcadeSiege::NearestPost(const FVector& At, float Radius) const
{
	int32 Best = INDEX_NONE;
	float BestD = Radius;
	for (int32 i = 0; i < Posts.Num(); ++i)
	{
		if (FMath::Abs(At.Z - Posts[i].Z) > 180.0f)
		{
			continue;
		}
		const float D = FVector::Dist2D(At, Posts[i]);
		if (D < BestD)
		{
			BestD = D;
			Best = i;
		}
	}
	return Best;
}

bool ADCCArcadeSiege::CanUse(const ADCCCharacterBase* Who) const
{
	if (!Who)
	{
		return false;
	}
	const FVector At = Who->GetActorLocation();
	return NearestTower(At, 220.0f) != INDEX_NONE || NearestPost(At, 200.0f) != INDEX_NONE;
}

FDCCSiegeCaster* ADCCArcadeSiege::FindCaster(ADCCCharacterBase* Who, bool bCreate)
{
	for (FDCCSiegeCaster& Caster : Casters)
	{
		if (Caster.Who.Get() == Who)
		{
			return &Caster;
		}
	}
	if (!bCreate || !Who)
	{
		return nullptr;
	}
	FDCCSiegeCaster& Added = Casters.AddDefaulted_GetRef();
	Added.Who = Who;
	return &Added;
}

void ADCCArcadeSiege::Use(ADCCCharacterBase* Who)
{
	if (!Who || bEnded)
	{
		return;
	}
	const FVector At = Who->GetActorLocation();
	const int32 Post = NearestPost(At, 200.0f);
	const int32 Tower = NearestTower(At, 220.0f);
	const bool bPostCloser = Post != INDEX_NONE && (Tower == INDEX_NONE || FVector::Dist2D(At, Posts[Post]) < FVector::Dist2D(At, TowerTops[Tower]));
	if (bPostCloser)
	{
		for (TActorIterator<ADCCEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (It->bIsAlly && It->bHoldGround && FVector::Dist2D(It->GetActorLocation(), Posts[Post]) < 180.0f)
			{
				It->Destroy();
				Who->ShowLocalNotice(FText::FromString(TEXT("Recalled.")));
				return;
			}
		}
		for (TActorIterator<ADCCEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->bIsAlly || It->bSiegeMarch || !It->IsAlive())
			{
				continue;
			}
			if (FVector::Dist2D(It->GetActorLocation(), At) > 800.0f)
			{
				continue;
			}
			It->SetActorLocation(Posts[Post] + FVector(0, 0, 100));
			It->bHoldGround = true;
			It->AggroRadius = 2200.0f;
			It->PostedBy = Who;
			It->bThrowsRocks = true;
			if (It->Skills)
			{
				FDCCSkillDef Def;
				for (const FDCCKnownSkill& Known : It->Skills->GetKnownSkills())
				{
					if (It->Skills->FindSkill(Known.SkillId, Def) && Def.Archetype == EDCCSkillArchetype::Damage)
					{
						It->bThrowsRocks = false;
						break;
					}
				}
			}
			Who->ShowLocalNotice(FText::FromString(TEXT("Posted.")));
			return;
		}
		Who->ShowLocalNotice(FText::FromString(TEXT("Bring someone to the post.")));
		return;
	}
	if (Tower == INDEX_NONE)
	{
		return;
	}
	FDCCSiegeCaster* Caster = FindCaster(Who, true);
	if (!Caster)
	{
		return;
	}
	const double Now = ServerTime();
	if (Caster->bChanneling)
	{
		if (Caster->Charge >= 0.4f)
		{
			FireCrystal(*Caster);
		}
		return;
	}
	if (Now < Caster->ReadyAt)
	{
		Who->ShowLocalNotice(FText::FromString(TEXT("The crystal is warm.")));
		return;
	}
	Caster->bChanneling = true;
	Caster->Charge = 0.0f;
	Caster->Tower = Tower;
	Multicast_CrystalCharge(Who);
}

void ADCCArcadeSiege::DriveCrystal(float DeltaSeconds)
{
	for (FDCCSiegeCaster& Caster : Casters)
	{
		if (!Caster.bChanneling)
		{
			continue;
		}
		ADCCCharacterBase* Who = Caster.Who.Get();
		const bool bOnTower = Who && Caster.Tower != INDEX_NONE && NearestTower(Who->GetActorLocation(), 260.0f) == Caster.Tower;
		const bool bJumped = Who && Who->GetVelocity().Z > 180.0f;
		if (!Who || !Who->IsAlive() || !bOnTower || bJumped)
		{
			Caster.bChanneling = false;
			Caster.Charge = 0.0f;
			continue;
		}
		Caster.Charge += DeltaSeconds;
		if (Caster.Charge >= 3.0f)
		{
			FireCrystal(Caster);
		}
	}
}

void ADCCArcadeSiege::FireCrystal(FDCCSiegeCaster& Caster)
{
	ADCCCharacterBase* Who = Caster.Who.Get();
	Caster.bChanneling = false;
	Caster.ReadyAt = ServerTime() + 1.5;
	const float Charge = Caster.Charge;
	Caster.Charge = 0.0f;
	if (!Who || !GetWorld() || Caster.Tower == INDEX_NONE || !TowerTops.IsValidIndex(Caster.Tower))
	{
		return;
	}
	const FVector From = TowerTops[Caster.Tower];
	const FVector Start = Who->GetPawnViewLocation();
	const FVector End = Start + Who->GetControlRotation().Vector() * 5000.0f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SiegeCrystal), false, Who);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	FVector At = bHit ? FVector(Hit.ImpactPoint) : End;
	if (FVector::Dist(At, From) > 2500.0f)
	{
		At = From + (At - From).GetSafeNormal() * 2500.0f;
	}
	int32 Die = 8;
	float Radius = 280.0f;
	FDCCSkillDef Def;
	if (Who->Skills && Who->Skills->FindSkill(Who->GetSelectedSkillId(), Def) && Def.Archetype == EDCCSkillArchetype::Damage)
	{
		Die = FMath::Max(1, Def.PowerDie);
		Radius = FMath::Max(180.0f, Def.Radius * 1.5f);
	}
	const float Power = 2.5f * FMath::Clamp(Charge / 3.0f, 0.45f, 1.0f);
	const int32 Magnitude = FMath::Max(1, FMath::RoundToInt((FMath::RandRange(1, Die) + 4) * Power));
	for (TActorIterator<ADCCEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || It->bIsAlly || FVector::Dist(It->GetActorLocation(), At) > Radius)
		{
			continue;
		}
		It->SiegeLastHit = Who;
		if (It->Stats)
		{
			It->Stats->ApplyDamage(Magnitude);
		}
		Who->NoteDamageDealtTo(*It, Magnitude);
	}
	Multicast_CrystalFire(From + FVector(0, 0, 80), At);
}

void ADCCArcadeSiege::Multicast_CrystalCharge_Implementation(ADCCCharacterBase* Who)
{
	if (!Who)
	{
		return;
	}
	if (FxCharge && Who->GetMesh())
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(FxCharge, Who->GetMesh(), NAME_None, FVector(0, 0, 140),
			FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	else
	{
		Shape(TEXT("Sphere"), Who->GetActorLocation() + FVector(0, 0, 140), FVector(48, 48, 48), FLinearColor(0.5f, 0.8f, 1.0f), FRotator::ZeroRotator, false);
	}
}

void ADCCArcadeSiege::Multicast_CrystalFire_Implementation(FVector From, FVector At)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		if (!It->GetMesh())
		{
			continue;
		}
		TArray<USceneComponent*> MeshChildren;
		It->GetMesh()->GetChildrenComponents(false, MeshChildren);
		for (USceneComponent* Child : MeshChildren)
		{
			UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Child);
			if (Niagara && Niagara->GetAsset() && Niagara->GetAsset()->GetName().Contains(TEXT("Aura")))
			{
				Niagara->DestroyComponent();
			}
		}
	}
	if (FxRelease)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, FxRelease, From);
	}
	if (FxImpact)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, FxImpact, At);
	}
	if (!FxImpact)
	{
		Shape(TEXT("Sphere"), At, FVector(90, 90, 90), FLinearColor(0.6f, 0.85f, 1.0f), FRotator::ZeroRotator, false);
	}
}

void ADCCArcadeSiege::Multicast_Announce_Implementation(const FString& Moment)
{
	DCCAnnouncer::PlayLocal(this, Moment);
}

void ADCCArcadeSiege::DriveCamps()
{
	for (FDCCSiegeCamp& Camp : Camps)
	{
		if (Camp.Live.IsValid() && Camp.Live->IsAlive())
		{
			continue;
		}
		if (CurrentWave < Camp.DeadOnWave + 5)
		{
			continue;
		}
		ADCCEnemyCharacter* Beast = SpawnKind(TEXT("thornback"), Camp.At, 220, TEXT("Camp"), false);
		if (!Beast)
		{
			continue;
		}
		Beast->bSiegeCamp = true;
		Beast->bPassiveUntilProvoked = true;
		Beast->AggroRadius = 600.0f;
		Beast->LeashRadius = 900.0f;
		Camp.Live = Beast;
	}
}

void ADCCArcadeSiege::OnCampDown(ADCCEnemyCharacter* Victim)
{
	for (FDCCSiegeCamp& Camp : Camps)
	{
		if (Camp.Live.Get() == Victim)
		{
			Camp.DeadOnWave = FMath::Max(1, CurrentWave);
			Camp.Clears++;
			break;
		}
	}
	ADCCCharacterBase* Who = Victim ? Victim->SiegeLastHit.Get() : nullptr;
	const int32 Which = CampGrant++ % 4;
	GrantBuff(Who, FName(BuffNames[Which]));
	if (Who)
	{
		Who->ShowLocalNotice(FText::FromString(BuffLines[Which]));
	}
}

void ADCCArcadeSiege::GrantBuff(ADCCCharacterBase* Who, FName Buff)
{
	FDCCSiegeCaster* Caster = FindCaster(Who, true);
	if (!Caster)
	{
		return;
	}
	Caster->Buff = Buff;
	Caster->BuffUntilWave = CurrentWave + 5;
}

bool ADCCArcadeSiege::HasBuff(const ADCCCharacterBase* Who, FName Buff) const
{
	for (const FDCCSiegeCaster& Caster : Casters)
	{
		if (Caster.Who.Get() == Who && Caster.Buff == Buff && CurrentWave <= Caster.BuffUntilWave)
		{
			return true;
		}
	}
	return false;
}

void ADCCArcadeSiege::ScaleHands(ADCCCharacterBase* Who, float Mul)
{
	if (!Who || !Who->GetMesh() || FMath::IsNearlyEqual(Mul, 1.0f))
	{
		return;
	}
	TArray<USceneComponent*> MeshChildren;
	Who->GetMesh()->GetChildrenComponents(false, MeshChildren);
	for (USceneComponent* Child : MeshChildren)
	{
		if (!Child || !Child->IsA<UStaticMeshComponent>())
		{
			continue;
		}
		const FName Socket = Child->GetAttachSocketName();
		if (Socket == TEXT("hand_rSocket") || Socket == TEXT("hand_lSocket"))
		{
			Child->SetRelativeScale3D(Child->GetRelativeScale3D() * Mul);
		}
	}
}

void ADCCArcadeSiege::UpdateBuffScales()
{
	for (FDCCSiegeCaster& Caster : Casters)
	{
		ADCCCharacterBase* Who = Caster.Who.Get();
		const bool bWant = Who && HasBuff(Who, TEXT("Large"));
		if (bWant && !Caster.bLargeMeshes)
		{
			ScaleHands(Who, 3.0f);
			Caster.bLargeMeshes = true;
		}
		else if (!bWant && Caster.bLargeMeshes)
		{
			ScaleHands(Who, 1.0f / 3.0f);
			Caster.bLargeMeshes = false;
		}
	}
}

void ADCCArcadeSiege::NoteHit(ADCCEnemyCharacter* Victim, ADCCCharacterBase* By)
{
	if (!Victim || !By)
	{
		return;
	}
	ADCCCharacterBase* Credit = By->IsPlayerControlled() ? By : nullptr;
	if (!Credit)
	{
		if (const ADCCEnemyCharacter* Ally = Cast<ADCCEnemyCharacter>(By))
		{
			Credit = Ally->PostedBy.Get();
		}
	}
	if (Credit && Credit->IsPlayerControlled())
	{
		Victim->SiegeLastHit = Credit;
	}
}

void ADCCArcadeSiege::ApplyCold(ADCCEnemyCharacter* Victim)
{
	if (!Victim || !Victim->bSiegeMarch || !Victim->GetWorld())
	{
		return;
	}
	Victim->SiegeHits++;
	const double Now = Victim->GetWorld()->GetTimeSeconds();
	if (Victim->SiegeHits % 5 == 0)
	{
		Victim->SiegeFreezeUntil = Now + 1.0;
	}
	else
	{
		Victim->SiegeSlowUntil = Now + 2.0;
	}
}

void ADCCArcadeSiege::RollHeldLoot()
{
	FDCCLootDrop Gear;
	Gear.PickupClass = ADCCPickup_Equipment::StaticClass();
	Gear.Chance = 0.45f;
	Gear.EquipmentItemId = TEXT("TenderiserMallet");
	FDCCLootDrop Coins;
	Coins.PickupClass = ADCCPickup_Coins::StaticClass();
	Coins.Chance = 1.0f;
	Coins.MinGold = 12;
	Coins.MaxGold = 28;
	TArray<FDCCLootDrop> Table = { Gear, Coins };
	TArray<FDCCRolledLoot> Rolled;
	DCCLoot::RollLootTable(Table, Rolled);
	Held.Append(Rolled);
}

void ADCCArcadeSiege::PayKill(ADCCEnemyCharacter* Victim)
{
	if (!Victim)
	{
		return;
	}
	ADCCCharacterBase* Who = Victim->SiegeLastHit.Get();
	APlayerState* State = Who ? Who->GetPlayerState() : nullptr;
	const int32 Wave = FMath::Max(1, Victim->SiegeWave);
	int32 Tickets = Wave >= 20 ? 4 : Wave >= 10 ? 3 : 2;
	if (Who && HasBuff(Who, TEXT("Commission")))
	{
		Tickets *= 2;
	}
	if (State)
	{
		int32 Index = INDEX_NONE;
		for (int32 i = 0; i < Scores.Num(); ++i)
		{
			if (Scores[i].Who.Get() == State)
			{
				Index = i;
				break;
			}
		}
		if (Index == INDEX_NONE)
		{
			FDCCSiegeScore Score;
			Score.Who = State;
			Index = Scores.Add(Score);
		}
		Scores[Index].Kills++;
		Scores[Index].Tickets += Tickets;
		if (Wave >= 10)
		{
			Scores[Index].Salvage++;
		}
		UDCCWalletComponent::AwardTickets(State, Tickets, FString::Printf(TEXT("wave %d"), Wave));
	}
	if (Wave >= 20)
	{
		RollHeldLoot();
	}
}

void ADCCArcadeSiege::OnFighterDied(ADCCCharacterBase* Who)
{
	ADCCEnemyCharacter* Enemy = Cast<ADCCEnemyCharacter>(Who);
	if (!Enemy || Enemy->bSiegePaid)
	{
		return;
	}
	Enemy->bSiegePaid = true;
	if (Enemy->bSiegeCamp)
	{
		OnCampDown(Enemy);
	}
	else if (Enemy->bSiegeMarch)
	{
		PayKill(Enemy);
	}
}

void ADCCArcadeSiege::EndRound()
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;
	WallHp = 0;
	BossHealth = 0.0f;
	Multicast_Announce(TEXT("Lose"));
	DCCNews::Post(GetWorld(), FString::Printf(TEXT("The wall fell. Wave %d was the last one cleared."), WavesCleared));
	const FVector Drop = Road.Num() ? Road.Last() + FVector(0, -200, 40) : GetActorLocation();
	for (const FDCCRolledLoot& Loot : Held)
	{
		DCCLoot::SpawnRolledLoot(GetWorld(), Loot, Drop, nullptr);
	}
	Held.Reset();
	StatusLines.Reset();
	StatusLines.Add(TEXT("THE WALL FELL"));
	StatusLines.Add(FString::Printf(TEXT("Waves cleared: %d"), WavesCleared));
	for (const FDCCSiegeScore& Score : Scores)
	{
		APlayerState* State = Score.Who.Get();
		if (!State)
		{
			continue;
		}
		if (Score.Salvage > 0)
		{
			DCCProfile::Add(State, TEXT("Siege"), TEXT("Salvage"), Score.Salvage);
		}
		DCCProfile::RecordBest(State, TEXT("Siege"), TEXT("BestWave"), (float)WavesCleared, true);
		const int32 Bonus = WavesCleared * 5;
		if (Bonus > 0)
		{
			UDCCWalletComponent::AwardTickets(State, Bonus, TEXT("waves cleared"));
		}
		if (WavesCleared >= 5)
		{
			DCCArcade::CompleteSeriesQuest(State, TEXT("Siege"), DCCArcade::SeriesQuestXp());
		}
		StatusLines.Add(FString::Printf(TEXT("%s  %d kills  %d tickets  salvage %d"),
			*State->GetPlayerName(), Score.Kills, Score.Tickets + Bonus, Score.Salvage));
	}
	if (Scores.Num() == 0)
	{
		StatusLines.Add(TEXT("Nobody took a kill."));
	}
	ReturnToLobbyIn(22.0f);
}

void ADCCArcadeSiege::RefreshStatus()
{
	if (bEnded)
	{
		return;
	}
	BossHealth = WallMax > 0 ? FMath::Clamp((float)WallHp / WallMax, 0.0f, 1.0f) : 0.0f;
	StatusLines.Reset();
	StatusLines.Add(FString::Printf(TEXT("Wave %d"), FMath::Max(1, CurrentWave)));
	StatusLines.Add(FString::Printf(TEXT("Wall %d / %d"), WallHp, WallMax));
	if (bRouteLocked)
	{
		StatusLines.Add(TEXT("!LOCKDOWN. The side door is staying shut."));
	}
	for (const FDCCSiegeCaster& Caster : Casters)
	{
		if (Caster.bChanneling)
		{
			StatusLines.Add(FString::Printf(TEXT("Crystal %0.1f / 3"), Caster.Charge));
			break;
		}
	}
	for (const FDCCSiegeCaster& Caster : Casters)
	{
		if (!Caster.Buff.IsNone() && CurrentWave <= Caster.BuffUntilWave && Caster.Who.IsValid())
		{
			StatusLines.Add(FString::Printf(TEXT("%s: %s"), *Caster.Who->GetName(), *Caster.Buff.ToString()));
		}
	}
}

void ADCCArcadeSiege::DevWave(int32 Wave)
{
	for (const TWeakObjectPtr<ADCCEnemyCharacter>& Marcher : Marchers)
	{
		if (Marcher.IsValid())
		{
			Marcher->Destroy();
		}
	}
	Marchers.Reset();
	Queue.Reset();
	bWaveArmed = false;
	StartWave(FMath::Max(1, Wave));
}

void ADCCArcadeSiege::DevWall(int32 Hp)
{
	WallHp = FMath::Clamp(Hp, 0, WallMax);
	BossHealth = WallMax > 0 ? (float)WallHp / WallMax : 0.0f;
}

void ADCCArcadeSiege::DevCamp(int32 Index)
{
	if (!Camps.IsValidIndex(Index))
	{
		return;
	}
	FDCCSiegeCamp& Camp = Camps[Index];
	if (Camp.Live.IsValid() && Camp.Live->IsAlive() && Camp.Live->Stats)
	{
		Camp.Live->Stats->ApplyDamage(100000);
	}
	else
	{
		Camp.DeadOnWave = CurrentWave - 5;
	}
}

void ADCCArcadeSiege::DevPost()
{
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	ADCCCharacterBase* Who = Players.Num() && Players[0] ? Cast<ADCCCharacterBase>(Players[0]->GetPawn()) : nullptr;
	const FVector At = Posts.Num() ? Posts[0] : GetActorLocation();
	ADCCEnemyCharacter* Ally = SpawnKind(TEXT("clerk"), At, 80, TEXT("Posted"), false);
	if (!Ally)
	{
		return;
	}
	Ally->bIsAlly = true;
	Ally->bHoldGround = true;
	Ally->AggroRadius = 2200.0f;
	Ally->PostedBy = Who;
	Ally->bThrowsRocks = true;
	if (Ally->Skills && Ally->Skills->GrantSkill(TEXT("StaticBurst"), 1))
	{
		Ally->bThrowsRocks = false;
	}
	else if (Who && Who->Wallet)
	{
		Who->Wallet->Tell(TEXT("I don't know any spells. I brought a rock."));
		Ally->bSaidRock = true;
	}
}

namespace DCCSiegeHooks
{
	int32 AdjustMelee(ADCCCharacterBase* Attacker, ADCCCharacterBase* Target, int32 Damage)
	{
		if (Damage <= 0 || !Target)
		{
			return Damage;
		}
		ADCCArcadeSiege* Siege = ADCCArcadeSiege::Get(Target->GetWorld());
		if (!Siege)
		{
			return Damage;
		}
		if (const ADCCEnemyCharacter* Marcher = Cast<ADCCEnemyCharacter>(Attacker))
		{
			if (Marcher->bSiegeMarch && Marcher->SiegeWave >= 10)
			{
				const float Scale = FMath::Min(2.5f, 1.0f + 0.05f * (Marcher->SiegeWave - 10));
				Damage = FMath::Max(1, FMath::RoundToInt(Damage * Scale));
			}
		}
		if (Siege->HasBuff(Attacker, TEXT("Large")))
		{
			Damage = FMath::Max(1, FMath::RoundToInt(Damage * 1.6f));
		}
		if (ADCCEnemyCharacter* Victim = Cast<ADCCEnemyCharacter>(Target))
		{
			if (Siege->HasBuff(Attacker, TEXT("Cold")))
			{
				Siege->ApplyCold(Victim);
			}
			Siege->NoteHit(Victim, Attacker);
		}
		return Damage;
	}

	int32 AdjustSpell(ADCCCharacterBase* Caster, ADCCCharacterBase* Target, int32 Magnitude)
	{
		if (Magnitude <= 0 || !Target)
		{
			return Magnitude;
		}
		ADCCArcadeSiege* Siege = ADCCArcadeSiege::Get(Target->GetWorld());
		if (!Siege)
		{
			return Magnitude;
		}
		if (Siege->HasBuff(Caster, TEXT("Amplified")))
		{
			Magnitude = FMath::Max(1, FMath::RoundToInt(Magnitude * 1.4f));
		}
		if (ADCCEnemyCharacter* Victim = Cast<ADCCEnemyCharacter>(Target))
		{
			if (Siege->HasBuff(Caster, TEXT("Cold")))
			{
				Siege->ApplyCold(Victim);
			}
			Siege->NoteHit(Victim, Caster);
		}
		return Magnitude;
	}
}

static void SiegeWaveCmd(const TArray<FString>& Args, UWorld* World)
{
	const int32 Wave = Args.Num() ? FCString::Atoi(*Args[0]) : 1;
	const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
	auto Go = [Wave](UWorld* InWorld)
	{
		if (ADCCArcadeSiege* Siege = ADCCArcadeSiege::Get(InWorld))
		{
			Siege->DevWave(Wave);
		}
	};
	if (!World || Delay <= 0.0f)
	{
		Go(World);
		return;
	}
	FTimerHandle Handle;
	TWeakObjectPtr<UWorld> Weak(World);
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Go]()
	{
		if (Weak.IsValid())
		{
			Go(Weak.Get());
		}
	}), Delay, false);
}
static void SiegeWallCmd(const TArray<FString>& Args, UWorld* World)
{
	if (ADCCArcadeSiege* Siege = ADCCArcadeSiege::Get(World))
	{
		Siege->DevWall(Args.Num() ? FCString::Atoi(*Args[0]) : 2000);
	}
}
static void SiegeCampCmd(const TArray<FString>& Args, UWorld* World)
{
	if (ADCCArcadeSiege* Siege = ADCCArcadeSiege::Get(World))
	{
		Siege->DevCamp(Args.Num() ? FCString::Atoi(*Args[0]) : 0);
	}
}
static void SiegePostCmd(const TArray<FString>& Args, UWorld* World)
{
	(void)Args;
	if (ADCCArcadeSiege* Siege = ADCCArcadeSiege::Get(World))
	{
		Siege->DevPost();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSiegeWave(TEXT("DCC.SiegeWave"), TEXT("DCC.SiegeWave <n> [delay]: jump the tower defense to wave n."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SiegeWaveCmd));
static FAutoConsoleCommandWithWorldAndArgs GSiegeWall(TEXT("DCC.SiegeWall"), TEXT("DCC.SiegeWall <hp>: set the wall's health."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SiegeWallCmd));
static FAutoConsoleCommandWithWorldAndArgs GSiegeCamp(TEXT("DCC.SiegeCamp"), TEXT("DCC.SiegeCamp <0|1>: clear a camp, or bring it back."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SiegeCampCmd));
static FAutoConsoleCommandWithWorldAndArgs GSiegePost(TEXT("DCC.SiegePost"), TEXT("DCC.SiegePost: plant an ally on the first post."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SiegePostCmd));

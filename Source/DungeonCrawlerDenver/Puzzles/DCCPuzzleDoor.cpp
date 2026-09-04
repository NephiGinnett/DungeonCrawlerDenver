#include "Puzzles/DCCPuzzleDoor.h"

#include "Characters/DCCCharacterBase.h"
#include "Combat/DCCGameState.h"
#include "Favor/DCCFavorComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADCCPuzzleDoor::ADCCPuzzleDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Placeholder geometry, same convention as ADCCExitPortal's frame - a real
	// art pass swaps the mesh without touching any of this logic.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
	IndicatorLight->SetupAttachment(Mesh);
	IndicatorLight->SetIntensity(2000.0f);
	IndicatorLight->SetAttenuationRadius(400.0f);
	IndicatorLight->SetLightColor(FLinearColor::Red);

	LockedDescription = NSLOCTEXT("DCC", "PuzzleDoorLocked", "The way is closed.");
	SolvedAnnouncement = NSLOCTEXT("DCC", "PuzzleDoorSolved",
		"Access granted. Your compliance has been noted and filed.");
	ForcedAnnouncement = NSLOCTEXT("DCC", "PuzzleDoorForced",
		"Unauthorised entry logged. Viewers are delighted. Management is not.");
}

void ADCCPuzzleDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCPuzzleDoor, bOpen);
}

void ADCCPuzzleDoor::BeginPlay()
{
	Super::BeginPlay();
	ApplyOpenState();
}

void ADCCPuzzleDoor::OnRep_Open()
{
	ApplyOpenState();
}

void ADCCPuzzleDoor::ApplyOpenState()
{
	// Runs on every machine. Deliberately visuals-and-collision only: the
	// authoritative fact is bOpen, and this is just how each client draws it.
	if (Mesh)
	{
		Mesh->SetVisibility(!bOpen, true);
		Mesh->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision
			: ECollisionEnabled::QueryAndPhysics);
	}
	if (IndicatorLight)
	{
		IndicatorLight->SetLightColor(bOpen ? FLinearColor::Green : FLinearColor::Red);
	}
}

void ADCCPuzzleDoor::Unlock()
{
	if (!HasAuthority() || bOpen)
	{
		return;
	}
	OpenInternal(EDCCPuzzleSolveMethod::Solved, nullptr);
}

bool ADCCPuzzleDoor::ForceOpen(ADCCCharacterBase* Forcer)
{
	if (!HasAuthority() || bOpen)
	{
		return false;
	}

	if (!bAllowForcing)
	{
		if (Forcer)
		{
			Forcer->ShowLocalNotice(NSLOCTEXT("DCC", "PuzzleDoorNoForce",
				"This one does not budge. It was built by people who expected you."));
		}
		return false;
	}

	// Strength SCORE, not modifier - per CLAUDE.md, the books use the full word
	// for the score and the score is what lifting and breaking read.
	if (Forcer)
	{
		if (const UDCCStatsComponent* Stats = Forcer->FindComponentByClass<UDCCStatsComponent>())
		{
			const int32 Strength = Stats->GetEnhanced().GetScore(EDCCAbility::Strength);
			if (Strength < ForceStrengthRequired)
			{
				Forcer->ShowLocalNotice(FText::Format(
					NSLOCTEXT("DCC", "PuzzleDoorTooWeak",
						"You cannot shift it. Strength {0} required; you have {1}."),
					FText::AsNumber(ForceStrengthRequired), FText::AsNumber(Strength)));
				return false;
			}
		}
	}

	// The crowbar is not free. Charging BEFORE opening, for the same reason
	// UDCCSkillComponent spends mana before the roll: the cost is the point,
	// and a cost you can dodge by succeeding isn't one.
	if (Forcer)
	{
		if (UDCCFavorComponent* Favor = Forcer->FindComponentByClass<UDCCFavorComponent>())
		{
			Favor->AwardFavor(ForceFavorCost, EDCCFavorEvent::ScriptedBeat,
				NSLOCTEXT("DCC", "PuzzleDoorForceReason", "Forced a door rather than clearing it properly."));
		}
	}

	OpenInternal(EDCCPuzzleSolveMethod::Forced, Forcer);
	return true;
}

void ADCCPuzzleDoor::OpenInternal(EDCCPuzzleSolveMethod Method, ADCCCharacterBase* Cause)
{
	bOpen = true;
	ApplyOpenState();

	if (UWorld* World = GetWorld())
	{
		if (ADCCGameState* GameState = World->GetGameState<ADCCGameState>())
		{
			const FText& Line = (Method == EDCCPuzzleSolveMethod::Forced)
				? ForcedAnnouncement : SolvedAnnouncement;
			if (!Line.IsEmpty())
			{
				GameState->AddLogEntry(EDCCLogCategory::Encounter, Line);
			}
		}
	}

	OnDoorOpened.Broadcast(this, Method);
}

void ADCCPuzzleDoor::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	if (!Interactor || bOpen)
	{
		return;
	}

	// Interaction on a locked door is always the crowbar attempt. The intended
	// solution is never "walk up and press E" - it is plates, breakers or
	// stamps, all of which drive Unlock() from elsewhere.
	if (!LockedDescription.IsEmpty())
	{
		Interactor->ShowLocalNotice(LockedDescription);
	}

	if (HasAuthority())
	{
		ForceOpen(Interactor);
	}
	else
	{
		Interactor->Server_ForcePuzzleDoor(this);
	}
}

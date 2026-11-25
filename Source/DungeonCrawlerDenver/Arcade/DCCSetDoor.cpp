#include "Arcade/DCCSetDoor.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	constexpr float OpenReach = 260.0f;   // cm from the door's middle that opens it
	constexpr float StayOpen = 2.0f;      // seconds clear before it shuts
	constexpr float OpenSeconds = 0.6f;   // time to swing or slide fully
	constexpr float SwingDegrees = 100.0f;
}

UDCCSetDoorComponent::UDCCSetDoorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

bool UDCCSetDoorComponent::LooksLikeDoor(const UStaticMeshComponent* Mesh)
{
	const UStaticMesh* Asset = Mesh ? Mesh->GetStaticMesh() : nullptr;
	if (!Asset)
	{
		return false;
	}
	const FString Name = Asset->GetName().ToLower();
	if (!Name.Contains(TEXT("door")))
	{
		return false;
	}
	// Frames, walls with a hole, thresholds, panes, handles, lockers and machine hatches are not doors to walk through.
	static const TCHAR* NotDoors[] = { TEXT("wall"), TEXT("doorway"), TEXT("frame"), TEXT("threshold"), TEXT("glass"),
		TEXT("_ui"), TEXT("handle"), TEXT("locker"), TEXT("assembly"), TEXT("floor"), TEXT("bottom"), TEXT("trim"), TEXT("brace") };
	for (const TCHAR* Not : NotDoors)
	{
		if (Name.Contains(Not))
		{
			return false;
		}
	}
	const FVector Size = Asset->GetBoundingBox().GetSize() * Mesh->GetComponentScale().GetAbs();
	const float Wide = FMath::Max(Size.X, Size.Y);
	const float Thin = FMath::Min(Size.X, Size.Y);
	return Size.Z >= 180.0f && Wide >= 60.0f && Wide <= 560.0f && Thin <= 110.0f;
}

void UDCCSetDoorComponent::Setup(UStaticMeshComponent* Leaf)
{
	if (!Leaf || !Leaf->GetStaticMesh())
	{
		return;
	}
	LeafMesh = Leaf;
	Leaf->SetMobility(EComponentMobility::Movable);
	Closed = Leaf->GetComponentTransform();
	const FBox Local = Leaf->GetStaticMesh()->GetBoundingBox();
	const FVector Scale = Closed.GetScale3D().GetAbs();
	const FVector Size = Local.GetSize() * Scale;
	const bool bAlongX = Size.X >= Size.Y;
	const FVector AxisLocal = bAlongX ? FVector::ForwardVector : FVector::RightVector;
	Along = Closed.GetRotation().RotateVector(AxisLocal).GetSafeNormal2D();
	Normal = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
	Width = bAlongX ? Size.X : Size.Y;
	Thickness = bAlongX ? Size.Y : Size.X;
	bSlide = Width > 180.0f;
	// The hinge: the leaf's end at its low side along its width, at its middle through its thickness.
	FVector HingeLocal = Local.GetCenter();
	if (bAlongX) { HingeLocal.X = Local.Min.X; } else { HingeLocal.Y = Local.Min.Y; }
	Hinge = Closed.TransformPosition(HingeLocal);
}

void UDCCSetDoorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UStaticMeshComponent* Leaf = LeafMesh.Get();
	UWorld* World = GetWorld();
	if (!Leaf || !World)
	{
		return;
	}
	NextLook -= DeltaTime;
	if (NextLook <= 0.0f)
	{
		NextLook = 0.15f;
		const FVector Middle = Closed.TransformPosition(Leaf->GetStaticMesh()->GetBoundingBox().GetCenter());
		const ADCCCharacterBase* Nearest = nullptr;
		float Best = OpenReach + Width * 0.5f;
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			if (!It->IsAlive())
			{
				continue;
			}
			const FVector At = It->GetActorLocation();
			if (FMath::Abs(At.Z - Middle.Z) > 300.0f)
			{
				continue;
			}
			const float D = FVector::Dist2D(At, Middle);
			if (D < Best)
			{
				Best = D;
				Nearest = *It;
			}
		}
		if (Nearest && !bLocked)
		{
			if (Target < 0.5f && Open < 0.05f)
			{
				// Swing away from whoever is opening it.
				Side = FVector::DotProduct(Nearest->GetActorLocation() - Hinge, Normal) > 0.0f ? -1.0f : 1.0f;
			}
			Target = 1.0f;
			ClearFor = 0.0f;
		}
		else
		{
			ClearFor += 0.15f;
			if (bLocked || ClearFor >= StayOpen)
			{
				Target = 0.0f;
			}
		}
	}
	if (!FMath::IsNearlyEqual(Open, Target))
	{
		Open = FMath::FInterpConstantTo(Open, Target, DeltaTime, 1.0f / OpenSeconds);
		Apply();
	}
}

void UDCCSetDoorComponent::Apply()
{
	UStaticMeshComponent* Leaf = LeafMesh.Get();
	if (!Leaf)
	{
		return;
	}
	const float T = FMath::SmoothStep(0.0f, 1.0f, Open);
	FTransform Now = Closed;
	if (bSlide)
	{
		// Step out of the wall's face first, then run along it like a factory door.
		const float Out = FMath::Min(1.0f, Open * 4.0f) * (Thickness + 6.0f);
		Now.SetLocation(Closed.GetLocation() + Normal * Out + Along * (Width * 0.92f * T));
	}
	else
	{
		const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(SwingDegrees * T * Side));
		Now.SetRotation(Turn * Closed.GetRotation());
		Now.SetLocation(Hinge + Turn.RotateVector(Closed.GetLocation() - Hinge));
	}
	Leaf->SetWorldTransform(Now, false, nullptr, ETeleportType::TeleportPhysics);
}

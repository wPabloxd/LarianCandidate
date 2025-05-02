// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FMODBlueprintStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SpaceRider/RoundComponents/_common/Feature/SR_Walkable.h"
#include "SRSpaceMovementComponent.generated.h"

class ASR_Walkable;
class APlayerCharacter;
/**
 * 
 */
UCLASS()
class SPACERIDER_API USRSpaceMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KF | Movement", meta = (AllowPrivateAccess = "true"))
	float SmoothTransitionSpeed = 150.f;

public:
	virtual void BeginPlay() override;

	virtual void CheckCurrentFloorStatus();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	virtual void PhysFalling(float deltaTime, int32 Iterations) override;

	virtual void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0) override;

	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

	virtual void SetPostLandedPhysics(const FHitResult& Hit) override;

	virtual bool IsWalkable(const FHitResult& Hit) const override;

	virtual bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const override;

	virtual void FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation,
	                       const FHitResult* DownwardSweepResult) const override;

	virtual void SimulateMovement(float DeltaTime) override;

	virtual void ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance,
	                              FFindFloorResult& OutFloorResult, float SweepRadius,
	                              const FHitResult* DownwardSweepResult) const override;

	virtual bool FloorSweepTest(FHitResult& OutHit, const FVector& Start, const FVector& End,
	                            ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape,
	                            const FCollisionQueryParams& Params,
	                            const FCollisionResponseParams& ResponseParam) const override;

	void UpdateGravityDirection(const FVector& NewGravityDirection);

protected:
	TObjectPtr<APlayerCharacter> PlayerCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SR | Movement")
	FFMODEventInstance PlayerMovementFMODInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "KF | Movement", meta = (AllowPrivateAccess = "true"))
	bool bPlayerInsideWalkableArea = false;

	EMovementMode PreviousMovementMode; // To help with the transition when boots are activated

	UFUNCTION()
	void UpdatePlayerStatusInsideWalkableArea();

	void ComputeFloorDistBeforeGrabbing(const FVector& CapsuleLocation,
	                                    FFindFloorResult& OutFloorResult,
	                                    const FHitResult* DownwardSweepResult = nullptr,
	                                    float LineDistance = 0.f,
	                                    float SweepDistance = 0.f,
	                                    float SweepRadius = 0.f) const;

	UFUNCTION()
	void OnPlayerActiveBoots(const APlayerCharacter* _PlayerCharacter, const bool bActiveBoots);

	UFUNCTION()
	void TransitionToSmoothFloatAndFloor(const bool _SmoothTransition = true);

	UFUNCTION()
	void OnDiscoverWalkable(ASR_Walkable* Walkable, bool bDiscovered);

	//UFUNCTION()
	//void IsOriented();

#pragma region Boots

#pragma  endregion

private:
	FVector CustomVelocity;

public:
	FORCEINLINE ASR_Walkable* GetWalkable() const
	{
		return CurrentFloor.HitResult.Component.IsValid()
			       ? Cast<ASR_Walkable>(CurrentFloor.HitResult.GetActor())
			       : nullptr;
	}
};

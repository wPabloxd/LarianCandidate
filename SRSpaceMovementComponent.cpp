#include "SRSpaceMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "SpaceRider/Characters/PlayerCharacter.h"
#include "SpaceRider/RoundComponents/_common/Feature/SR_Walkable.h"
#include "SpaceRider/Characters/Player/SRPlayerInteractionInstigator.h"
#include "SpaceRider/Config/GameInstance/SRGameInstance.h"
#include "SpaceRider/Config/Subsystem/SRAudioSettings.h"
#include "SpaceRider/Config/_common/InGame/SRBaseSpacePlayerController.h"

void USRSpaceMovementComponent::BeginPlay()
{
	PrimaryComponentTick.bCanEverTick = true;
	Super::BeginPlay();

	// Get References
	PlayerCharacter = Cast<APlayerCharacter>(GetOwner());
	PlayerCharacter->OnPlayerActiveBoots.AddDynamic(this, &USRSpaceMovementComponent::OnPlayerActiveBoots);
	PlayerCharacter->GetInteractionInstigator()->OnDiscoverWalkable.AddDynamic(
		this, &USRSpaceMovementComponent::OnDiscoverWalkable);
}

void USRSpaceMovementComponent::CheckCurrentFloorStatus()
{
	UpdatePlayerStatusInsideWalkableArea();
}

void USRSpaceMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update the player status inside the walkable area
	UpdatePlayerStatusInsideWalkableArea();
	// Smooth transition between floating and walking
	if (PlayerCharacter->GetBootsTransitionState() ==
		EPlayerBootsState::TRANSITION_TO)
	{
		TransitionToSmoothFloatAndFloor(true);
	}
	// Update gravity direction based on the current floor
	if (MovementMode == MOVE_Walking && CurrentFloor.IsWalkableFloor())
	{
		// TODO: This may be lerp to avoid sudden changes
		const FVector _ShapeGravityDirection = CurrentFloor.HitResult.ImpactNormal;
		const FVector _GravityDirection = -_ShapeGravityDirection;


		const FVector CurrentActorUpVector = PlayerCharacter->GetActorUpVector().GetSafeNormal();

		const float DotProduct = FVector::DotProduct(CurrentActorUpVector, _ShapeGravityDirection);

		// Calculate the angle between the vectors in radians
		const float AngleRadians = FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f));

		// Convert the angle from radians to degrees
		const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);

		if (AngleDegrees > 1.f || AngleDegrees < -1.f)
		{
			FVector LerpedVector = FMath::Lerp(CurrentActorUpVector, _ShapeGravityDirection,
			                                   FMath::Clamp(AngleRadians / PI, 0.0f, 1.0f));
			UpdateGravityDirection(-LerpedVector);
		}
		else
		{
			UpdateGravityDirection(_GravityDirection);
		}
	}
	else if (PlayerCharacter && PlayerCharacter->GetBootsTransitionState() == EPlayerBootsState::GRABBED && (
		MovementMode == MOVE_Falling && PreviousMovementMode == MOVE_Walking))
	{
		// If the player was walking on a walkable floor and now is not, we need to disable the boots
		PlayerCharacter->DeactivateBoots();
	}


	// Get the UEnum pointer for your enum type
	//UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPlayerBootsState"), true);

	/* if (EnumPtr)
	{
		// Get the index of the enum value
		int32 Index = static_cast<int32>(PlayerCharacter->GetBootsTransitionState());

		// Get the name of the enum value at the given index
		FString EnumName = EnumPtr->GetNameStringByIndex(Index);

		// Print the name of the enum value
		//UE_LOG(LogTemp, Warning, TEXT("Current Boots State: %s"), *EnumName);
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("Estado: %s"), *EnumName));
	}*/
}

void USRSpaceMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	Super::PhysFalling(deltaTime, Iterations);
}

/**
 * @brief This function is called when the player activates or deactivates the boots. (Event call)
 * And will bind the current floor with the appropriate walkable floor.
 * @param Walkable 
 * @param bDiscovered 
 */
void USRSpaceMovementComponent::OnDiscoverWalkable(ASR_Walkable* Walkable, bool bDiscovered)
{
	if (!PlayerCharacter->GetInteractionInstigator()->HasAnyWalkableClose())
	{
		PlayerCharacter->UpdateBootsTransitionState(EPlayerBootsState::IDLE);
	}
	else
	{
		PlayerCharacter->UpdateBootsTransitionState(EPlayerBootsState::IN_RANGE);
		if (PlayerMovementFMODInstance.Instance != NULL)
		{
			PlayerMovementFMODInstance.Instance->stop(FMOD_STUDIO_STOP_MODE::FMOD_STUDIO_STOP_ALLOWFADEOUT);
		}
		PlayerMovementFMODInstance = Cast<USRGameInstance>(GetWorld()->GetGameInstance())->
		                             GetAudioSettingsSubsystem()->
		                             PlayFMODAtLocation(
			                             PlayerCharacter->GetSpacePlayerController()->GetPlayerMovementSFX()[
				                             EPlayerMovementSFXSounds::RADARBOOTSON],
			                             GetActorTransform());
	}
}


void USRSpaceMovementComponent::UpdatePlayerStatusInsideWalkableArea()
{
	if (PlayerCharacter->GetBootsTransitionState() == EPlayerBootsState::IN_RANGE ||
		PlayerCharacter->GetBootsTransitionState() == EPlayerBootsState::ORIENTED)
	{
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, Velocity.IsZero(), NULL);
		const FVector PlayerUpVector = PlayerCharacter->GetActorUpVector();
		const FVector FloorNormalVector = CurrentFloor.HitResult.Normal;

		float DotProduct = FVector::DotProduct(PlayerUpVector.GetSafeNormal(), FloorNormalVector.GetSafeNormal());

		// Calculate the angle in radians using the dot product
		float AngleRadians = FMath::Acos(DotProduct);

		// Convert the angle from radians to degrees
		float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);
		// Check if the angle is within the X-degree margin
		if (AngleDegrees <= 40.0f && !(PlayerCharacter->GetRollRotation() > 1000 ||
			PlayerCharacter->GetRollRotation() < -1000) && PlayerCharacter->GetVelocity().Length() < 400.f)
		{
			// Vectors are similar within the 10-degree margin
			PlayerCharacter->UpdateBootsTransitionState(EPlayerBootsState::ORIENTED);
		}
		else
		{
			// Vectors are not similar within the 10-degree margin
			PlayerCharacter->UpdateBootsTransitionState(EPlayerBootsState::IN_RANGE);
		}
	}
}

void USRSpaceMovementComponent::ComputeFloorDistBeforeGrabbing(const FVector& CapsuleLocation,
                                                               FFindFloorResult& OutFloorResult,
                                                               const FHitResult* DownwardSweepResult,
                                                               float LineDistance,
                                                               float SweepDistance,
                                                               float SweepRadius) const
{
	const float HeightCheckAdjust = (IsMovingOnGround() ? MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);
	// To check the height to the floor detected.
	float PlayerCharacterSphereRadius = MAX_FLOOR_DIST;
	float PlayerCapsuleSweepRadiusToApply = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	// IMPORTANT Get player character sphere area
	if (PlayerCharacter)
	{
		// PlayerCapsuleSweepRadiusToApply = PlayerCharacter->InteractionInstigator->GetWalkableInteractionArea()->
		//                                                    GetScaledSphereRadius();
		PlayerCharacterSphereRadius = PlayerCharacter->GetInteractionInstigator()->GetWalkableInteractionArea()->
		                                               GetScaledSphereRadius();
	}

	// MAX_FLOOR_DIST used before instead of PlayerCharacterSphereRadius
	const float FloorSweepTraceDist = FMath::Max(PlayerCharacterSphereRadius, MaxStepHeight + HeightCheckAdjust);
	const float FloorLineTraceDist = FloorSweepTraceDist;
	ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult,
	                 PlayerCapsuleSweepRadiusToApply, DownwardSweepResult);
}

// TODO: We should use a customized movement mode, however we are using fall state as float state
void USRSpaceMovementComponent::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
	// TODO: We should use this to use a customized movement mode
	// if (NewMovementMode == MOVE_Falling)
	// {
	// 	NewMovementMode = MOVE_Custom;
	// }
	// UE_LOG(LogTemp, Warning, TEXT("SetMovementMode %d, %d"), NewMovementMode, NewCustomMode);
	PreviousMovementMode = MovementMode;

	// Player reach the floor and is walking now
	if (NewMovementMode == MOVE_Walking && PlayerCharacter && PlayerCharacter->GetBootsTransitionState() ==
		EPlayerBootsState::TRANSITION_TO)
	{
		PlayerCharacter->UpdateBootsTransitionState(EPlayerBootsState::GRABBED);
		if (PlayerMovementFMODInstance.Instance != NULL)
		{
			PlayerMovementFMODInstance.Instance->stop(FMOD_STUDIO_STOP_MODE::FMOD_STUDIO_STOP_ALLOWFADEOUT);
		}
	}
	else if (NewMovementMode == MOVE_Falling && PlayerCharacter &&
		PlayerCharacter->GetBootsTransitionState() == EPlayerBootsState::GRABBED)
	{
		if (PlayerCharacter->GetSpacePlayerController()->GetPlayerMovementSFX().Contains(
			EPlayerMovementSFXSounds::BOOTSOFF))
		{
			Cast<USRGameInstance>(GetWorld()->GetGameInstance())
				->GetAudioSettingsSubsystem()
				->PlayFMODAtLocation(PlayerCharacter->GetSpacePlayerController()
				                                    ->GetPlayerMovementSFX()[EPlayerMovementSFXSounds::BOOTSOFF],
				                     GetActorTransform());
		}
		PlayerCharacter->bHasActivatedBoots = false;
		PlayerCharacter->UpdateBootsTransitionState(EPlayerBootsState::IN_RANGE);
		ASRBaseSpacePlayerController* PlayerController =
			Cast<ASRBaseSpacePlayerController>(PlayerCharacter->GetController());
		if (PlayerController->GetIsMoving())
		{
			PlayerCharacter->LaunchCharacter(PlayerCharacter->GetActorForwardVector() * 150, false, false);
		}
		//if (PlayerCharacter)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
		//	                                 FString::Printf(
		//		                                 TEXT("Si se pudo crear el material dinámico para el índice %d"), 1));
		//	//PlayerCharacter->ChangeColorBoots(PlayerCharacter, 1, 2); // Cambiará el color a azul
		//	PlayerCharacter->ChangeColorBoots(); // Cambiará el color a azul
		//}
		//else
		//{
		//	// Manejar el caso en el que PlayerCharacter sea nulo
		//	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
		//	                                 FString::Printf(
		//		                                 TEXT("No se pudo crear el material dinámico para el índice %d"), 1));
		//	UE_LOG(LogTemp, Warning, TEXT("El puntero PlayerCharacter es nulo"));
		//}
	}
	Super::SetMovementMode(NewMovementMode, NewCustomMode);
}

/**
 * @brief This function is called when the player activates or deactivates the boots. (Event call)
 * @param _PlayerCharacter 
 * @param bActiveBoots 
 */
void USRSpaceMovementComponent::OnPlayerActiveBoots(const APlayerCharacter* _PlayerCharacter, const bool bActiveBoots)
{
	if (PlayerCharacter->GetBootsTransitionState() != EPlayerBootsState::ORIENTED &&
		PlayerCharacter->GetBootsTransitionState() != EPlayerBootsState::GRABBED)
	{
		return;
	}
	// Transition Smoothly to walk on the floor
	if (bActiveBoots)
	{
		if (MovementMode == MOVE_Falling)
		{
			// Previous FindFloor check to ensure thYoe player is in the correct place
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, Velocity.IsZero(), NULL);
			// TODO: Enhance this
			// Play Sound
			if (PlayerCharacter->GetSpacePlayerController()->GetPlayerMovementSFX().Contains(
				EPlayerMovementSFXSounds::BOOTSON))
			{
				Cast<USRGameInstance>(GetWorld()->GetGameInstance())->GetAudioSettingsSubsystem()->PlayFMODAtLocation(
					PlayerCharacter->GetSpacePlayerController()->GetPlayerMovementSFX()[
						EPlayerMovementSFXSounds::BOOTSON],
					GetActorTransform());
			}
			PlayerCharacter->SetRollRotation(0);
			PlayerCharacter->UpdateBootsTransitionState(EPlayerBootsState::TRANSITION_TO);
			if (PlayerMovementFMODInstance.Instance != NULL)
			{
				PlayerMovementFMODInstance.Instance->stop(FMOD_STUDIO_STOP_MODE::FMOD_STUDIO_STOP_ALLOWFADEOUT);
			}
			PlayerMovementFMODInstance = Cast<USRGameInstance>(GetWorld()->GetGameInstance())->
			                             GetAudioSettingsSubsystem()->
			                             PlayFMODAtLocation(
				                             PlayerCharacter->GetSpacePlayerController()->GetPlayerMovementSFX()[
					                             EPlayerMovementSFXSounds::BOOTSLANDING],
				                             GetActorTransform());
		}
	}
	else
	{
		// If the player is walking on a walkable floor, we can deactivate the boots
		if (MovementMode == MOVE_Walking) // && CurrentFloor.IsWalkableFloor()
		{
			APawn *OwnerPawn = Cast<APawn>(GetOwner());
			if (OwnerPawn)
			{
				APlayerController *PlayerController = OwnerPawn->GetController<APlayerController>();
				if (PlayerController)
				{
					ASRBaseSpacePlayerController *Controller = Cast<ASRBaseSpacePlayerController>(PlayerController);
					if (Controller)
					{
						// Reseted to prevent facing an incorrect direction when grabbed again
						Controller->LastMovementInput = FVector2D(0.f, 0.f); 
					}
				}
			}
			// TODO: Enhance this
			// Play Sound
			if (PlayerCharacter->GetSpacePlayerController()->GetPlayerMovementSFX().Contains(
				EPlayerMovementSFXSounds::BOOTSOFF))
			{
				Cast<USRGameInstance>(GetWorld()->GetGameInstance())->GetAudioSettingsSubsystem()->PlayFMODAtLocation(
					PlayerCharacter->GetSpacePlayerController()->GetPlayerMovementSFX()[
						EPlayerMovementSFXSounds::BOOTSOFF],
					GetActorTransform());
			}
			PlayerCharacter->UpdateBootsTransitionState(EPlayerBootsState::ORIENTED);
		}
	}
}

void USRSpaceMovementComponent::TransitionToSmoothFloatAndFloor(const bool _SmoothTransition)
{
	// TODO: This would be probable unnecessary, because we compute the floor distance in the FindFloor already but we don't save the result (But that is a unreal native behaviour)
	// Calculate the distance to the floor before grabbing
	ComputeFloorDistBeforeGrabbing(UpdatedComponent->GetComponentLocation(),
	                               CurrentFloor);
	// const float TargetFloorHeight = CurrentFloor.HitResult.ImpactPoint.Z;

	const FVector TargetFloorLocation = CurrentFloor.HitResult.Location; // World location

	// Obtain the current height of the character
	const FVector CurrentLocation = PlayerCharacter->GetActorLocation();
	const FVector CurrentCharacterLocation = CurrentLocation;

	// Calculate the necessary displacement
	const float DistanceToFloor = FVector::Distance(CurrentCharacterLocation, TargetFloorLocation);

	// Calculate the progress towards the floor
	const float Progress = FMath::Clamp((SmoothTransitionSpeed * GetWorld()->GetDeltaSeconds()) / DistanceToFloor, 0.0f,
	                                    1.0f);

	// ---------------------------------------------------------
	// Rotation Calculation
	// ---------------------------------------------------------
	//TODO: DESCOMENTAR ESTO PARA ROTAR. NO FUNCIONA PORQUE SI TIENES UN TECHO ARRIBITA PUES SE VA A LA VERGA
	//TODO: HAY QUE CHECKEAR QUE EL SUELO ESTA ARRIBA DE EL PARA NO ROTAR HACIA LA DIRECCIÓN CONTRARIA
	// TODO: Enhance this
	const float ProgressRotation = FMath::Clamp(
		(SmoothTransitionSpeed * 10 * GetWorld()->GetDeltaSeconds()) / DistanceToFloor, 0.0f,
		1.0f);


	const FRotator _PlayerCurrentRotation = PlayerCharacter->GetActorRotation();
	const FRotator _EndFloorRotation = CurrentFloor.HitResult.Location.Rotation();
	FRotator _FinalPlayerRotationToApply = _PlayerCurrentRotation;
	// Interpolate smoothly player rotation to the floor
	const FRotator _NewPlayerRotation = FMath::Lerp(_PlayerCurrentRotation,
	                                                _EndFloorRotation,
	                                                ProgressRotation);
	// // TODO: Only apply this Axis?
	//_FinalPlayerRotationToApply.Roll = _NewPlayerRotation.Roll;
	// ---------------------------------------------------------

	// ---------------------------------------------------------
	// Location Calculation
	// ---------------------------------------------------------
	// Interpolate smoothly to the target height
	const FVector NewLocation = FMath::Lerp(CurrentLocation, TargetFloorLocation,
	                                        Progress);
	// ---------------------------------------------------------

	// UE_LOG(LogTemp, Warning, TEXT("CurrentLocation--- %s ----- NewLocation-- %s -- TargetFloorZLocation: %f"),
	//        *CurrentLocation.ToString(), *NewLocation.ToString(), TargetFloorZLocation);

	// Update the position of the character
	PlayerCharacter->SetActorLocation(NewLocation);
	//TODO: DESCOMENTAR ESTO PARA ROTAR. NO FUNCIONA PORQUE SI TIENES UN TECHO ARRIBITA PUES SE VA A LA VERGA
	//TODO: HAY QUE CHECKEAR QUE EL SUELO ESTA ARRIBA DE EL PARA NO ROTAR HACIA LA DIRECCIÓN CONTRARIA
	PlayerCharacter->SetActorRotation(_FinalPlayerRotationToApply);

	// Check if the character has reached the floor
	//if (FMath::IsNearlyEqual(NewLocation, TargetFloorLocation, 1.0f))
	if (FVector::Distance(PlayerCharacter->GetActorLocation(), CurrentFloor.HitResult.Location) < 60.f)
	{
		// We check with Unreal's native function if the character has reached the floor
		ProcessLanded(CurrentFloor.HitResult, 0.f, 0);
		PlayerMovementFMODInstance.Instance->stop(FMOD_STUDIO_STOP_MODE::FMOD_STUDIO_STOP_ALLOWFADEOUT);
	}
}

void USRSpaceMovementComponent::UpdateGravityDirection(const FVector& NewGravityDirection)
{
	SetGravityDirection(NewGravityDirection);
}

#pragma region Walkeable floors

/**
 * @brief This function is called to check if the hit is walkable.
 * We are in the space, so generally we can walk on everything.
 * @param Hit 
 * @return true 
 * @return false 
 */
bool USRSpaceMovementComponent::IsWalkable(const FHitResult& Hit) const
{
	// No call to Super::IsWalkable(Hit) because we want to use our custom walkable
	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration
		return false;
	}
	return true;
}

bool USRSpaceMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	return true;
}

/**
 * @brief This function is called to find the floor under the character.
 * When the character is finding the floor, will want to use the custom floor to walk on.
 * @param CapsuleLocation 
 * @param OutFloorResult 
 * @param bCanUseCachedLocation 
 * @param DownwardSweepResult 
 */
void USRSpaceMovementComponent::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult,
                                          bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	UE_LOG(LogTemp, Verbose, TEXT("[Role:%d] FindFloor: %s at location %s"),
	       (int32)CharacterOwner->GetLocalRole(), *GetNameSafe(CharacterOwner), *CapsuleLocation.ToString());
	// No collision, no floor...
	if (!HasValidData() || !UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	check(CharacterOwner->GetCapsuleComponent());

	// Increase height check slightly if walking, to prevent floor height adjustment from later invalidating the floor result.
	const float HeightCheckAdjust = (IsMovingOnGround() ? MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);

	// To check the height to the floor detected.
	float PlayerCharacterSphereRadius = MAX_FLOOR_DIST;
	float PlayeCapsuleSweepRadiusToApply = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	// IMPORTANT Get player character sphere area
	if (PlayerCharacter)
	{
		// PlayeCapsuleSweepRadiusToApply = PlayerCharacter->InteractionInstigator->GetWalkableInteractionArea()->
		//                                                   GetScaledSphereRadius();
		PlayerCharacterSphereRadius = PlayerCharacter->GetInteractionInstigator()->GetWalkableInteractionArea()->
		                                               GetScaledSphereRadius();
	}

	// MAX_FLOOR_DIST used before instead of PlayerCharacterSphereRadius
	float FloorSweepTraceDist = FMath::Max(PlayerCharacterSphereRadius, MaxStepHeight + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor = true;

	// Sweep floor
	if (FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		USRSpaceMovementComponent* MutableThis = const_cast<USRSpaceMovementComponent*>(this);

		if (bAlwaysCheckFloor || !bCanUseCachedLocation || bForceNextFloorCheck || bJustTeleported)
		{
			MutableThis->bForceNextFloorCheck = false;
			ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult,
			                 PlayeCapsuleSweepRadiusToApply, DownwardSweepResult);
		}
		else
		{
			// Force floor check if base has collision disabled or if it does not block us.
			UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
			const AActor* BaseActor = MovementBase ? MovementBase->GetOwner() : NULL;
			const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

			if (MovementBase != NULL)
			{
				MutableThis->bForceNextFloorCheck = !MovementBase->IsQueryCollisionEnabled()
					|| MovementBase->GetCollisionResponseToChannel(CollisionChannel) != ECR_Block
					|| MovementBaseUtility::IsDynamicBase(MovementBase);
			}

			const bool IsActorBasePendingKill = BaseActor && !IsValid(BaseActor);

			if (!bForceNextFloorCheck && !IsActorBasePendingKill && MovementBase)
			{
				//UE_LOG(LogCharacterMovement, Log, TEXT("%s SKIP check for floor"), *CharacterOwner->GetName());
				OutFloorResult = CurrentFloor;
				bNeedToValidateFloor = false;
			}
			else
			{
				MutableThis->bForceNextFloorCheck = false;
				ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult,
				                 PlayeCapsuleSweepRadiusToApply, DownwardSweepResult);
			}
		}
	}

	// OutFloorResult.HitResult is now the result of the vertical floor check.
	// See if we should try to "perch" at this location.
	if (bNeedToValidateFloor && OutFloorResult.bBlockingHit && !OutFloorResult.bLineTrace)
	{
		const bool bCheckRadius = true;
		if (ShouldComputePerchResult(OutFloorResult.HitResult, bCheckRadius))
		{
			float MaxPerchFloorDist = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + HeightCheckAdjust);
			if (IsMovingOnGround())
			{
				MaxPerchFloorDist += FMath::Max(0.f, PerchAdditionalHeight);
			}

			FFindFloorResult PerchFloorResult;
			if (ComputePerchResult(GetValidPerchRadius(), OutFloorResult.HitResult, MaxPerchFloorDist,
			                       PerchFloorResult))
			{
				// Don't allow the floor distance adjustment to push us up too high, or we will move beyond the perch distance and fall next time.
				const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
				const float MoveUpDist = (AvgFloorDist - OutFloorResult.FloorDist);
				if (MoveUpDist + PerchFloorResult.FloorDist >= MaxPerchFloorDist)
				{
					OutFloorResult.FloorDist = AvgFloorDist;
				}

				// If the regular capsule is on an unwalkable surface but the perched one would allow us to stand, override the normal to be one that is walkable.
				if (!OutFloorResult.bWalkableFloor)
				{
					// Floor distances are used as the distance of the regular capsule to the point of collision, to make sure AdjustFloorHeight() behaves correctly.
					OutFloorResult.SetFromLineTrace(PerchFloorResult.HitResult, OutFloorResult.FloorDist,
					                                FMath::Max(OutFloorResult.FloorDist, MIN_FLOOR_DIST), true);
				}
			}
			else
			{
				// We had no floor (or an invalid one because it was unwalkable), and couldn't perch here, so invalidate floor (which will cause us to start falling).
				OutFloorResult.bWalkableFloor = false;
			}
		}
	}

	// By default, the floor is not walkable
	uint32 _bIsWalkable = 0; // 0 is false
	if (OutFloorResult.HitResult.GetActor())
	{
		// Check if the floor is walkable
		if (ASR_Walkable* _Walkable = Cast<ASR_Walkable>(OutFloorResult.HitResult.GetActor()))
		{
			if (OutFloorResult.bWalkableFloor == 1 && _Walkable->IsWalkable())
			{
				// UE_LOG(LogTemp, Warning, TEXT("Walkable"));
				_Walkable->SetPossibleToWalkWithBoots(true);
			}
			else
			{
				// UE_LOG(LogTemp, Warning, TEXT("Not Walkable"));
				// It's needed in case the floor change from walkable to not walkable at some point
				_Walkable->SetPossibleToWalkWithBoots(false);
			}
			if (PlayerCharacter && PlayerCharacter->IsBootsActive() && _Walkable->IsPossibleToWalkWithBoots())
			{
				_bIsWalkable = 1; // 1 is true
			}
		}
	}
	OutFloorResult.bWalkableFloor = _bIsWalkable;
}

/**
 * @brief This function is called to compute the floor distance.
 * @param CapsuleLocation 
 * @param LineDistance 
 * @param SweepDistance 
 * @param OutFloorResult 
 * @param SweepRadius 
 * @param DownwardSweepResult 
 */
//TODO: This method is the key to make the character walk on the floor
bool USRSpaceMovementComponent::FloorSweepTest(FHitResult& OutHit, const FVector& Start, const FVector& End,
                                               ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape,
                                               const FCollisionQueryParams& Params,
                                               const FCollisionResponseParams& ResponseParam) const
{
	bool bBlockingHit = false;

	if (!bUseFlatBaseForFloorChecks)
	{
		// if (CollisionShape.IsNearlyZero())
		// {
		// 	UE_LOG(LogTemp, Warning, TEXT("CollisionShape.IsNearlyZero()"));
		// 	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params, ResponseParam);
		// 	// FPhysicsInterface::RaycastSingle(GetWorld(), OutHit, Start, End, TraceChannel, Params, ResponseParam,
		// 	// 								 FCollisionObjectQueryParams::DefaultObjectQueryParam);
		// }
		// else
		// {
		// 	UE_LOG(LogTemp, Warning, TEXT("CollisionShape.IsNotNearlyZero()"));
		// 	FPhysicsInterface::GeomSweepSingle(GetWorld(), CollisionShape, FQuat::Identity, OutHit, Start, End,
		// 	                                   TraceChannel, Params, ResponseParam,
		// 	                                   FCollisionObjectQueryParams::DefaultObjectQueryParam);
		// }
		// FHitResult DummyHit(1.f);
		// DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 3.f, 0, 2);
		// bool hitted = FPhysicsInterface::GeomSweepSingle(GetWorld(), CollisionShape,
		//                                                  FQuat::Identity, DummyHit, Start,
		//                                                  End,
		//                                                  TraceChannel, Params, ResponseParam,
		//                                                  FCollisionObjectQueryParams::DefaultObjectQueryParam);
		//FCollisionObjectQueryParams::DefaultObjectQueryParam
		// DrawDebug CollisionShape
		// DrawDebugCapsule(GetWorld(), Start, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(),
		//                  FQuat::Identity, FColor::Green, false, 3.f, 0, 2);
		// DrawDebug capsule component
		// DrawDebugCapsule(GetWorld(), Start, CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),
		//                  CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), FQuat::Identity,
		//                  FColor::Red,
		//                  false, 3.f, 0, 2);
		// UE_LOG(LogTemp, Warning, TEXT("DummyHit %d, %s"), hitted, *DummyHit.ImpactPoint.ToString());


		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel,
		                                                CollisionShape, Params, ResponseParam);
		// UE_LOG(LogTemp, Warning, TEXT("bBlockingHit: %d"), bBlockingHit);
	}
	else
	{
		// Test with a box that is enclosed by the capsule.
		const float CapsuleRadius = CollisionShape.GetCapsuleRadius();
		const float CapsuleHeight = CollisionShape.GetCapsuleHalfHeight();
		const FCollisionShape BoxShape = FCollisionShape::MakeBox(
			FVector(CapsuleRadius * 0.707f, CapsuleRadius * 0.707f, CapsuleHeight));

		// First test with the box rotated so the corners are along the major axes (ie rotated 45 degrees).
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End,
		                                                FQuat(RotateGravityToWorld(FVector(0.f, 0.f, -1.f)),
		                                                      UE_PI * 0.25f), TraceChannel, BoxShape, Params,
		                                                ResponseParam);

		if (!bBlockingHit)
		{
			// Test again with the same box, not rotated.
			OutHit.Reset(1.f, false);
			bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, BoxShape,
			                                                Params, ResponseParam);
		}
	}

	return bBlockingHit;
}

/**
 * IMPORTANT. Here the character found the floor and we can use this to walk on the floor.
 * @param Hit 
 * @param remainingTime 
 * @param Iterations 
 */
void USRSpaceMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	// UE_LOG(LogTemp, Warning, TEXT("1ProcessLanded"));
	// Super::ProcessLanded(Hit, remainingTime, Iterations);

	if (CharacterOwner && CharacterOwner->ShouldNotifyLanded(Hit))
	{
		// UE_LOG(LogTemp, Warning, TEXT("2ProcessLanded: %s"), *GetNameSafe(CharacterOwner));
		CharacterOwner->Landed(Hit);
	}
	if (IsFalling())
	{
		// UE_LOG(LogTemp, Warning, TEXT("3ProcessLanded"));
		if (GetGroundMovementMode() == MOVE_NavWalking)
		{
			// UE_LOG(LogTemp, Warning, TEXT("4ProcessLanded"));
			// verify navmesh projection and current floor
			// otherwise movement will be stuck in infinite loop:
			// navwalking -> (no navmesh) -> falling -> (standing on something) -> navwalking -> ....

			const FVector TestLocation = GetActorFeetLocation();
			FNavLocation NavLocation;

			const bool bHasNavigationData = FindNavFloor(TestLocation, NavLocation);
			if (!bHasNavigationData || NavLocation.NodeRef == INVALID_NAVNODEREF)
			{
				SetGroundMovementMode(MOVE_Walking);
				UE_LOG(LogTemp, Verbose,
				       TEXT(
					       "ProcessLanded(): %s tried to go to NavWalking but couldn't find NavMesh! Using Walking instead."
				       ), *GetNameSafe(CharacterOwner));
			}
		}

		SetPostLandedPhysics(Hit);
	}

	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		UE_LOG(LogTemp, Warning, TEXT("5ProcessLanded"));
		PFAgent->OnLanded();
	}

	StartNewPhysics(remainingTime, Iterations);
}

void USRSpaceMovementComponent::ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance,
                                                 float SweepDistance, FFindFloorResult& OutFloorResult,
                                                 float SweepRadius,
                                                 const FHitResult* DownwardSweepResult) const
{
	// Super::ComputeFloorDist(CapsuleLocation, LineDistance, SweepDistance, OutFloorResult, SweepRadius,
	//                         DownwardSweepResult);
	// UE_LOG(LogTemp, Warning, TEXT("[Role:%d] ComputeFloorDist: %s at location %s"),
	//        (int32)CharacterOwner->GetLocalRole(), *GetNameSafe(CharacterOwner), *CapsuleLocation.ToString());
	OutFloorResult.Clear();

	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	bool bSkipSweep = false;
	if (DownwardSweepResult != NULL && DownwardSweepResult->IsValidBlockingHit())
	{
		UE_LOG(LogTemp, Warning, TEXT("DownwardSweepResult"));
		// Only if the supplied sweep was vertical and downward.
		const bool bIsDownward = RotateWorldToGravity(DownwardSweepResult->TraceStart - DownwardSweepResult->TraceEnd).Z
			> 0;
		const bool bIsVertical = RotateWorldToGravity(DownwardSweepResult->TraceStart - DownwardSweepResult->TraceEnd).
			SizeSquared2D() <= UE_KINDA_SMALL_NUMBER;
		if (bIsDownward && bIsVertical)
		{
			// Reject hits that are barely on the cusp of the radius of the capsule
			if (IsWithinEdgeTolerance(DownwardSweepResult->Location, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				// Don't try a redundant sweep, regardless of whether this sweep is usable.
				bSkipSweep = true;

				const bool bIsWalkable = IsWalkable(*DownwardSweepResult);
				const float FloorDist = RotateWorldToGravity(CapsuleLocation - DownwardSweepResult->Location).Z;
				OutFloorResult.SetFromSweep(*DownwardSweepResult, FloorDist, bIsWalkable);

				if (bIsWalkable)
				{
					// Use the supplied downward sweep as the floor hit result.			
					return;
				}
			}
		}
	}

	// We require the sweep distance to be >= the line distance, otherwise the HitResult can't be interpreted as the sweep result.
	if (SweepDistance < LineDistance)
	{
		ensure(SweepDistance >= LineDistance);
		return;
	}

	bool bBlockingHit = false;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

	// Sweep test
	if (!bSkipSweep && SweepDistance > 0.f && SweepRadius > 0.f)
	{
		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = SweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(SweepRadius, PawnHalfHeight - ShrinkHeight);

		FVector TraceDistVectorGravity = RotateGravityToWorld(FVector(0.f, 0.f, -TraceDist));
		// UE_LOG(LogTemp, Warning, TEXT("TraceDistVectorGravity: %s"), *TraceDistVectorGravity.ToString());
		FVector EndLocationGravity = CapsuleLocation + TraceDistVectorGravity;

		// Obtener la dirección hacia donde apunta el personaje debajo de sus pies
		FVector CharacterForwardVector = CharacterOwner->GetActorForwardVector();
		FVector DownDirection = FVector::CrossProduct(CharacterForwardVector, FVector::UpVector).GetSafeNormal();

		// Calcular el vector de dirección final para el sweep test
		FVector EndLocation = RotateGravityToWorld(DownDirection) * SweepDistance + CapsuleLocation - TraceDist;


		// UE_LOG(LogTemp, Warning, TEXT("StartLocation: %s, EndLocation: %s"), *CapsuleLocation.ToString(),
		//        *EndLocation.ToString());
		// UE_LOG(LogTemp, Warning, TEXT("EndLocation: %s, EndLocationGravity: %s"), *EndLocation.ToString(),
		//        *EndLocationGravity.ToString());

		// DrawDebugLine(GetWorld(), CapsuleLocation, EndLocationGravity, FColor::Cyan, false, 3.f, 0, 2);
		// DrawDebugLine(GetWorld(), CapsuleLocation, EndLocation, FColor::Red, false, 3.f, 0, 2);
		// UE_LOG(LogTemp, Warning,
		//        TEXT("SweepDistance: %f, ShrinkHeight: %f, TraceDist: %f, CapsuleLocation: %s, EndLocationGravity: %s"),
		//        SweepDistance, ShrinkHeight, TraceDist, *CapsuleLocation.ToString(), *EndLocationGravity.ToString());
		FHitResult Hit(1.f);
		bBlockingHit = FloorSweepTest(Hit, CapsuleLocation,
		                              EndLocationGravity,
		                              CollisionChannel, CapsuleShape, QueryParams, ResponseParam);

		// UE_LOG(LogTemp, Warning, TEXT("bBlockingHit: %d"), bBlockingHit);

		if (bBlockingHit)
		{
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius.
			if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(CapsuleLocation, Hit.ImpactPoint,
			                                                    CapsuleShape.Capsule.Radius))
			{
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				CapsuleShape.Capsule.Radius = FMath::Max(
					0.f, CapsuleShape.Capsule.Radius - SWEEP_EDGE_REJECT_DISTANCE - UE_KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = SweepDistance + ShrinkHeight;
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight,
					                                             CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					bBlockingHit = FloorSweepTest(Hit, CapsuleLocation,
					                              CapsuleLocation + RotateGravityToWorld(FVector(0.f, 0.f, -TraceDist)),
					                              CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsWalkable(Hit))
			{
				if (SweepResult <= SweepDistance)
				{
					// Hit within test distance.
					OutFloorResult.bWalkableFloor = true;
					return;
				}
			}
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = SweepDistance;
		return;
	}

	// Line trace
	if (LineDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = CapsuleLocation;
		const float TraceDist = LineDistance + ShrinkHeight;
		const FVector Down = RotateGravityToWorld(FVector(0.f, 0.f, -TraceDist));
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down,
		                                                    CollisionChannel, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			if (Hit.Time > 0.f)
			{
				// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
				// We allow negative distances here, because this allows us to pull out of penetrations.
				const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
				const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.bBlockingHit = true;
				if (LineResult <= LineDistance && IsWalkable(Hit))
				{
					OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
					return;
				}
			}
		}
	}

	// No hits were acceptable.
	OutFloorResult.bWalkableFloor = false;
}

#pragma endregion

#pragma region Testing

// #################
// These methods are used only for debug purposes, but we didn't custom anything IMPORTANT in these methods
// Just debug things or logs
//#################

void USRSpaceMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
	// No call to Super::SetPostLandedPhysics(Hit) because we want to use our custom walkable
	if (CharacterOwner)
	{
		if (CanEverSwim() && IsInWater())
		{
			SetMovementMode(MOVE_Swimming);
		}
		else
		{
			const FVector PreImpactAccel = Acceleration + (IsFalling()
				                                               ? -GetGravityDirection() * GetGravityZ()
				                                               : FVector::ZeroVector);
			const FVector PreImpactVelocity = Velocity;

			if (DefaultLandMovementMode == MOVE_Walking ||
				DefaultLandMovementMode == MOVE_NavWalking ||
				DefaultLandMovementMode == MOVE_Falling)
			{
				// UE_LOG(LogTemp, Warning, TEXT("SetPostLandedPhysics"));
				SetMovementMode(GetGroundMovementMode());
			}
			else
			{
				SetDefaultMovementMode();
			}

			ApplyImpactPhysicsForces(Hit, PreImpactAccel, PreImpactVelocity);
		}
	}
}

void USRSpaceMovementComponent::SimulateMovement(float DeltaTime)
{
	UE_LOG(LogTemp, Warning, TEXT("Proxy %s simulating movement"), *GetNameSafe(CharacterOwner));
	if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->
		IsSimulatingPhysics())
	{
		return;
	}

	const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);

	const FRepMovement& ConstRepMovement = CharacterOwner->GetReplicatedMovement();

	// Workaround for replication not being updated initially
	if (bIsSimulatedProxy &&
		ConstRepMovement.Location.IsZero() &&
		ConstRepMovement.Rotation.IsZero() &&
		ConstRepMovement.LinearVelocity.IsZero())
	{
		return;
	}

	// If base is not resolved on the client, we should not try to simulate at all
	if (CharacterOwner->GetReplicatedBasedMovement().IsBaseUnresolved())
	{
		UE_LOG(LogTemp, Verbose,
		       TEXT("Base for simulated character '%s' is not resolved on client, skipping SimulateMovement"),
		       *CharacterOwner->GetName());
		return;
	}

	FVector OldVelocity;
	FVector OldLocation;

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent,
		                                           bEnableScopedMovementUpdates
			                                           ? EScopedUpdate::DeferredUpdates
			                                           : EScopedUpdate::ImmediateUpdates);

		bool bHandledNetUpdate = false;
		if (bIsSimulatedProxy)
		{
			// Handle network changes
			if (bNetworkUpdateReceived)
			{
				bNetworkUpdateReceived = false;
				bHandledNetUpdate = true;
				UE_LOG(LogTemp, Verbose, TEXT("Proxy %s received net update"), *CharacterOwner->GetName());
				if (bNetworkGravityDirectionChanged)
				{
					SetGravityDirection(CharacterOwner->GetReplicatedGravityDirection());
					bNetworkGravityDirectionChanged = false;
				}

				if (bNetworkMovementModeChanged)
				{
					ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
					bNetworkMovementModeChanged = false;
				}
				else if (bJustTeleported || bForceNextFloorCheck)
				{
					// Make sure floor is current. We will continue using the replicated base, if there was one.
					bJustTeleported = false;
					UpdateFloorFromAdjustment();
				}
			}
			else if (bForceNextFloorCheck)
			{
				UpdateFloorFromAdjustment();
			}
		}

		UpdateCharacterStateBeforeMovement(DeltaTime);

		if (MovementMode != MOVE_None)
		{
			//TODO: Also ApplyAccumulatedForces()?
			HandlePendingLaunch();
		}
		ClearAccumulatedForces();

		if (MovementMode == MOVE_None)
		{
			return;
		}

		const bool bSimGravityDisabled = (bIsSimulatedProxy && CharacterOwner->bSimGravityDisabled);
		const bool bZeroReplicatedGroundVelocity = (bIsSimulatedProxy && IsMovingOnGround() && ConstRepMovement.
			LinearVelocity.IsZero());

		// bSimGravityDisabled means velocity was zero when replicated and we were stuck in something. Avoid external changes in velocity as well.
		// Being in ground movement with zero velocity, we cannot simulate proxy velocities safely because we might not get any further updates from the server.
		if (bSimGravityDisabled || bZeroReplicatedGroundVelocity)
		{
			Velocity = FVector::ZeroVector;
		}

		MaybeUpdateBasedMovement(DeltaTime);

		// simulated pawns predict location
		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();

		UpdateProxyAcceleration();

		// May only need to simulate forward on frames where we haven't just received a new position update.
		if (!bHandledNetUpdate || !bNetworkSkipProxyPredictionOnNetUpdate)
		// || !CharacterMovementCVars::NetEnableSkipProxyPredictionOnNetUpdate
		{
			UE_LOG(LogTemp, Verbose, TEXT("Proxy %s simulating movement"), *GetNameSafe(CharacterOwner));
			FStepDownResult StepDownResult;
			MoveSmooth(Velocity, DeltaTime, &StepDownResult);

			// find floor and check if falling
			if (IsMovingOnGround() || MovementMode == MOVE_Falling)
			{
				bool bShouldFindFloor = Velocity.Z <= 0.f;
				if (HasCustomGravity())
				{
					bShouldFindFloor = RotateWorldToGravity(Velocity).Z <= 0.0;
				}

				if (StepDownResult.bComputedFloor)
				{
					CurrentFloor = StepDownResult.FloorResult;
				}
				else if (bShouldFindFloor)
				{
					UE_LOG(LogTemp, Verbose, TEXT("Proxy %s finding floor"), *GetNameSafe(CharacterOwner));
					FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, Velocity.IsZero(), NULL);
				}
				else
				{
					CurrentFloor.Clear();
				}

				// Possible for dynamic movement bases, particularly those that align to slopes while the character does not, to encroach the character.
				// Check to see if we can resolve the penetration in those cases, and if so find the floor.
				if (CurrentFloor.HitResult.bStartPenetrating && MovementBaseUtility::IsDynamicBase(GetMovementBase()))
				{
					// Follows PhysWalking approach for encroachment on floor tests
					FHitResult Hit(CurrentFloor.HitResult);
					if (HasCustomGravity())
					{
						Hit.TraceEnd = Hit.TraceStart - GetGravityDirection() * MAX_FLOOR_DIST;
					}
					else
					{
						Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, MAX_FLOOR_DIST);
					}

					const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
					const bool bResolved = ResolvePenetration(RequestedAdjustment, Hit,
					                                          UpdatedComponent->GetComponentQuat());
					bForceNextFloorCheck |= bResolved;
				}
				else if (!CurrentFloor.IsWalkableFloor())
				{
					if (!bSimGravityDisabled)
					{
						// No floor, must fall.
						if (HasCustomGravity())
						{
							if (RotateWorldToGravity(Velocity).Z <= 0.f || bApplyGravityWhileJumping || !CharacterOwner
								->IsJumpProvidingForce())
							{
								Velocity = NewFallVelocity(Velocity, -GetGravityDirection() * GetGravityZ(),
								                           DeltaTime);
							}
						}
						else if (Velocity.Z <= 0.f || bApplyGravityWhileJumping || !CharacterOwner->
							IsJumpProvidingForce())
						{
							Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaTime);
						}
					}
					SetMovementMode(MOVE_Falling);
				}
				else
				{
					// Walkable floor
					if (IsMovingOnGround())
					{
						AdjustFloorHeight();
						SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
					}
					else if (MovementMode == MOVE_Falling)
					{
						if (CurrentFloor.FloorDist <= MIN_FLOOR_DIST || (bSimGravityDisabled && CurrentFloor.FloorDist
							<= MAX_FLOOR_DIST))
						{
							// Landed
							SetPostLandedPhysics(CurrentFloor.HitResult);
						}
						else
						{
							if (!bSimGravityDisabled)
							{
								// Continue falling.
								Velocity = NewFallVelocity(Velocity, -GetGravityDirection() * GetGravityZ(),
								                           DeltaTime);
							}
							CurrentFloor.Clear();
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("Proxy %s SKIPPING simulate movement"),
			       *GetNameSafe(CharacterOwner));
		}

		UpdateCharacterStateAfterMovement(DeltaTime);

		// consume path following requested velocity
		LastUpdateRequestedVelocity = bHasRequestedVelocity ? RequestedVelocity : FVector::ZeroVector;
		bHasRequestedVelocity = false;

		OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaTime, OldLocation, OldVelocity);


	SaveBaseLocation(); // behaviour before implementing this fix
	// if (CharacterMovementCVars::BasedMovementMode == 0)
	// {
	// 	SaveBaseLocation(); // behaviour before implementing this fix
	// }
	// else
	// {
	// 	MaybeSaveBaseLocation();
	// }
	UpdateComponentVelocity();
	bJustTeleported = false;

	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;
}

#pragma endregion Testing

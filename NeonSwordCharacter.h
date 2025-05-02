#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Blueprint/UserWidget.h"
#include "PlayerWeapons/PlayerSword.h"
#include "Logging/LogMacros.h"
#include "PlayerWeapons/LeftArm.h"
#include "NeonSwordCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USphereComponent;
class UCapsuleComponent;
struct FInputActionValue;

enum class EUtilityState : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	SwordAttack     UMETA(DisplayName = "SwordAttack"),
	SwordDefense    UMETA(DisplayName = "SwordDefense"),
	Shield			UMETA(DisplayName = "Shield"),
	Ultimate	    UMETA(DisplayName = "Ultimate"),
	DashAttack	    UMETA(DisplayName = "DashAttack")
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class ANeonSwordCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh1P;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* WallDetectionCapsule;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Weapons")
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Arms, meta = (AllowPrivateAccess = "true"))
	UChildActorComponent* SwordActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Arms, meta = (AllowPrivateAccess = "true"))
	UChildActorComponent* LeftArmManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Arms, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* HandParticleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Arms, meta = (AllowPrivateAccess = "true"))
	UNiagaraComponent* ElectricityNiagara;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* DefendAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ShieldAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* UltimateAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* DashAttackAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;


public:
	ANeonSwordCharacter();

protected:
	virtual void BeginPlay();

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> PlayerHUDClass;

	UPROPERTY()
	UUserWidget* PlayerHUD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* BrokenGuardSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* BulletFleshHitSoundCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* SwordImpaleSoundCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* SwordDashLoadSoundCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* SwordDashLaunchSoundCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* LandSoundCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* UltimateActivationCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* NotEnoughEnergyCue;

	float GetHP()
	{
		return CurrentHP;
	}

	float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	void GuardBroken();

	UFUNCTION(CallInEditor, BlueprintCallable)
	void CameraShake(float Scale);

	UFUNCTION(CallInEditor, BlueprintCallable)
	void ToggleHUD(bool bShow);

	void Stunned(float Duration);

	void AddChargeAmount(float ChargeToAdd, bool bCanBreakGuard);

	void AddUltChargeAmount(float ChargeToAdd);

	float GetChargeAmount() 
	{
		return ChargeAmount;
	}

	bool GetIsChargeAtMax()
	{
		return bChargeAtMax;
	}

	bool GetIsUncharged()
	{
		return bIsUncharged;
	}

	float GetUltimateChargeAmount()
	{
		return UltimateAmount;
	}

	float GetKineticEnergyAmount()
	{
		return KineticEnergy;
	}

	void SetInvencibility(bool NewState);

	void SetInvencibilityTime(float Time);

	FLinearColor GetCurrentChargeColor();

	FVector GetObjective(float SphereRadius);


	EUtilityState CurrentUtilityState;
	void SetUtilityState(EUtilityState NewState);

	EUtilityState GetUtilityState();

protected:

	void Tick(float DeltaTime);

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	void EndMove();

	FVector2D LastMovementInput;

	bool bPlayerMovementActive = true;

	void UpdateMovementAnimationSpeed();

	void UpdateCameraHeadBob();

	void ResetInvincibility();
	FTimerHandle InvincibilityTimerHandle;

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Arms Offset Section */

	void UpdateArmsOffset();

	float PreviousCameraPitch = 0.0f;
	float PreviousCharacterYaw = 0.0f;
	FVector PreviousCharacterLocation = FVector::ZeroVector;

	/** Jump Section */

	bool bSlideJump = false;
	bool bCanCoyoteJump = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Jump System")
	float CoyoteTimeDuration = 0.2f;

	void StartCoyoteTime();
	void EndCoyoteTime();
	FTimerHandle CoyoteTimerHandle;


	void BufferJumpInput();
	void EndBufferJumpInput();
	FTimerHandle InputBufferJumpTimer;

	bool bBufferJumpAction = false;

	bool bHadDoubleJump = false;

	/** Wall Jump Section */

	UFUNCTION()
	void OnWallCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnWallCapsuleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool bIsHuggingWall = false;

	UPrimitiveComponent* WallHuggedComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Jump System")
	float WallJumpCoolDownTime = 0.5f;

	FTimerHandle WallJumpCoolDownTimer;

	bool bWallJumpInCoolDown = false;

	void ResetWallJumpCoolDown();

	FTimerHandle WallOffTimer;

	void WallOff();

	UAudioComponent* AudioComponentWallJump;

	UAudioComponent* AudioComponentSlide;
	
	UAudioComponent* AudioComponentDash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "NS | Sound Effects")
	USoundCue* WallSlideSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "NS | Sound Effects")
	USoundCue* SlideSound;

	/** Sprint Section */

	void Sprint(const FInputActionValue& Value);
	void StartSprint();

	bool bIsSprintLocked = false;

	float TargetSpeed = 0.f;
	bool bIsTransitioningSpeed = false;
	bool bIsHoldingSprint = false;

	FTimerHandle JustSprintedTimerHandle;
	bool bJustSprinted = false;

	void ResetJustSprinted();

	void SmoothSpeedTransition(float DeltaTime);

	/** Crouch Section */

	void CrouchToggler(const FInputActionValue& Value);

	float TargetCapsuleHeight = 0.f;
	float CrouchInterpSpeed = 13.f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Crouch System")
	float HeightStanding = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Crouch System")
	float HeightCrouched = 60.0f;

	bool bIsCrouching = false;
	bool bIsCrouchTransition = false;
	bool bBufferUncrouch = false;

	void SmoothCrouchingTransition(float DeltaTime);

	bool CanUncrouch(bool bIsJumpingInSlide);

	bool bIsCrouchToggled = true;

	/** Slide Section */

	float SlideSpeed = 900.0f;

	void BeginSlide();
	void Sliding();
	void EndSlide(bool bIsSlideJump);

	FTimerHandle SlideTimerHandle;

	FVector SlideDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Movement")
	bool bIsSliding = false;

	/** Sword Section */

	bool bCanAttack = true;
	bool bCanDefend = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsDefending = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsAttackingSword = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bRecentlyAttacked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bChargeAtMax = false;

	bool bIsUncharged = true;

	bool bIsInvincible = false;

	void StartDefense();
	void EndDefense();

	/** Sword Attacks */

	bool bIsHoldingAttack = false;
	float AttackHoldTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Sword |  Attack", meta = (AllowPrivateAccess = "true"))
	float HeavyAttackThreshold = 0.4f;

	void HoldingSwordAttack();
	void ReleaseSwordAttack();

	//Light Attack

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "NS | Sword |  Attack")
	float ChargedLightExtraDamage = 35.0f;

	void StartSwordLightAttack();
	FTimerHandle SwordAttackTimerHandle;
	void EndSwordLightAttack();

	bool bBufferLightAttack = false;
	void ResetLightAttackBuffer();
	FTimerHandle SwordLightAttackBuffer;

	void SwordAttackBounceOff();

	//Heavy Attack

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "NS | Sword |  Attack")
	float ChargedHeavyExtraDamage = 100.0f;

	void StartSwordHeavyAttack();

	void ReleaseSwordHeavyAttack();
	FTimerHandle SwordHeavyAttackTimerHandle;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Heavy Attack")
	void ReadyToReleaseHeavyAttack();

	void MakeDamageHeavyAttack();

	void EndSwordHeavyAttack();

	void CancelHeavyAttack();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsChargingHeavyAttack = false;

	bool bIsPerformingHeavyAttack = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bHeavyAttackCancelled = false;

	bool bIsReadyToReleaseHeavyAttack = false;

	bool bHeavyDamageDone = false;

	FTimerHandle SwordRecentlyAttackedTimerHandle;
	void ResetRecentlyAttacked();

	/** Sword Defense */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bBufferDefense = false;

	void EndStun();

	/** Dash Attack */

	void StartDashAttack();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Dash Attack")
	void ReleaseDashAttack();

	FVector GetGroundPoint(FVector Origin);

	FVector DashTargetLocation = FVector::Zero();
	FVector DashEnemyTargetLocation = FVector::Zero();

	FTimerHandle DashStopTimerHandle;

	ABasicEnemy* DashTargetEnemy;

	bool CheckEnemyDashed(ABasicEnemy* Enemy);

	void StopDash();

	void StartDashImpaling();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Dash Attack")
	void StopDashImpaling();

	bool bIsDashing = false;
	bool bIsFlyingToPlace = false;

	bool bDashToEnemy = false;

	bool bReadyToRelease = false;
	bool bBufferRelease = false;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Dash Attack")
	void SetReadyToRelease() {
		bReadyToRelease = true;
	}

	void CheckDashReach();

	bool bDashReachedEnemy = false;

	void EndDashAttack();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Dash Attack", meta = (AllowPrivateAccess = "true"))
	float DashAttackEnergyCost = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Dash Attack", meta = (AllowPrivateAccess = "true"))
	float DashAttackHoldTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Dash Attack", meta = (AllowPrivateAccess = "true"))
	float DashAttackMaximumDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Dash Attack", meta = (AllowPrivateAccess = "true"))
	float DashAttackDashForce = 5000.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsHoldingDashAttack = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bIsImpalingEnemy = false;

	void SetPhantomMode(bool NewState);

	bool CheckIfNPCIsSight(AActor* NPC);
	
	/** Kinetic Energy */

	void UpdateArmsEnergy();

	void KineticEnergyUpdate(float DeltaTime);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "NS | Energy", meta = (AllowPrivateAccess = "true"))
	float KineticEnergy = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Energy", meta = (AllowPrivateAccess = "true"))
	float KineticEnergyFactor = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Energy", meta = (AllowPrivateAccess = "true"))
	float KineticEnergyExpireRate = 10.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "NS | Charge", meta = (AllowPrivateAccess = "true"))
	float ChargeAmount = 0.0f;

	float PreviousVelocitySize;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "NS | Ultimate", meta = (AllowPrivateAccess = "true"))
	float UltimateAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	USphereComponent* SphereCollision;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
public:

	void ActivateSwordSphere();

	void DeactivateSwordSphere();

private:

	/** Input Buffering */

	float InputBufferTime = 0.15f;

	/** Movement Settings */

	enum class EMovementState : uint8
	{
		Still		UMETA(DisplayName = "Still"),
		Walking     UMETA(DisplayName = "Walking"),
		Sprinting   UMETA(DisplayName = "Sprinting"),
		Crouching   UMETA(DisplayName = "Crouching"),
		Sliding     UMETA(DisplayName = "Sliding")
	};
	EMovementState CurrentMovementState;

	void SetMovementState(EMovementState NewState);
	FString GetMovementStateString(EMovementState MovementState);

	float CurrentMaxSpeed = 400.0f;

	float TimeSpeedChange = 0.0f;

	bool bSpeedChangeMidAir = false;
	float SpeedBufferedMidAir = 0.f;

	void SetTargetSpeed(EMovementState NewState, float NewTargetSpeed, float TransitionDuration, bool bOverrideAirborne);
	void SetTargetSpeed(float NewTargetSpeed, float TransitionDuration, bool bOverrideAirborne);

	/** HP */

	float CurrentHP;

	void ActivateRegenerateHealth();
	FTimerHandle RegenerationTimer;

	bool bIsRenegerating = false;

	void RegeneratingHealth(float DeltaTime);

	void UpdateHPBar();

	void FellOutOfWorld(const UDamageType& dmgType) override;

	/** Camera Rotation */

	FRotator TargetCameraRotation;

	float CameraLerpDuration = 1.0f;

	float CameraLerpElapsed = 0.0f;

	bool bIsLerpingCamera = false;

	void StartCameraLerpTo(FRotator NewTargetRotation, float Duration);

	void CameraLerpToRotation(float DeltaTime);

private:

	void OnDeath();

	FTimerHandle ReloadLevelTimer;
	void ReloadLevel();

	/** Shield */

	void StartShield();

	bool bShieldInCoolDown = false;
	FTimerHandle ShieldCoolDownTimer;

	void ResetShieldCooldown();

	bool bBufferShield = false;
	FTimerHandle ShieldBufferTimer;

	void ResetShieldBuffer();

public:

	void EndShield();

private:

	/** Ultimate */

	void TriggerUltimate();

public:

	UFUNCTION(BlueprintCallable, Category = "Ultimate")
	void UltimateActivated();

private:

	void EndUltimate();

	void UpdateLeftHandPosition();

	void OnUltimateActive();

	void UpdateHandParticleSpawnRate(float NewSpawnRate);

	bool bIsUltimateActive = false;

	bool bUltimateReadyToAttack = false;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraHeadBobIdle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraHeadBobRunning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraHeadBobSprinting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraHeadBobCrouching;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Speeds")
	float MovementAnimationRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera Control")
	float LookSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera Control")
	bool LookInvertX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera Control")
	bool LookInvertY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Controls")
	bool bHoldCrouch = false;

	UFUNCTION(BlueprintCallable, Category = "NS | Controls")
	void UpdateCrouchHold();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bShieldActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Ultimate")
	bool bIsLeftHandInSword = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Speeds")
	float CrouchingSpeed = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Speeds")
	float WalkingSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Speeds")
	float SprintingSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Speeds")
	float SlideSpeedMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Speeds")
	float FastTimeSpeedChange = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Speeds")
	float RegularTimeSpeedChange = 13.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Speeds")
	float SlowTimeSpeedChange = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Slide System")
	float SlideDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Health")
	float MaxHP = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Health")
	float RegenerationDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Health")
	float RegenerationRate = 1.5f;

	float ActualRegenerationRate = RegenerationRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Health")
	float RegenerationExponentialFactor = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Charge")
	float ChargeGenerationFactor = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Charge")
	UCurveLinearColor* ColorCurveCharges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Ultimate")
	float UltimateGenerationMultiplier = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Ultimate")
	float UltimateConsumptionRate = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Ultimate")
	float UltimateSpawnRateMax = 8000.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FVector LeftHandSocketPosition = FVector::Zero();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FRotator LeftHandSocketRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FTransform LeftHandSocketTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Ultimate")
	float UltimateSmoothTransition = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Ultimate")
	float UltimateSlashCost = -8.0f;

	float UltimateTransitionMax = 0.2f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Sword | Defense")
	float MovementSpeedPenaltyMultiplier = 0.5f;

	float GetPenaltySpeed();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Sword | Defense")
	bool bIsStunned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Sword | Attack")
	bool bBouncedOff = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera | Offset")
	float CameraOffsetX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera | Offset")
	float CameraOffsetY = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NS | Camera | Offset")
	float CameraOffsetZ = 0.0f;

	FTimerHandle StunTimerHandler;

	UMaterialInterface* ArmMaterialInstance;
	UMaterialInstanceDynamic* ArmDynamicMaterial;

	UMaterialInterface* HandMaterialInstance;
	UMaterialInstanceDynamic* HandDynamicMaterial;

	UMaterialInterface* ShieldMaterialInstance;
	UMaterialInstanceDynamic* ShieldDynamicMaterial;

protected:

	/** Overrides */

	virtual void Landed(const FHitResult& Hit) override;

	virtual void Jump() override;

	virtual void CheckJumpInput(float DeltaTime) override;

	void DoubleJumpDash();

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

	/**  Utility */

	float GetGroundPlaneVelocitySize();

	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

private:

	class UAIPerceptionStimuliSourceComponent* StimulusSource;

	void SetupStimulusSource();

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	
	APlayerSword* GetSword() const;

	ALeftArm* GetLeftArm() const
	{
		return  Cast<ALeftArm>(LeftArmManager->GetChildActor());
	}
};
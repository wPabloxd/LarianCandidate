// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeonSwordCharacter.h"
#include "NeonSwordGameMode.h"
#include "NeonSwordProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "PlayerWeapons/PlayerShield.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Gun/BaseGun.h"
#include "PlayerWeapons/LeftArm.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "InputActionValue.h"
#include "PlayerWeapons/LeftArm.h"
#include "PlayerWeapons/PlayerSword.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "UI/PlayerHUD.h"
#include <Kismet/GameplayStatics.h>
#include "Gun/RegularBullet.h"
#include "IA/BasicEnemy.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ANeonSwordCharacter

ANeonSwordCharacter::ANeonSwordCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	//FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->SetupAttachment(RootComponent);

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	SwordActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("Sword"));
	SwordActor->SetupAttachment(Mesh1P, TEXT("RightHandSocket"));

	LeftArmManager = CreateDefaultSubobject<UChildActorComponent>(TEXT("LeftArmManager"));
	LeftArmManager->SetupAttachment(Mesh1P, TEXT("LeftHandSocket"));

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	SetupStimulusSource();

	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereSwordCollision"));
	SphereCollision->SetupAttachment(FirstPersonCameraComponent);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ECollisionChannel OnlyPawn = ECC_GameTraceChannel2;
	SphereCollision->SetCollisionObjectType(OnlyPawn);
	SphereCollision->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

	SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &ANeonSwordCharacter::OnSphereBeginOverlap);

	WallDetectionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("WallDetectionCapsule"));
	WallDetectionCapsule->SetupAttachment(RootComponent);

	WallDetectionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallDetectionCapsule->SetCollisionObjectType(ECC_WorldDynamic);
	WallDetectionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	WallDetectionCapsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);

	//WallDirectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("WallDirectionSphere"));
	//WallDirectionSphere->SetupAttachment(RootComponent);

	//WallDirectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	//WallDirectionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	//WallDirectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	//WallDirectionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);

	HandParticleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandParticleMesh"));
	HandParticleMesh->SetupAttachment(Mesh1P);

	ElectricityNiagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ElectricityNiagara"));
	ElectricityNiagara->SetupAttachment(HandParticleMesh);

	GetMesh()->PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void ANeonSwordCharacter::BeginPlay()
{
	Super::BeginPlay();

	WallDetectionCapsule->OnComponentBeginOverlap.AddDynamic(this, &ANeonSwordCharacter::OnWallCapsuleBeginOverlap);
	WallDetectionCapsule->OnComponentEndOverlap.AddDynamic(this, &ANeonSwordCharacter::OnWallCapsuleEndOverlap);

	ArmMaterialInstance = GetMesh1P()->GetMaterial(3);
	ArmDynamicMaterial = UMaterialInstanceDynamic::Create(ArmMaterialInstance, nullptr);
	GetMesh1P()->SetMaterial(3, ArmDynamicMaterial);

	HandMaterialInstance = GetMesh1P()->GetMaterial(9);
	HandDynamicMaterial = UMaterialInstanceDynamic::Create(HandMaterialInstance, nullptr);
	GetMesh1P()->SetMaterial(9, HandDynamicMaterial);

	ShieldMaterialInstance = GetMesh1P()->GetMaterial(8);
	ShieldDynamicMaterial = UMaterialInstanceDynamic::Create(ShieldMaterialInstance, nullptr);
	GetMesh1P()->SetMaterial(8, ShieldDynamicMaterial);

	HandParticleMesh->SetVisibility(false);

	ElectricityNiagara->SetRelativeRotation(FRotator::ZeroRotator);
	ElectricityNiagara->SetRelativeLocation(FVector::ZeroVector);
	UpdateHandParticleSpawnRate(0.0f);

	//UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.1f);

	if (!AudioComponentWallJump)
	{
		AudioComponentWallJump = NewObject<UAudioComponent>(this);
		AudioComponentWallJump->RegisterComponent();
		AudioComponentWallJump->bAutoDestroy = false;
	}

	AudioComponentWallJump->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AudioComponentWallJump->SetSound(WallSlideSound);

	if (!AudioComponentDash)
	{
		AudioComponentDash = NewObject<UAudioComponent>(this);
		AudioComponentDash->RegisterComponent();
		AudioComponentDash->bAutoDestroy = false;
	}

	AudioComponentDash->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AudioComponentDash->SetSound(SwordDashLoadSoundCue);

	if (!AudioComponentSlide)
	{
		AudioComponentSlide = NewObject<UAudioComponent>(this);
		AudioComponentSlide->RegisterComponent();
		AudioComponentSlide->bAutoDestroy = false;
	}

	AudioComponentSlide->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AudioComponentSlide->SetSound(SlideSound);

	if (!AudioComponentHeartBeat)
	{
		AudioComponentHeartBeat = NewObject<UAudioComponent>(this);
		AudioComponentHeartBeat->RegisterComponent();
		AudioComponentHeartBeat->bAutoDestroy = false;
	}

	AudioComponentHeartBeat->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AudioComponentHeartBeat->SetSound(HeartBeatCue);
	AudioComponentHeartBeat->SetVolumeMultiplier(0.0f);

	TimeSpeedChange = RegularTimeSpeedChange;
	CurrentHP = MaxHP;
	UpdateHPBar(0.0f);

	DeactivateSwordSphere();

	PlayerHUD = CreateWidget<UUserWidget>(GetWorld(), PlayerHUDClass);

	if (PlayerHUD)
	{
		PlayerHUD->AddToViewport();
	}

	APlayerSword* SwordInstance = Cast<APlayerSword>(SwordActor->GetChildActor());
	if (SwordInstance)
	{
		SwordInstance->SetOwner(this);
	}

	ALeftArm* LeftArmInstance = Cast<ALeftArm>(LeftArmManager->GetChildActor());
	if (LeftArmInstance)
	{
		LeftArmInstance->SetOwner(this);
	}

	AddChargeAmount(0.0f, false);
	AddUltChargeAmount(0.0f);

	SetMovementState(EMovementState::Walking);
	SetUtilityState(EUtilityState::Idle);

	ANeonSwordGameMode* GameMode = Cast<ANeonSwordGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode)
	{
		bIsTutorialActivated = GameMode->bIsTutorialActivated;
		bUltimateUnlocked = GameMode->bUltimateUnlocked;
		bDashAttackUnlocked = GameMode->bDashAttackUnlocked;

		if (!GameMode->bShieldUnlocked)
		{
			ToggleLightShield(false);
			AddChargeAmount(100.0f, false);
			bShieldUnlocked = GameMode->bShieldUnlocked;
		}
	}
}

//////////////////////////////////////////////////////////////////////////// Input

void ANeonSwordCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANeonSwordCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ANeonSwordCharacter::EndMove);
	
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANeonSwordCharacter::Look);
		
		//EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &ANeonSwordCharacter::Sprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ANeonSwordCharacter::StartSprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ANeonSwordCharacter::EndSprint);
	
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &ANeonSwordCharacter::CrouchToggler);
		
		EnhancedInputComponent->BindAction(DefendAction, ETriggerEvent::Started, this, &ANeonSwordCharacter::StartDefense);
		EnhancedInputComponent->BindAction(DefendAction, ETriggerEvent::Completed, this, &ANeonSwordCharacter::EndDefense);
	
		//EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ANeonSwordCharacter::HoldingSwordAttack);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ANeonSwordCharacter::StartSwordLightAttack);
		//EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &ANeonSwordCharacter::ReleaseSwordAttack);
		
		EnhancedInputComponent->BindAction(ShieldAction, ETriggerEvent::Started, this, &ANeonSwordCharacter::StartShield);
		EnhancedInputComponent->BindAction(ShieldAction, ETriggerEvent::Completed, this, &ANeonSwordCharacter::EndShield);
		EnhancedInputComponent->BindAction(ShieldAction, ETriggerEvent::Completed, this, &ANeonSwordCharacter::ResetShieldBuffer);
		
		EnhancedInputComponent->BindAction(UltimateAction, ETriggerEvent::Triggered, this, &ANeonSwordCharacter::TriggerUltimate);
	
		EnhancedInputComponent->BindAction(DashAttackAction, ETriggerEvent::Started, this, &ANeonSwordCharacter::StartDashAttack);
		EnhancedInputComponent->BindAction(DashAttackAction, ETriggerEvent::Completed, this, &ANeonSwordCharacter::ReleaseDashAttack);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ANeonSwordCharacter::Tick(float DeltaTime)
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Is Attacking: %s"), bIsDefending ? TEXT("true") : TEXT("false")));
	UpdateMovementAnimationSpeed();
	UpdateArmsOffset();
	UpdateCameraHeadBob();
	UpdateHeartSound(DeltaTime);
	
	HandParticleMesh->AttachToComponent(Mesh1P, FAttachmentTransformRules::SnapToTargetIncludingScale, FName("LeftHandSocket"));
	
	if (bIsTransitioningSpeed) {
		SmoothSpeedTransition(DeltaTime);
	}

	if (bIsCrouchTransition) {
		SmoothCrouchingTransition(DeltaTime);
	}

	if (bBufferUncrouch) {
		CrouchToggler(FInputActionValue());
	}

	if (bIsSliding) {
		Sliding();
	}

	if (bIsRenegerating) {
		RegeneratingHealth(DeltaTime);
	}

	//if (GetVelocity().Size() != 0.0f && !bChargeAtMax){
	//	AddChargeAmount(GetVelocity().Size() * DeltaTime * ChargeGenerationFactor, false);
	//}

	if (bIsUltimateActive) {
		OnUltimateActive();

		if (UltimateAmount <= 0.0f)
		{
			EndUltimate();
		}
	}
	else if (UltimateSmoothTransition > 0.0f) {
		UltimateSmoothTransition -= DeltaTime / UltimateTransitionMax;
		UltimateSmoothTransition = FMath::Clamp(UltimateSmoothTransition, 0.0f, 1.0f);
	}

	if(bBufferLightAttack)
		StartSwordLightAttack();

	if (bBufferShield)
		StartShield();

	KineticEnergyUpdate(DeltaTime);

	if (bIsFlyingToPlace && bDashToEnemy)
		CheckDashReach();

	if (bBufferRelease)
		ReleaseDashAttack();

	if(bIsLerpingCamera)
		CameraLerpToRotation(DeltaTime);

	if (bIsLeftHandInSword) 
		UpdateLeftHandPosition();

	if (bHasToJumpAtMax && GetVelocity().Z <= DoubleJumpMinVelocity) {
		bHasToJumpAtMax = false;
		Jump();
	}

	//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("Health: %f"), GetVelocity().Z));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Current: %f"), GetGroundPlaneVelocitySize()));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Healing: %f"), ActualRegenerationRate));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Is hugging?  %s"), bIsSprintLocked ? TEXT("true") : TEXT("false")));
	//FString MovementStateString = GetMovementStateString(CurrentMovementState);
	//FString MovementStateString = GetUtilityStateString(CurrentUtilityState);
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Current Utilty State: %s"), *MovementStateString));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf
	//(TEXT("Current Movement State: %f"), GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	//DrawDebugCapsule(GetWorld(), GetActorLocation(), GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), GetCapsuleComponent()->GetScaledCapsuleRadius(), FQuat::Identity, FColor::Red, false, -1.0f, 0, 2.0f);
}
FString ANeonSwordCharacter::GetMovementStateString(EMovementState MovementState)
{
	switch (MovementState)
	{
	case EMovementState::Walking:
		return TEXT("Walking");
	case EMovementState::Sprinting:
		return TEXT("Sprinting");
	case EMovementState::Crouching:
		return TEXT("Crouching");
	case EMovementState::Sliding:
		return TEXT("Sliding");
	default:
		return TEXT("Unknown");
	}
}

EUtilityState ANeonSwordCharacter::GetUtilityState()
{
	switch (CurrentUtilityState)
	{
	case EUtilityState::Idle:
		return EUtilityState::Idle;
	case EUtilityState::SwordAttack:
		return EUtilityState::SwordAttack;
	case EUtilityState::SwordDefense:
		return EUtilityState::SwordDefense;
	case EUtilityState::Shield:
		return EUtilityState::Shield;
	case EUtilityState::Ultimate:
		return EUtilityState::Ultimate;
	case EUtilityState::DashAttack:
		return EUtilityState::DashAttack;
	default:
		return EUtilityState::Idle;
	}
}

void ANeonSwordCharacter::Move(const FInputActionValue& Value)
{
	if (!bPlayerMovementActive)
		return;

	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	LastMovementInput = MovementVector;

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void ANeonSwordCharacter::EndMove()
{
	if (!bIsSprintKeyPressed) {

		bIsSprintLocked = false;
		SetTargetSpeed(EMovementState::Walking, WalkingSpeed, RegularTimeSpeedChange, false);
	}

	if (bIsCrouching) 
	{
		SetTargetSpeed(EMovementState::Crouching, CrouchingSpeed, RegularTimeSpeedChange, false);
	}

	LastMovementInput = FVector2D::ZeroVector;
}

void ANeonSwordCharacter::UpdateMovementAnimationSpeed()
{
	float Speed = GetVelocity().Size();
	MovementAnimationRate = Speed / 300.0f;
}

void ANeonSwordCharacter::UpdateCameraHeadBob()
{
	float Speed = GetVelocity().Size();
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (Speed > 700.0f && !GetCharacterMovement()->IsFalling() && !bIsSliding)
	{
		PlayerController->PlayerCameraManager->StartCameraShake(CameraHeadBobSprinting, 1.0f);
	}
	else if (Speed > 450.0f && !GetCharacterMovement()->IsFalling() && !bIsSliding)
	{
		PlayerController->PlayerCameraManager->StartCameraShake(CameraHeadBobRunning, 1.0f);
	}
	else if (Speed > 100.0f && !GetCharacterMovement()->IsFalling() && !bIsSliding)
	{
		PlayerController->PlayerCameraManager->StartCameraShake(CameraHeadBobCrouching, 1.0f);
	}
	else 
	{
		PlayerController->PlayerCameraManager->StartCameraShake(CameraHeadBobIdle, 1.0f);
	}
}

void ANeonSwordCharacter::ResetInvincibility()
{
	bIsInvincible = false;
	GetWorld()->GetTimerManager().ClearTimer(InvincibilityTimerHandle);
}

void ANeonSwordCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	LookAxisVector.X *= LookSensitivity;
	LookAxisVector.Y *= LookSensitivity;

	if (LookInvertX) 
	{
		LookAxisVector.X = -LookAxisVector.X;
	}
	if (LookInvertY)
	{
		LookAxisVector.Y = -LookAxisVector.Y;
	}

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ANeonSwordCharacter::UnlockShield()
{
	ToggleLightShield(true);
	bShieldUnlocked = true;
}

void ANeonSwordCharacter::UpdateArmsOffset()
{
	float CurrentCameraPitch = GetControlRotation().Pitch;
	float CurrentCharacterYaw = GetActorRotation().Yaw;

	float CameraYawDelta = FMath::UnwindDegrees(CurrentCameraPitch - PreviousCameraPitch);
	float CharacterYawDelta = FMath::UnwindDegrees(CurrentCharacterYaw - PreviousCharacterYaw);

	float TargetOffsetX = FMath::Clamp(-CharacterYawDelta * 3.0f, -50.0f, 50.0f);
	float TargetOffsetY = FMath::Clamp(-CameraYawDelta * 3.0f, -50.0f, 50.0f);

	float InterpSpeed = 8.0f;

	PreviousCameraPitch = CurrentCameraPitch;
	PreviousCharacterYaw = CurrentCharacterYaw;

	FVector CurrentCharacterLocation = GetActorLocation();
	FVector CharacterLocationDelta = CurrentCharacterLocation - PreviousCharacterLocation;
	
	FVector CameraForward = GetControlRotation().Vector();
	FVector CameraRight = FRotationMatrix(GetControlRotation()).GetScaledAxis(EAxis::Y);
	FVector CameraUp = FRotationMatrix(GetControlRotation()).GetScaledAxis(EAxis::Z);

	float LocalDeltaX = FVector::DotProduct(CharacterLocationDelta, CameraRight);   // Side movement
	float LocalDeltaY = FVector::DotProduct(CharacterLocationDelta, CameraUp);      // Vertical movement
	float LocalDeltaZ = FVector::DotProduct(CharacterLocationDelta, CameraForward);

	TargetOffsetX += FMath::Clamp(-LocalDeltaX * 3.0f, -50.0f, 50.0f);
	TargetOffsetY += FMath::Clamp(-LocalDeltaY * 3.0f, -50.0f, 50.0f);
	float TargetOffsetZ = FMath::Clamp(-LocalDeltaZ * 3.0f, -100.0f, 100.0f);

	CameraOffsetX = FMath::FInterpTo(CameraOffsetX, TargetOffsetX, GetWorld()->GetDeltaSeconds(), InterpSpeed);
	CameraOffsetY = FMath::FInterpTo(CameraOffsetY, TargetOffsetY, GetWorld()->GetDeltaSeconds(), InterpSpeed);
	CameraOffsetZ = FMath::FInterpTo(CameraOffsetZ, TargetOffsetZ, GetWorld()->GetDeltaSeconds(), InterpSpeed);

	PreviousCharacterLocation = CurrentCharacterLocation;
}

void ANeonSwordCharacter::SetMovementState(EMovementState NewState)
{
	CurrentMovementState = NewState;

	switch (CurrentMovementState)
	{
	case EMovementState::Walking:
		break;
	case EMovementState::Sprinting:
		break;
	case EMovementState::Crouching:
		break;
	case EMovementState::Sliding:
		break;
	}
}

void ANeonSwordCharacter::OnWallCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (GetCharacterMovement()->IsFalling() && !bWallJumpInCoolDown) {
		WallHuggedComponent = OtherComp;

		if (bIsHuggingWall)
			return;

		bHadDoubleJump = JumpCurrentCount <= 1 ? true : false;

		JumpCurrentCount = 1;
		bIsHuggingWall = true;
		
		AudioComponentWallJump->Play();

		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("BE")));
	}
}

void ANeonSwordCharacter::OnWallCapsuleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (WallHuggedComponent == OtherComp) {
		AudioComponentWallJump->Stop();
		GetWorld()->GetTimerManager().SetTimer(WallOffTimer, this, &ANeonSwordCharacter::WallOff, CoyoteTimeDuration, false);
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("EN")));
	}
}

void ANeonSwordCharacter::ResetWallJumpCoolDown()
{
	GetWorld()->GetTimerManager().ClearTimer(WallJumpCoolDownTimer);
	bWallJumpInCoolDown = false;
}

void ANeonSwordCharacter::WallOff()
{
	GetWorld()->GetTimerManager().ClearTimer(WallOffTimer);
	if (GetCharacterMovement()->IsFalling() && bIsHuggingWall) {

		AudioComponentWallJump->Stop();
		bIsHuggingWall = false;
		JumpCurrentCount = bHadDoubleJump ? 1 : 2;
	}
}

void ANeonSwordCharacter::Sprint(const FInputActionValue& Value)
{
	bool IsSprinting = Value.Get<bool>();
	bIsHoldingSprint = IsSprinting == true ? true : false;
	if (bIsCrouching) {
		return;
	}

	if (IsSprinting)
	{
		SetTargetSpeed(EMovementState::Sprinting, SprintingSpeed, RegularTimeSpeedChange, false);
	}
	else
	{
		bJustSprinted = true;
		GetWorld()->GetTimerManager().SetTimer(JustSprintedTimerHandle, this, &ANeonSwordCharacter::ResetJustSprinted, CoyoteTimeDuration, false);
		SetTargetSpeed(EMovementState::Walking, WalkingSpeed, RegularTimeSpeedChange, false);
	}
}

void ANeonSwordCharacter::StartSprint()
{
	//if (LastMovementInput == FVector2D::ZeroVector)
	//	return;

	if (bIsCrouching)
		return;

	bIsSprintLocked = true;
	bIsSprintKeyPressed = true;
	SetTargetSpeed(EMovementState::Sprinting, SprintingSpeed, RegularTimeSpeedChange, false);
}

void ANeonSwordCharacter::EndSprint()
{
	if (LastMovementInput == FVector2D::ZeroVector) {

		if(bIsCrouching)
			SetTargetSpeed(EMovementState::Crouching, CrouchingSpeed, RegularTimeSpeedChange, false);
		else
			SetTargetSpeed(EMovementState::Walking, WalkingSpeed, RegularTimeSpeedChange, false);

		bIsSprintLocked = false;
	}

	bIsSprintKeyPressed = false;
}

void ANeonSwordCharacter::ResetJustSprinted()
{
	GetWorld()->GetTimerManager().ClearTimer(JustSprintedTimerHandle);
	bJustSprinted = false;
}

void ANeonSwordCharacter::SmoothSpeedTransition(float DeltaTime)
{
	float CurrentSpeed = GetCharacterMovement()->MaxWalkSpeed;
	float NewSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed * GetPenaltySpeed(), DeltaTime, TimeSpeedChange);
	// * bIsStunned ? MovementSpeedPenaltyMultiplier : 1
	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
	CurrentMaxSpeed = NewSpeed;

	if (FMath::IsNearlyEqual(NewSpeed, TargetSpeed, 1.0f)) {
		GetCharacterMovement()->MaxWalkSpeed = TargetSpeed * GetPenaltySpeed();
		CurrentMaxSpeed = TargetSpeed;
		bIsTransitioningSpeed = false;

		if (bIsSliding) {
			SetTargetSpeed(EMovementState::Sliding, CrouchingSpeed, SlowTimeSpeedChange, false);
		}
	}
}

void ANeonSwordCharacter::BeginSlide()
{
	AudioComponentSlide->Play();
	bIsSliding = true;
	bPlayerMovementActive = false;
	SlideDirection = GetCharacterMovement()->Velocity;
	SlideDirection.Z = 0.f;
	SlideDirection.Normalize();
	GetWorld()->GetTimerManager().SetTimer(SlideTimerHandle, [this]()
	{
		EndSlide(false);
	}, SlideDuration, false);
	//SetTargetSpeed(EMovementState::Sliding, SlideSpeed, FastTimeSpeedChange, true);
	SlideSpeed = GetVelocity().Size() * SlideSpeedMultiplier;
	SlideSpeed = FMath::Clamp(SlideSpeed, 0.0f, 1800.0f);
	SetTargetSpeed(EMovementState::Sliding, SlideSpeed, FastTimeSpeedChange, true);
}

void ANeonSwordCharacter::Sliding()
{
	AddMovementInput(SlideDirection, 1.0f);
	if (GetVelocity().Size() <= 100.0f) {
		GetWorld()->GetTimerManager().ClearTimer(SlideTimerHandle);
		EndSlide(false);
	}
	if (bSlideJump) {
		GetWorld()->GetTimerManager().ClearTimer(SlideTimerHandle);
		EndSlide(true);
	}
}

void ANeonSwordCharacter::EndSlide(bool bIsSlideJump)
{
	AudioComponentSlide->Stop();
	bSlideJump = false;
	bIsSliding = false;
	bPlayerMovementActive = true;
	SlideDirection = FVector::ZeroVector;
	if (bIsSlideJump) {
		SetTargetSpeed(CurrentMaxSpeed, FastTimeSpeedChange, true);
	}
	else {
		SetTargetSpeed(EMovementState::Crouching, CrouchingSpeed, RegularTimeSpeedChange, false);
	}
}

void ANeonSwordCharacter::StartDefense()
{
	//if (!bCanDefend)
		//return;

	if (bIsUltimateActive)
		return;

	//if (bIsDashing)
	//	return;

	//if (GetSword()->GetSwordState() == ESwordState::Attacking || GetLeftArm()->GetCurrentState() == ELeftArmState::Shield) {
	if (CurrentUtilityState == EUtilityState::SwordAttack || CurrentUtilityState == EUtilityState::Shield || CurrentUtilityState == EUtilityState::DashAttack) {
		bBufferDefense = true;
		return;
	}
	if (!bIsStunned) 
	{
		GetSword()->SetSwordState(ESwordState::Defending);
		SetUtilityState(EUtilityState::SwordDefense);
		bIsDefending = true;
		bBufferDefense = true;
	}
}

void ANeonSwordCharacter::EndDefense()
{
	bIsDefending = false;
	bBufferDefense = false;

	if (CurrentUtilityState == EUtilityState::SwordDefense) {
		SetUtilityState(EUtilityState::Idle);
	}
}

void ANeonSwordCharacter::UpdateArmsEnergy()
{
	ArmDynamicMaterial->SetVectorParameterValue(TEXT("LightsColor"), GetCurrentChargeColor());
	HandDynamicMaterial->SetVectorParameterValue(TEXT("LightsColor"), GetCurrentChargeColor());
	ShieldDynamicMaterial->SetVectorParameterValue(TEXT("ForceFieldColor"), GetCurrentChargeColor());
}

void ANeonSwordCharacter::HoldingSwordAttack()
{
	if (!bCanAttack)
		return;
	if (bIsChargingHeavyAttack)
		return;
	if (bIsStunned)
		return;

	bIsHoldingAttack = true;
	AttackHoldTime += GetWorld()->GetDeltaSeconds();

	if (AttackHoldTime >= HeavyAttackThreshold)
	{
		StartSwordHeavyAttack(); //DEACTIVATED
	}
}

void ANeonSwordCharacter::ReleaseSwordAttack()
{
	bIsHoldingAttack = false;
	AttackHoldTime = 0.0f;

	if (bIsChargingHeavyAttack) 
		ReleaseSwordHeavyAttack();
	else
		StartSwordLightAttack();

	//if (AttackHoldTime >= HeavyAttackThreshold)
	//{
	//	StartSwordHeavyAttack();
	//}
	//else
	//{
	//	StartSwordLightAttack();
	//}
}

void ANeonSwordCharacter::StartSwordLightAttack()
{
	if (!bCanAttack)
		return;

	if (bIsDashing)
		return;

	//if (bIsAttackingSword || bIsPerformingHeavyAttack) {
	if (bIsAttackingSword) {
		//bBufferLightAttack = true;
		GetWorld()->GetTimerManager().SetTimer(SwordLightAttackBuffer, this, &ANeonSwordCharacter::ResetLightAttackBuffer, 0.1f, false);
		return;
	}
	if (bIsStunned)
		return;

	if (!bIsUltimateActive) {

		SetUtilityState(EUtilityState::SwordAttack);
		GetWorld()->GetTimerManager().SetTimer(SwordAttackTimerHandle, this, &ANeonSwordCharacter::EndSwordLightAttack, 0.2f, false);
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("ATTACK")));
		GetSword()->StartAttack(ChargedLightExtraDamage);
	}
	else {
		if (!bUltimateReadyToAttack)
			return;

		GetWorld()->GetTimerManager().SetTimer(SwordAttackTimerHandle, this, &ANeonSwordCharacter::EndSwordLightAttack, 0.3f, false);
		GetSword()->StartUltAttack();
		UltimateAmount += UltimateSlashCost;
		UltimateAmount = FMath::Clamp(UltimateAmount, 0.0f, 100.0f);
	}

	AttackHoldTime = 0.0f;
	bIsDefending = false;
	bIsAttackingSword = true;
	bBufferLightAttack = false;

}

void ANeonSwordCharacter::EndSwordLightAttack()
{
	bBouncedOff = false;

	bRecentlyAttacked = true;
	GetWorld()->GetTimerManager().SetTimer(SwordRecentlyAttackedTimerHandle, this, &ANeonSwordCharacter::ResetRecentlyAttacked, 0.6f, false);
	
	bIsAttackingSword = false;
	GetWorld()->GetTimerManager().ClearTimer(SwordAttackTimerHandle);

	GetSword()->EndAttack();

	//if (bBufferDefense) {
	//	GetSword()->SetSwordState(ESwordState::Defending);
	//	//SetUtilityState(EUtilityState::SwordDefense);
	//	StartDefense();
	//}
	if(CurrentUtilityState == EUtilityState::SwordAttack)
	{
		GetSword()->SetSwordState(ESwordState::Idle);
		SetUtilityState(EUtilityState::Idle);
	}
}

void ANeonSwordCharacter::ResetLightAttackBuffer()
{
	bBufferLightAttack = false;
}

void ANeonSwordCharacter::SwordAttackBounceOff()
{
	bBouncedOff = true;
	CameraShake(0.75f);
	GetWorld()->GetTimerManager().ClearTimer(SwordAttackTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(SwordAttackTimerHandle, this, &ANeonSwordCharacter::EndSwordLightAttack, 0.6f, false);
}

void ANeonSwordCharacter::StartSwordHeavyAttack()
{
	bIsPerformingHeavyAttack = true;
	bHeavyAttackCancelled = false;
	AttackHoldTime = 0.0f;
	bIsChargingHeavyAttack = true;
	bIsReadyToReleaseHeavyAttack = false;
	bIsDefending = false;
	bBufferLightAttack = false;
	//bIsAttackingSword = true;
}

void ANeonSwordCharacter::ReleaseSwordHeavyAttack()
{
	bIsChargingHeavyAttack = false;

	if (bIsReadyToReleaseHeavyAttack) {

		MakeDamageHeavyAttack();
	}
	else
		CancelHeavyAttack();
}

void ANeonSwordCharacter::ReadyToReleaseHeavyAttack()
{
	bIsReadyToReleaseHeavyAttack = true;

	if (bIsHoldingAttack)
		return;

	MakeDamageHeavyAttack();
}

void ANeonSwordCharacter::MakeDamageHeavyAttack()
{
	if (bHeavyAttackCancelled)
		return;
	if (bHeavyDamageDone)
		return;
	bHeavyDamageDone = true;
	GetWorld()->GetTimerManager().SetTimer(SwordHeavyAttackTimerHandle, this, &ANeonSwordCharacter::EndSwordHeavyAttack, 0.4f, false);
	GetSword()->StartAttack(ChargedHeavyExtraDamage);
}

void ANeonSwordCharacter::EndSwordHeavyAttack()
{
	AttackHoldTime = 0.0f;
	bRecentlyAttacked = true;
	GetWorld()->GetTimerManager().SetTimer(SwordRecentlyAttackedTimerHandle, this, &ANeonSwordCharacter::ResetRecentlyAttacked, 0.6f, false);

	bIsReadyToReleaseHeavyAttack = false;

	//bIsAttackingSword = false;
	bIsPerformingHeavyAttack = false;

	bHeavyDamageDone = false;

	GetSword()->EndAttack();

	//if (bBufferDefense) {
	//	GetSword()->SetSwordState(ESwordState::Defending);
	//	StartDefense();
	//}
	//else {
		GetSword()->SetSwordState(ESwordState::Idle);
	//}
}

void ANeonSwordCharacter::CancelHeavyAttack()
{
	bHeavyAttackCancelled = true;
	bIsChargingHeavyAttack = false;
	EndSwordHeavyAttack();
}

void ANeonSwordCharacter::ResetRecentlyAttacked()
{
	bRecentlyAttacked = false;
}

void ANeonSwordCharacter::UpdateCrouchHold()
{
	if (bIsCrouching && !bHoldCrouch) {
		bIsCrouchToggled = false;
	}
}

void ANeonSwordCharacter::CrouchToggler(const FInputActionValue& Value)
{
	bool IsInputCrouched = Value.Get<bool>();

	bIsCrouchToggled = !bIsCrouchToggled;
	if (!bHoldCrouch)
	{
		if (bIsCrouchToggled) {
			return;
		}

		if (bIsCrouching)
		{
			IsInputCrouched = !IsInputCrouched;
		}
	}

	bIsCrouchTransition = true;
	if (IsInputCrouched)
	{
		bBufferUncrouch = false;
		TargetCapsuleHeight = HeightCrouched;
		bIsCrouching = true;
		if (((bJustSprinted && GetGroundPlaneVelocitySize() >= 500.0f) || GetGroundPlaneVelocitySize() >= 600.0f) && GetCharacterMovement()->IsMovingOnGround() && !bIsSliding) {
			BeginSlide();
		}
		else if (!bIsSliding) {
			SetTargetSpeed(EMovementState::Crouching,CrouchingSpeed, RegularTimeSpeedChange, false);
		}
	}
	else
	{
		if (CanUncrouch(false)) {

			TargetCapsuleHeight = HeightStanding;
			bIsCrouching = false;
			if (bIsSprintLocked) {
				SetTargetSpeed(EMovementState::Sprinting, SprintingSpeed, RegularTimeSpeedChange, false);
			}
			else
			{
				SetTargetSpeed(EMovementState::Walking, WalkingSpeed, RegularTimeSpeedChange, false);
			}
			bBufferUncrouch = false;
		}
		else {
			bBufferUncrouch = true;
		}
	}
}

void ANeonSwordCharacter::SmoothCrouchingTransition(float DeltaTime)
{
	float CurrentHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	float NewHeight = FMath::FInterpTo(CurrentHeight, TargetCapsuleHeight, DeltaTime, CrouchInterpSpeed);
	GetCapsuleComponent()->SetCapsuleHalfHeight(NewHeight);

	GetFirstPersonCameraComponent()->AddRelativeLocation(FVector(0.0f, 0.0f, NewHeight - CurrentHeight));

	if (!GetCharacterMovement()->IsFalling()) 
		AddActorLocalOffset(FVector(0.0f, 0.0f, (NewHeight - CurrentHeight) * 2));

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Other Speed"));
	if (FMath::IsNearlyEqual(NewHeight, TargetCapsuleHeight, 1.0f)) {
		bIsCrouchTransition = false;
		GetFirstPersonCameraComponent()->SetRelativeLocation(FVector(0.0f, 0.0f, bIsCrouching == true ? (60.0f - (HeightStanding - HeightCrouched)) : 60.0f));
		GetCapsuleComponent()->SetCapsuleHalfHeight(bIsCrouching == true ? HeightCrouched : HeightStanding);
	}
}

bool ANeonSwordCharacter::CanUncrouch(bool bIsJumpingInSlide)
{
	if (bIsSliding && !bIsJumpingInSlide)
		return false;

	FVector Start = GetActorLocation();
	FVector End = Start + FVector(0, 0, 97);
	float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius() - 5.0f;

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(CapsuleRadius),
		CollisionParams
	);

	return !bHit;
}

void ANeonSwordCharacter::SetUtilityState(EUtilityState NewState)
{
	CurrentUtilityState = NewState;

	switch (NewState)
	{
	case EUtilityState::Idle:

		if (bBufferShield) {
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Egcudo"));
			StartShield();
			//bBufferShield = false;
			break;
		}
		else if (bBufferDefense) {
			GetSword()->SetSwordState(ESwordState::Defending);
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("epadita"));
			StartDefense();
			//bBufferDefense = false;
			break;
		}
	
		break;

	case EUtilityState::SwordAttack:
		break;

	case EUtilityState::SwordDefense:
		break;

	case EUtilityState::Shield:
		break;

	case EUtilityState::Ultimate:
		break;

	case EUtilityState::DashAttack:
		EndShield();
		bShieldActive = false;
		bIsDefending = false;
		break;

	default:
		break;
	}
}

void ANeonSwordCharacter::SetTargetSpeed(EMovementState NewState, float NewTargetSpeed, float TransitionDuration, bool bOverrideAirborne)
{
	if (GetCharacterMovement()->IsMovingOnGround() || bOverrideAirborne)
	{
		TargetSpeed = NewTargetSpeed;
		TimeSpeedChange = TransitionDuration;
		bIsTransitioningSpeed = true;
		SetMovementState(NewState);
	}
	else
	{
		SpeedBufferedMidAir = NewTargetSpeed;
		bSpeedChangeMidAir = true;
	}
}

void ANeonSwordCharacter::SetTargetSpeed(float NewTargetSpeed, float TransitionDuration, bool bOverrideAirborne)
{
	if (GetCharacterMovement()->IsMovingOnGround() || bOverrideAirborne)
	{
		TargetSpeed = NewTargetSpeed;
		TimeSpeedChange = TransitionDuration;
		bIsTransitioningSpeed = true;
		const float Tolerance = 1.00f;
		if (FMath::IsNearlyEqual(NewTargetSpeed, WalkingSpeed, Tolerance))
		{
			SetMovementState(EMovementState::Walking);
		}
		else if (FMath::IsNearlyEqual(NewTargetSpeed, SprintingSpeed, Tolerance))
		{
			SetMovementState(EMovementState::Sprinting);
		}
		else if (FMath::IsNearlyEqual(NewTargetSpeed, CrouchingSpeed, Tolerance))
		{
			SetMovementState(EMovementState::Crouching);
		}
		else if (FMath::IsNearlyEqual(NewTargetSpeed, SlideSpeed, Tolerance))
		{
			SetMovementState(EMovementState::Sliding);
		}
		else if (FMath::IsNearlyEqual(NewTargetSpeed, 0.0f, Tolerance))
		{
			SetMovementState(EMovementState::Still);
		}
		else
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Other Speed"));
		}
	}
	else
	{
		SpeedBufferedMidAir = NewTargetSpeed;
		bSpeedChangeMidAir = true;
	}
}

float ANeonSwordCharacter::GetPenaltySpeed()
{
	return bIsStunned ? MovementSpeedPenaltyMultiplier : 1;
}

void ANeonSwordCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	UGameplayStatics::PlaySoundAtLocation(this, LandSoundCue, GetActorLocation());

	GetWorld()->GetTimerManager().ClearTimer(WallOffTimer);
	AudioComponentWallJump->Stop();
	bIsHuggingWall = false;

	if (bSpeedChangeMidAir) {
		SetTargetSpeed(SpeedBufferedMidAir, RegularTimeSpeedChange, true);
		SpeedBufferedMidAir = 0.f;
		bSpeedChangeMidAir = false;
	}

	if (GetGroundPlaneVelocitySize() >= 650.0f && bIsCrouching) {
		BeginSlide();
	}
}

void ANeonSwordCharacter::BufferJumpInput()
{
	bBufferJumpAction = true;
	GetWorld()->GetTimerManager().SetTimer(InputBufferJumpTimer, this, &ANeonSwordCharacter::EndBufferJumpInput, InputBufferTime, false);
}

void ANeonSwordCharacter::EndBufferJumpInput()
{
	bBufferJumpAction = false;
	GetWorld()->GetTimerManager().ClearTimer(InputBufferJumpTimer);

	StopJumping();
}

void ANeonSwordCharacter::Jump()
{
 	bool bCanUncrouch = CanUncrouch(bIsSliding);

	if (bIsSliding)
	{
		bSlideJump = bCanUncrouch;

		if (!bSlideJump) return;
	}
	else if (!bCanUncrouch)
	{
		return;
	}

	if (GetVelocity().Z > DoubleJumpMinVelocity) {
		bHasToJumpAtMax = true;
		return;
	}

	Super::Jump();
}

void ANeonSwordCharacter::CheckJumpInput(float DeltaTime)
{
	JumpCurrentCountPreJump = JumpCurrentCount;

	if (GetCharacterMovement())
	{
		if (bPressedJump)
		{
			if (JumpCurrentCount == JumpMaxCount) {
				BufferJumpInput();
			}
			// If this is the first jump and we're already falling,
			// then increment the JumpCount to compensate.
			const bool bFirstJump = JumpCurrentCount == 0;
			if (bFirstJump && GetCharacterMovement()->IsFalling() && !bCanCoyoteJump)
			{
				JumpCurrentCount++;
			}

			const bool bDidJump = CanJump() && GetCharacterMovement()->DoJump(bClientUpdating, DeltaTime);
			if (bDidJump)
			{
				// Transition from not (actively) jumping to jumping.
				if (!bWasJumping)
				{
					JumpCurrentCount++;

					if (JumpCurrentCount > 1) {
						DoubleJumpDash();
						//bWasJumping = bDidJump;
						//return;
					}
					if (bIsHuggingWall) {
						//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("CD")));
						bWallJumpInCoolDown = true;
						GetWorld()->GetTimerManager().SetTimer(WallJumpCoolDownTimer, this, &ANeonSwordCharacter::ResetWallJumpCoolDown, WallJumpCoolDownTime, false);
					}
					
					JumpForceTimeRemaining = GetJumpMaxHoldTime();


					OnJumped();
				}
			}

			bWasJumping = bDidJump;
		}
	}
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Zalto: %i"), JumpCurrentCount));
}

void ANeonSwordCharacter::DoubleJumpDash()
{
	FRotator CameraRotation = GetControlRotation();
	FRotator YawRotation(0, CameraRotation.Yaw, 0);

	FVector ForwardVector = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	FVector RightVector = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	FVector MovementDirection = (ForwardVector * LastMovementInput.Y) + (RightVector * LastMovementInput.X);

	MovementDirection.Z = 0.0f;
	MovementDirection.Normalize();

	float ActualMomentum = GetVelocity().Length();

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("X: %f"), GetVelocity().Length()));
	if (LastMovementInput != FVector2D::ZeroVector)
		LaunchCharacter(MovementDirection * GetVelocity().Length(), true, false);
}

void ANeonSwordCharacter::StartCoyoteTime()
{
	bCanCoyoteJump = true;
	GetWorld()->GetTimerManager().SetTimer(CoyoteTimerHandle, this, &ANeonSwordCharacter::EndCoyoteTime, CoyoteTimeDuration, false);
}

void ANeonSwordCharacter::EndCoyoteTime()
{
	bCanCoyoteJump = false;
}

float ANeonSwordCharacter::GetGroundPlaneVelocitySize()
{
	return FMath::Sqrt(FMath::Square(GetVelocity().X) + FMath::Square(GetVelocity().Y));
}

void ANeonSwordCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (GetCharacterMovement()->IsMovingOnGround())
	{
		if (bBufferJumpAction) {
			Jump();
		}

		GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;
	}
	else
	{
		GetCharacterMovement()->bUseFlatBaseForFloorChecks = false;
	}
	if (GetCharacterMovement()->MovementMode == MOVE_Falling)
	{
		StartCoyoteTime();
	}
}

void ANeonSwordCharacter::SetupStimulusSource()
{
	StimulusSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("Stimulus"));
	if (StimulusSource) {
		StimulusSource->RegisterForSense(TSubclassOf<UAISense_Sight>());
		StimulusSource->RegisterWithPerceptionSystem();
	}
}

APlayerSword* ANeonSwordCharacter::GetSword() const
{
	if (SwordActor && SwordActor->GetChildActor())
	{
		return Cast<APlayerSword>(SwordActor->GetChildActor());
	}

	return nullptr;
}

float ANeonSwordCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsInvincible)
		return DamageAmount;
	//if (bIsDefending && !GetIsChargeAtMax()) {
	if (bIsDefending) {

		ARegularBullet* Bullet = Cast<ARegularBullet>(DamageCauser);
		ABaseGun* Gun = Cast<ABaseGun>(Bullet->OwningActor);

		if (Gun->GetIsDefectable()) 
		{
			GetSword()->BulletRicocheted(DamageAmount);
			return DamageAmount;
		}
	}
	else if (GetLeftArm()->GetCurrentState() == ELeftArmState::Shield) 
	{
		ARegularBullet* Bullet = Cast<ARegularBullet>(DamageCauser);
		ABaseGun* Gun = Cast<ABaseGun>(Bullet->OwningActor);
		if (Gun) 
		{
			GetLeftArm()->GetShield()->AddBullet(Gun->GetBulletModel(), Gun->GetBulletDamage(), Gun->GetBulletSize());
		}
		return DamageAmount;
	}

	UGameplayStatics::PlaySoundAtLocation(this, BulletFleshHitSoundCue, GetActorLocation());
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("DAMAGE: %f"), DamageAmount));
	CurrentHP -= DamageAmount;
	CurrentHP = FMath::Clamp(CurrentHP, 0, MaxHP);

	bIsRenegerating = false;
	GetWorld()->GetTimerManager().ClearTimer(RegenerationTimer);
	GetWorld()->GetTimerManager().SetTimer(RegenerationTimer, this, &ANeonSwordCharacter::ActivateRegenerateHealth, RegenerationDelay, false);
	
	HeartBeatRate += DamageAmount;

	UpdateHPBar(DamageAmount);
	CameraShake(DamageAmount * 0.1f);

	if (CurrentHP <= 0)
	{
		OnDeath();
	}

	return DamageAmount;
}

void ANeonSwordCharacter::ActivateRegenerateHealth()
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("HEAL")));
	GetWorld()->GetTimerManager().ClearTimer(RegenerationTimer);
	bIsRenegerating = true;
	ActualRegenerationRate = RegenerationRate;
}

void ANeonSwordCharacter::RegeneratingHealth(float DeltaTime)
{
	if (CurrentHP < MaxHP)
	{
		CurrentHP = FMath::Min(CurrentHP + ActualRegenerationRate * DeltaTime, MaxHP);
		UpdateHPBar(-ActualRegenerationRate * DeltaTime);
		HeartBeatRate -= ActualRegenerationRate * DeltaTime;
		ActualRegenerationRate *=  1 + RegenerationExponentialFactor * DeltaTime;
		//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("Health: %f, Regen Rate: %f"), CurrentHP, RegenerationRate));
	}
	else
	{
		HeartBeatRate = 60.0f;
		ActualRegenerationRate = RegenerationRate;
		GetWorld()->GetTimerManager().ClearTimer(RegenerationTimer);
		bIsRenegerating = false;
	}
}

void ANeonSwordCharacter::UpdateHPBar(float Damage)
{
	if (PlayerHUD)
	{
		UPlayerHUD* PlayerWidget = Cast<UPlayerHUD>(PlayerHUD);

		if (PlayerWidget)
		{
			PlayerWidget->UpdateHealth(CurrentHP);
			PlayerWidget->UpdateDamageScreenState(CurrentHP);
			PlayerWidget->GetDamageScreenUpdate(Damage);
		}
	}
}

void ANeonSwordCharacter::FellOutOfWorld(const UDamageType& dmgType)
{
	ReloadLevel();
}

void ANeonSwordCharacter::UpdateHeartSound(float DeltaTime)
{
	TimeSinceLastBeat += DeltaTime;

	float NewPitch = FMath::GetMappedRangeValueClamped(FVector2D(60.0f, 160.0f), FVector2D(1.00f, 2.67f), HeartBeatRate);
	
	if (HeartBeatRate > 100.0f) {
		float NewVolume = FMath::GetMappedRangeValueClamped(FVector2D(100.0f, 160.0f), FVector2D(0.0f, 1.5f), HeartBeatRate);
		AudioComponentHeartBeat->SetVolumeMultiplier(NewVolume);
		AudioComponentHeartBeat->SetPitchMultiplier(NewPitch);
		//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("Hola %f"), NewPitch));
	
		if (TimeSinceLastBeat >= 1 / NewPitch)
		{
			UPlayerHUD* PlayerWidget = Cast<UPlayerHUD>(PlayerHUD);
			PlayerWidget->UpdateDamageBeat(HeartBeatRate);

			float Margin = TimeSinceLastBeat - 1 / NewPitch;
			TimeSinceLastBeat -= Margin;
			TimeSinceLastBeat -= 1 / NewPitch;
			AudioComponentHeartBeat->Play();
			//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("Hola %f"), NewPitch));
		}
	}
}

void ANeonSwordCharacter::StartCameraLerpTo(FRotator NewTargetRotation, float Duration)
{
	TargetCameraRotation = NewTargetRotation;
	CameraLerpDuration = Duration;
	CameraLerpElapsed = 0.0f;
	bIsLerpingCamera = true;
}

void ANeonSwordCharacter::CameraLerpToRotation(float DeltaTime)
{
	CameraLerpElapsed += DeltaTime;
	float Alpha = FMath::Clamp(CameraLerpElapsed / CameraLerpDuration, 0.0f, 1.0f);

	FRotator CurrentRotation = GetControlRotation();

	FRotator NewRotation = FMath::Lerp(CurrentRotation, TargetCameraRotation, Alpha);
	//FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetCameraRotation, DeltaTime, 1.0f / DeltaTime);
	if (GetController())
	{
		GetController()->SetControlRotation(NewRotation);
	}

	if (Alpha >= 1.0f)
	{
		bIsLerpingCamera = false;
		if (GetController())
		{
			GetController()->SetControlRotation(TargetCameraRotation);
		}
	}
}

void ANeonSwordCharacter::OnDeath()
{
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	GetWorld()->GetTimerManager().SetTimer(ReloadLevelTimer, this, &ANeonSwordCharacter::ReloadLevel, 0.5f, false);

	//PLACEHOLDER
	GetMesh1P()->SetVisibility(false);
	GetCharacterMovement()->DisableMovement();
	GetController()->SetIgnoreLookInput(true);
	GetSword()->GetSwordMesh()->SetVisibility(false, true);

}

void ANeonSwordCharacter::ReloadLevel()
{
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()));
}

void ANeonSwordCharacter::ToggleLightShield(bool bIsActive)
{
	ShieldDynamicMaterial->SetScalarParameterValue(TEXT("Alpha"), bIsActive ? 0.374407f : 1.0f);
	HandDynamicMaterial->SetScalarParameterValue(TEXT("LightIntensity"), bIsActive ? 50000.0f : 0.0f);
}

void ANeonSwordCharacter::StartShield()
{
	if (!bShieldUnlocked)
		return;

	//if (GetChargeAmount() <= 2.5f)
	//	return;
	if (bShieldInCoolDown || CurrentUtilityState == EUtilityState::SwordAttack || CurrentUtilityState == EUtilityState::Shield || CurrentUtilityState == EUtilityState::DashAttack) {
		bBufferShield = true;
		GetWorld()->GetTimerManager().SetTimer(ShieldBufferTimer, this, &ANeonSwordCharacter::ResetShieldBuffer, 0.2f, false);
		return;
	}

	if (bIsDashing)
		return;

	if (bIsStunned)
		return;

	if (bIsUltimateActive)
		return;

	if (bIsChargingHeavyAttack)
		return;

	GetSword()->SetSwordState(ESwordState::Idle);
	GetLeftArm()->SetCurrentState(ELeftArmState::Shield);
	SetUtilityState(EUtilityState::Shield);

	bBufferShield = true;
	bShieldActive = true;
	bCanDefend = false;
	bIsDefending = false;
	bCanAttack = false;

	//EndDefense();
	//EndSwordAttack();

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("START")));
}

void ANeonSwordCharacter::ResetShieldCooldown()
{
	bShieldInCoolDown = false;
	GetWorld()->GetTimerManager().ClearTimer(ShieldCoolDownTimer);
}

void ANeonSwordCharacter::ResetShieldBuffer()
{
	bBufferShield = false;
	GetWorld()->GetTimerManager().ClearTimer(ShieldBufferTimer);
}

void ANeonSwordCharacter::EndShield()
{
	if (!bShieldActive)
		return;

	bShieldActive = false;
	bCanDefend = true;
	bCanAttack = true;

	if (CurrentUtilityState == EUtilityState::Shield) {
		bBufferShield = false;
		SetUtilityState(EUtilityState::Idle);
	}

	//if (bBufferDefense) {
	//	GetSword()->SetSwordState(ESwordState::Defending);
	//	StartDefense();
	//}

	if (GetChargeAmount() == 0.0f && !bShieldInCoolDown) {
		bShieldInCoolDown = true;
		GetWorld()->GetTimerManager().SetTimer(ShieldCoolDownTimer, this, &ANeonSwordCharacter::ResetShieldCooldown, 0.3f, false);
	}
	GetLeftArm()->SetCurrentState(ELeftArmState::Idle);


	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("END")));
}

void ANeonSwordCharacter::TriggerUltimate()
{
	if(bIsAttackingSword)
		return;

	if (bIsDashing)
		return;

	if (bIsUltimateActive)
		return;

	if (UltimateAmount < 100.0f)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("NO HAY")));
		return;
	}

	UPlayerHUD* PlayerWidget = Cast<UPlayerHUD>(PlayerHUD);
	PlayerWidget->ToggleOverdrive(false);

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("ULTEAU")));
	bIsLeftHandInSword = true;
	SetInvencibility(true);
	bIsUltimateActive = true;

	GetSword()->SetSwordState(ESwordState::UltIdle);
	GetLeftArm()->SetCurrentState(ELeftArmState::Ultimate);
	SetUtilityState(EUtilityState::Ultimate);
}

void ANeonSwordCharacter::UltimateActivated()
{
	UpdateHandParticleSpawnRate(0.0f);
	bUltimateReadyToAttack = true;
	UGameplayStatics::PlaySoundAtLocation(this, UltimateActivationCue, GetActorLocation());
	CameraShake(1.0f);
	AddChargeAmount(100.0f, false);
	GetSword()->ToggleElectricParticles(true);
	GetSword()->UpdateElectricSpawnRate(UltimateSpawnRateMax, 100.0f);
	GetSword()->ToggleUltimateIdleSound(true);
}

void ANeonSwordCharacter::UpdateLeftHandPosition()
{
	FTransform BoneWorldTransform = GetMesh()->GetBoneTransform(GetMesh()->GetBoneIndex(FName("hand_r")));

	LeftHandSocketPosition = GetSword()->GetSwordMesh()->GetSocketLocation(FName("LeftHandSocket"));
	LeftHandSocketRotation = GetSword()->GetSwordMesh()->GetSocketRotation(FName("LeftHandSocket"));
	LeftHandSocketTransform = GetSword()->GetSwordMesh()->GetSocketTransform(FName("LeftHandSocket"), RTS_World);
	//FTransform SocketWorldTransform = GetSword()->GetSwordMesh()->GetSocketTransform(FName("LeftHandSocket"), RTS_World);
	//LeftHandSocketTransform = BoneWorldTransform.GetRelativeTransform(SocketWorldTransform);

	if (UltimateSmoothTransition < 1.0f)
	{
		UltimateSmoothTransition += GetWorld()->GetDeltaSeconds() / UltimateTransitionMax;
		UltimateSmoothTransition = FMath::Clamp(UltimateSmoothTransition, 0.0f, 1.0f);
	}
}

void ANeonSwordCharacter::OnUltimateActive()
{
	UltimateAmount -= GetWorld()->GetDeltaSeconds() * UltimateConsumptionRate;
	
	float UltAmountRemaped = UltimateAmount * UltimateSpawnRateMax / 100;
	GetSword()->UpdateElectricSpawnRate(UltAmountRemaped, UltimateAmount);

	if (PlayerHUD)
	{
		UPlayerHUD* PlayerWidget = Cast<UPlayerHUD>(PlayerHUD);
		if (PlayerWidget)
		{
			PlayerWidget->UpdateUltimateCharge(UltimateAmount, true);
		}
	}
}

void ANeonSwordCharacter::UpdateHandParticleSpawnRate(float NewSpawnRate)
{
	ElectricityNiagara->SetFloatParameter("SpawnRate", NewSpawnRate);
}

void ANeonSwordCharacter::EndUltimate()
{
	bUltimateReadyToAttack = false;
	bIsLeftHandInSword = false;
	bIsUltimateActive = false;
	SetInvencibility(false);
	GetSword()->UpdateElectricSpawnRate(0.0f, 0.0f);
	GetSword()->ToggleElectricParticles(false);
	GetSword()->ToggleUltimateIdleSound(false);
}

void ANeonSwordCharacter::GuardBroken()
{
	bChargeAtMax = true;
	bIsDefending = false;
	CameraShake(1.0f);
	UGameplayStatics::PlaySoundAtLocation(this, BrokenGuardSound, GetActorLocation());
	Stunned(0.8f);
}

void ANeonSwordCharacter::CameraShake(float Scale)
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	PlayerController->PlayerCameraManager->StartCameraShake(CameraShakeClass, Scale);
}

void ANeonSwordCharacter::ToggleHUD(bool bShow)
{
	if (!PlayerHUDClass)
		return;
	if(bShow){

		PlayerHUD->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		PlayerHUD->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ANeonSwordCharacter::Stunned(float Duration)
{
	bIsStunned = true;
	SetTargetSpeed(CurrentMovementState, GetCharacterMovement()->MaxWalkSpeed * MovementSpeedPenaltyMultiplier, RegularTimeSpeedChange, true);
	GetWorld()->GetTimerManager().SetTimer(StunTimerHandler, this, &ANeonSwordCharacter::EndStun, Duration, false);
}

void ANeonSwordCharacter::EndStun()
{
	bIsStunned = false;
	float TargetMaxSpeed = 0.0f;
	switch(CurrentMovementState)
	{
	case EMovementState::Walking:
		TargetMaxSpeed = WalkingSpeed;
		break;
	case EMovementState::Sprinting:
		TargetMaxSpeed = SprintingSpeed;
		break;
	case EMovementState::Crouching:
		TargetMaxSpeed = CrouchingSpeed;
		break;
	case EMovementState::Sliding:
		TargetMaxSpeed = SlideSpeed;
		break;
	case EMovementState::Still:
		TargetMaxSpeed = WalkingSpeed;
		break;
	default:
		TargetMaxSpeed = WalkingSpeed;
		break;
	}

	SetTargetSpeed(CurrentMovementState, TargetMaxSpeed, RegularTimeSpeedChange, true);
	GetWorld()->GetTimerManager().ClearTimer(StunTimerHandler);
}

void ANeonSwordCharacter::StartDashAttack()
{
	if (!bDashAttackUnlocked)
		return;

	if (bIsCrouching) {
		if (CanUncrouch(false)) {

			CrouchToggler(FInputActionValue());
		}
		else {
			UGameplayStatics::PlaySoundAtLocation(this, NotEnoughEnergyCue, GetActorLocation());
			return;
		}
	}

	if (bIsUltimateActive)
		return;
	if (GetChargeAmount() < DashAttackEnergyCost) {
		UGameplayStatics::PlaySoundAtLocation(this, NotEnoughEnergyCue, GetActorLocation());
		return;
	}
	SetUtilityState(EUtilityState::DashAttack);
	AudioComponentDash->Play();

	SetInvencibility(true);
	bIsHoldingDashAttack = true;
	bIsDashing = true;
}

void ANeonSwordCharacter::ReleaseDashAttack()
{
	if (!bIsHoldingDashAttack)
		return;
	if (!bReadyToRelease) {
		bBufferRelease = true;
		return;
	}

	bBufferRelease = false;
	bReadyToRelease = false;
	bIsHoldingDashAttack = false;
	bIsFlyingToPlace = true;

	DashEnemyTargetLocation = GetObjective(500.0f);
	DashEnemyTargetLocation.Z -= 60.0f;
	FVector  Direction = (DashEnemyTargetLocation - GetActorLocation()).GetSafeNormal();
	DashTargetLocation = DashEnemyTargetLocation - Direction * 100.0f;

	DashTargetLocation = GetGroundPoint(DashTargetLocation);
	DashTargetLocation.Z += 90.0f;

	//DrawDebugSphere(GetWorld(), DashTargetLocation, 10.0f, 12, FColor::Red, false, 5.0f);
	
	LaunchCharacter(Direction * DashAttackDashForce, true, true);

	AudioComponentDash->Stop();
	UGameplayStatics::PlaySoundAtLocation(this, SwordDashLaunchSoundCue, GetActorLocation());

	float TimeDashing = 0.1f;

	if (!DashTargetEnemy || FVector::Dist(GetActorLocation(), DashTargetLocation) > DashAttackMaximumDistance) {
		bDashToEnemy = false;
	}
	else if (CheckIfNPCIsSight(DashTargetEnemy)) {
		
		SetPhantomMode(true);
		bDashToEnemy = true;
		DashTargetEnemy->PrepareForImpaling(DashEnemyTargetLocation);

		FVector RotationDirection = (DashTargetEnemy->GetActorLocation() - DashTargetLocation).GetSafeNormal();
		FRotator NewTargetRotation = RotationDirection.Rotation();
		StartCameraLerpTo(NewTargetRotation, 0.3f);
	}
	else {
		bDashToEnemy = false;
	}
	
	GetWorldTimerManager().SetTimer(DashStopTimerHandle, this, &ANeonSwordCharacter::StopDash, 0.13f, false);
}

FVector ANeonSwordCharacter::GetGroundPoint(FVector Origin)
{
	FVector End = Origin - FVector(0, 0, 10000.0f);
	FHitResult HitResult;
	FCollisionQueryParams TraceParams(FName(TEXT("GroundTrace")), true, this);
	TraceParams.bTraceComplex = true;
	TraceParams.bReturnPhysicalMaterial = false;

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Origin, End, ECC_Visibility, TraceParams))
	{
		return HitResult.Location;
	}
	return FVector();
}

bool ANeonSwordCharacter::CheckEnemyDashed(ABasicEnemy* Enemy)
{

	return DashTargetEnemy == Enemy ? true : false;
}

void ANeonSwordCharacter::StopDash()
{
	SetPhantomMode(false);
	bIsFlyingToPlace = false;
	GetWorld()->GetTimerManager().ClearTimer(DashStopTimerHandle);

	if (!bDashToEnemy || !bDashReachedEnemy) {
		EndDashAttack();
	}

	GetCharacterMovement()->StopMovementImmediately();
}

void ANeonSwordCharacter::StartDashImpaling()
{
	SetPlayerControls(false);

	DashTargetEnemy->BeingImpaled(DashEnemyTargetLocation);
	UGameplayStatics::PlaySoundAtLocation(this, SwordImpaleSoundCue, GetActorLocation());

	bIsImpalingEnemy = true;
}

void ANeonSwordCharacter::StopDashImpaling()
{
	SetPlayerControls(true);

	EndDashAttack();
}

void ANeonSwordCharacter::CheckDashReach()
{
	//if (FVector::Dist(GetActorLocation(), DashTargetLocation) < 30.0f)
	if (FVector::Dist2D(GetActorLocation(), DashTargetLocation) < 30.0f)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Llegué")));
		
		bDashReachedEnemy = true;
		SetActorLocation(DashTargetLocation);
		StopDash();
		StartDashImpaling();
	}
}

void ANeonSwordCharacter::EndDashAttack()
{
	AddChargeAmount(-DashAttackEnergyCost, false);

	DashTargetEnemy = nullptr;
	bDashToEnemy = false;
	SetInvencibility(false);
	bIsImpalingEnemy = false;
	bIsDashing = false;
	bDashReachedEnemy = false;
	SetUtilityState(EUtilityState::Idle);
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Adios")));
}

void ANeonSwordCharacter::SetPlayerControls(bool bState)
{
	if(bState)
		EnableInput(Cast<APlayerController>(GetController()));
	else
		DisableInput(Cast<APlayerController>(GetController()));

	//GetController()->SetIgnoreMoveInput(!bState);
	//GetController()->SetIgnoreLookInput(!bState);
}

FVector ANeonSwordCharacter::GetObjective(float SphereRadius)
{
	FHitResult HitResult;

	FVector StartLocation = GetFirstPersonCameraComponent()->GetComponentLocation();

	FVector ForwardVector = GetFirstPersonCameraComponent()->GetForwardVector();
	FVector Offset = ForwardVector * 100000.0f;
	FVector EndLocation = GetFirstPersonCameraComponent()->GetComponentLocation() + Offset;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	float SweepDistance = 10000.0f;
	float InitialRadius = 2.5f;
	float MaxRadius = 250.0f;
	int NumSteps = 20;

	float StepDistance = SweepDistance / NumSteps;

	ECollisionChannel OnlyPawn = ECC_GameTraceChannel2;

	for (int i = 0; i < NumSteps; i++)
	{
		FVector CurrentLocation = StartLocation + ForwardVector * StepDistance * i;

		float SweepRadius = FMath::Lerp(InitialRadius, MaxRadius, i / (NumSteps - 1.0f));

		FCollisionShape CollisionShape = FCollisionShape::MakeSphere(SweepRadius);

		bool bHit = GetWorld()->SweepSingleByChannel(
			HitResult,
			CurrentLocation,
			CurrentLocation + ForwardVector * StepDistance,
			FQuat::Identity,
			OnlyPawn,
			CollisionShape,
			QueryParams
		);
		//DrawDebugSphere(GetWorld(), CurrentLocation, SweepRadius, 12, FColor::Red, false, 2.0f);
		if (bHit)
		{
			if (ABasicEnemy* Enemy = Cast<ABasicEnemy>(HitResult.GetActor()))
			{
				//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Name: %s"), *HitResult.GetActor()->GetName()));
				FVector EnemyFinalLocation = Enemy->GetActorLocation();
				EnemyFinalLocation.Z += 60.0f;
				DashTargetEnemy = Enemy;
				return EnemyFinalLocation;
			}
		}
	}
	DashTargetEnemy = nullptr;
	return EndLocation;
}

void ANeonSwordCharacter::SetPhantomMode(bool NewState)
{
	if (NewState) {
		//WallDetectionCapsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	else {
		GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
		//WallDetectionCapsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	}

}

bool ANeonSwordCharacter::CheckIfNPCIsSight(AActor* NPC)
{
	if (!NPC) return false;

	UCapsuleComponent* Capsule = NPC->FindComponentByClass<UCapsuleComponent>();
	if (!Capsule) return false;

	FVector NPC_Location = Capsule->GetComponentLocation();
	float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	float CapsuleRadius = Capsule->GetScaledCapsuleRadius();

	FVector PlayerViewPoint;
	FRotator PlayerViewRotation;
	GetWorld()->GetFirstPlayerController()->GetPlayerViewPoint(PlayerViewPoint, PlayerViewRotation);

	TArray<FVector> TargetPoints;
	TargetPoints.Add(NPC_Location + FVector(0, 0, CapsuleHalfHeight)); // Top
	TargetPoints.Add(NPC_Location); // Middle
	TargetPoints.Add(NPC_Location - FVector(0, 0, CapsuleHalfHeight)); // Bottom

	const int NumSidePoints = 6;
	for (int i = 0; i < NumSidePoints; i++)
	{
		float AngleRad = FMath::DegreesToRadians((360.f / NumSidePoints) * i);
		FVector Offset = FVector(FMath::Cos(AngleRad) * CapsuleRadius, FMath::Sin(AngleRad) * CapsuleRadius, 0);
		TargetPoints.Add(NPC_Location + Offset);
	}

	for (const FVector& TargetPoint : TargetPoints)
	{
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.AddIgnoredActor(NPC);

		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, PlayerViewPoint, TargetPoint, ECC_Visibility, QueryParams);

		FColor TraceColor = bHit ? FColor::Red : FColor::Green;
		//DrawDebugLine(GetWorld(), PlayerViewPoint, TargetPoint, TraceColor, false, 10.0f, 0, 2.0f);

		if (!bHit)
		{
			return true;
		}
	}
	return false;
}

void ANeonSwordCharacter::KineticEnergyUpdate(float DeltaTime)
{
	float KineticEnergyThisFrame = (GetVelocity().Size() / SprintingSpeed * KineticEnergyFactor - KineticEnergyExpireRate) * DeltaTime;

	KineticEnergy = FMath::Clamp(KineticEnergy + KineticEnergyThisFrame, 0.0f, 100.0f);
	
	//if (bIsUltimateActive || bIsDashing)
	if (GetUtilityState() == EUtilityState::Ultimate || GetUtilityState() == EUtilityState::DashAttack)
		KineticEnergy = 100.0f;
	
	if(KineticEnergy > 5.0f)
		GetSword()->ToggleSwordCharge(true, KineticEnergy);
	else {
		GetSword()->ToggleSwordCharge(false, 0.0f);
	}

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("V: %f"), KineticEnergyThisFrame));
}

void ANeonSwordCharacter::AddChargeAmount(float ChargeToAdd, bool bCanBreakGuard)
{
	if (!bShieldUnlocked)
		return;

	ChargeAmount += ChargeToAdd;
	ChargeAmount = FMath::Clamp(ChargeAmount, 0.0f, 100.0f);
	
	if (ChargeAmount >= 100.0f && bCanBreakGuard)
	{
		bChargeAtMax = true;
		bIsUncharged = false;
		GuardBroken();
	}
	else if (ChargeAmount <= 0.0f)
	{
		EndShield();
		bChargeAtMax = false;
		bIsUncharged = true;
	}
	else
	{
		bIsUncharged = false;
		bChargeAtMax = false;
	}

	GetSword()->ChangeLightsColor();
	UpdateArmsEnergy();

	if (PlayerHUD)
	{
		UPlayerHUD* PlayerWidget = Cast<UPlayerHUD>(PlayerHUD);

		if (PlayerWidget)
		{
			PlayerWidget->UpdateCharge(ChargeAmount, GetCurrentChargeColor());
		}
	}
}

void ANeonSwordCharacter::AddUltChargeAmount(float ChargeToAdd)
{
	if (!bUltimateUnlocked)
		return;

	if (bIsUltimateActive)
		return;

	UltimateAmount += ChargeToAdd * UltimateGenerationMultiplier;
	UltimateAmount = FMath::Clamp(UltimateAmount, 0.0f, 100.0f);

	if (PlayerHUD)
	{
		UPlayerHUD* PlayerWidget = Cast<UPlayerHUD>(PlayerHUD);

		if (PlayerWidget)
		{
			PlayerWidget->UpdateUltimateCharge(UltimateAmount, false);
		}
		if (UltimateAmount >= 100.0f)
		{
			PlayerWidget->ToggleOverdrive(true);
		}
	}

	if (GetUltimateChargeAmount() >= 100.0f) {
		UpdateHandParticleSpawnRate(2000.0f);
	}
	else if (GetUltimateChargeAmount() >= 90.0f) {
		UpdateHandParticleSpawnRate(200.0f);
	}
}

void ANeonSwordCharacter::SetInvencibility(bool NewState)
{
	bIsInvincible = NewState;
	GetWorld()->GetTimerManager().ClearTimer(InvincibilityTimerHandle);
}

void ANeonSwordCharacter::SetInvencibilityTime(float Time)
{
	bIsInvincible = true;
	GetWorld()->GetTimerManager().ClearTimer(InvincibilityTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(InvincibilityTimerHandle, this, &ANeonSwordCharacter::ResetInvincibility, Time, false);
}

FLinearColor ANeonSwordCharacter::GetCurrentChargeColor()
{
	float ChargeAmountRemapped = FMath::Clamp(GetChargeAmount() / 100.0f, 0.0f, 1.0f);;
	const FLinearColor CurrentChargeColor = ColorCurveCharges->GetLinearColorValue(FMath::Abs(1 -ChargeAmountRemapped));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Retrieved Color: %f"), ChargeAmountRemapped));
	return CurrentChargeColor;
}

void ANeonSwordCharacter::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		ABasicEnemy* Enemy = Cast<ABasicEnemy>(OtherActor);
		if (Enemy && Enemy->GetIsAlive())
		{
			if (!bIsDashing) {

				GetSword()->MakeDamage(Enemy);
				if (GetKineticEnergyAmount() <= 2.5f) {
					SwordAttackBounceOff();

					if(bIsTutorialActivated)
						DisplayBounceMessage();
				}
			}
			//else {
			//	if (CheckEnemyDashed(Enemy)) {
			//		//StartDashImpaling();
			//	}
			//}
		}
	}
}

void ANeonSwordCharacter::ActivateSwordSphere()
{
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void ANeonSwordCharacter::DeactivateSwordSphere()
{
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
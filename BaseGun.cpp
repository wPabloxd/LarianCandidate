// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseGun.h"
#include "Accessories/Laser.h"
#include "NeonSword/IA/BasicEnemy.h"
#include "NiagaraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "RegularBullet.h"
#include <NeonSword/NeonSwordCharacter.h>
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "NiagaraFunctionLibrary.h"
#include <Kismet/GameplayStatics.h>
#include <Perception/AISense_Hearing.h>

ABaseGun::ABaseGun()
{
	PrimaryActorTick.bCanEverTick = true;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComp->SetVisibility(false);

	LaserActor	= CreateDefaultSubobject<UChildActorComponent>(TEXT("Laser"));
	//LasterActor->SetVisibility(bHasLaser);

	FlashLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashLight"));
	FlashLight->SetVisibility(false);

	GunSkeletonComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GunMeshSkeleton"));

	RootComponent = GunSkeletonComponent;

	StaticMeshComp->SetupAttachment(GunSkeletonComponent);
	FlashLight->SetupAttachment(GunSkeletonComponent);
	LaserActor->SetupAttachment(GunSkeletonComponent);


}

void ABaseGun::BeginPlay()
{
	Super::BeginPlay();

	StaticMeshComp->SetVisibility(false);
	FlashLight->SetVisibility(false);

	LaserActor->GetChildActor()->SetOwner(this);
	ToggleLaser(bHasLaser);

	FireRate /= 60;
	OriginalFireRate = FireRate;
	CurrentBurstCount = ShotsPerBurst;
	CurrentMagBullets = MagCapacity;
}

void ABaseGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CurrentDeltaTime = DeltaTime;

	if (bIsFiring)
		FiringModeHandler();
}

void ABaseGun::FiringModeHandler()
{
	if (CurrentMagBullets == 0) {

		StartReload();
		return;
	}

	switch (FireMode)
	{
	case EFireMode::FullAuto:
		FullAutoShooting();
		break;

	case EFireMode::SemiAuto:
		SemiAutoShooting();
		break;

	case EFireMode::Burst:
		BurstShooting();
		break;

	case EFireMode::EnergyCharge:
		EnergyChargeShooting();
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("Unknown fire mode"));
		break;
	}
}

void ABaseGun::FullAutoShooting()
{
	if (!bShotCoolDown && !bIsReloading) {
		ShotFired();
	}
}

void ABaseGun::SemiAutoShooting()
{
	if (!bShotCoolDown && !bIsReloading) {
		FireRate = OriginalFireRate;
		FireRate *= 1.0f + FMath::FRandRange(-RandomVarianceBetweenShots, RandomVarianceBetweenShots);
		ShotFired();
	}
}

void ABaseGun::BurstShooting()
{
	if (!bShotCoolDown && !bIsBetweenBursts && !bIsReloading) {

		if (CurrentBurstCount > 0) {
			ShotFired();
			CurrentBurstCount--;
		}
		else if (CurrentBurstCount == 0) {
			bIsBetweenBursts = true;
			GetWorld()->GetTimerManager().SetTimer(BurstTimerHandle, this, &ABaseGun::ResetBurstCoolDown, TimeBetweenBursts, false);
		}
	}
}

void ABaseGun::EnergyChargeShooting()
{
	CurrentEnergyChargeTime += CurrentDeltaTime;
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("ENER: %f"), CurrentEnergyChargeTime));
	if (!bShotCoolDown && !bIsReloading && CurrentEnergyChargeTime >= EnergyChargeTime - 0.01f) {
		
		ABasicEnemy* EnemyOwner = Cast<ABasicEnemy>(GetOwner());
		EnemyOwner->SetIsMissing(false);
		
		ALaser* Laser = Cast<ALaser>(LaserActor->GetChildActor());
		Laser->ChangeLightIntensity(10000.0f);
		Laser->SetLockedOnPlayer(false);
		ShotFired();
		CurrentEnergyChargeTime = 0.0f;
	}
	else if (CurrentEnergyChargeTime <= EnergyChargeTime)
	{
		ALaser* Laser = Cast<ALaser>(LaserActor->GetChildActor());
		Laser->SetLockedOnPlayer(true);
		//float Intensity = FMath::Lerp(10000.0f, 1000000.0f, CurrentEnergyChargeTime / EnergyChargeTime);
		float Intensity = 10000.0f * FMath::Pow(1000000.0f / 10000.0f, CurrentEnergyChargeTime / EnergyChargeTime);
		Laser->ChangeLightIntensity(Intensity);
	}
}

void ABaseGun::ShotFired()
{
	for (int i = 0; i < NumberOfProjectiles; i++)
	{
		BulletDirection = RandomShotDirection(bIsMissing ? MissingDispersion : HittingDispersion);
		BulletTrajectory = BulletDirection.ToOrientationRotator();
		if (!bIsMissing)
			PhysicalShot();

		PlayTracerParticle();
	}

	PlayShotMontage();

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("BALAS: %d"), CurrentMagBullets));
	
	GetWorld()->GetTimerManager().SetTimer(CadenceTimerHandle, this, &ABaseGun::ResetShotCoolDown, 1/FireRate, false);
	bShotCoolDown = true;
	CurrentMagBullets--;
}

void ABaseGun::PhysicalShot()
{
	MuzzleTipLocation = GunSkeletonComponent->GetBoneLocation("MuzzleTip", EBoneSpaces::WorldSpace);
	FTransform SpawnTransform;
	SpawnTransform.SetLocation(MuzzleTipLocation);
	SpawnTransform.SetRotation(BulletTrajectory.Quaternion());

	ARegularBullet* Bullet = GetWorld()->SpawnActorDeferred<ARegularBullet>(
		Bullets,
		SpawnTransform,
		this,
		nullptr, 
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);

	if (Bullet)
	{
		Bullet->Damage = Damage;
		Bullet->Speed = BulletSpeed;
		Bullet->BulletDirection = BulletDirection;
		Bullet->OwningActor = this;
		Bullet->bIsEnemy = true;
		UGameplayStatics::FinishSpawningActor(Bullet, SpawnTransform);
	}
}

void ABaseGun::ResetShotCoolDown()
{
	GetWorld()->GetTimerManager().ClearTimer(CadenceTimerHandle);
	bShotCoolDown = false;

	if(FireMode == EFireMode::Burst && CurrentBurstCount > 0)
		BurstShooting();
}

void ABaseGun::ResetBurstCoolDown()
{
	GetWorld()->GetTimerManager().ClearTimer(BurstTimerHandle);
	bIsBetweenBursts = false;
	CurrentBurstCount = ShotsPerBurst;
}

void ABaseGun::StartReload()
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("RECARGANDO")));
	GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseGun::Reloaded, ReloadTime, false);
	bIsReloading = true;

	ABasicEnemy* EnemyOwner = Cast<ABasicEnemy>(GetOwner());
	if (EnemyOwner)
	{
		EnemyOwner->SetReloading(this->bIsReloading);
	}
}

void ABaseGun::Reloaded()
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("RECARGADO")));
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimerHandle);
	CurrentMagBullets = MagCapacity;
	bIsReloading = false;

	ABasicEnemy* EnemyOwner = Cast<ABasicEnemy>(GetOwner());
	if (EnemyOwner)
	{
		EnemyOwner->SetReloading(this->bIsReloading);
	}
}

FVector ABaseGun::RandomShotDirection(float Dispersion)
{
	const ACharacter* Player = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	FVector PlayerLocation = Player->GetActorLocation();
	FVector GunLocation = GetActorLocation();

	FVector DirectionToPlayer = (PlayerLocation - GunLocation).GetSafeNormal();

	FRotator RandomRotator = FRotator(FMath::RandRange(-Dispersion, Dispersion), FMath::RandRange(-Dispersion, Dispersion), 0.0f);

	FVector RandomShotDirection = -RandomRotator.RotateVector(DirectionToPlayer);

	return RandomShotDirection;
}

void ABaseGun::PlayShotMontage()
{
	FlashLight->SetVisibility(true);
	GetWorld()->GetTimerManager().SetTimer(FlashLightTimerHandle, this, &ABaseGun::TurnOffFlash, 0.05f, false);
	UAnimInstance* AnimInstance = GunSkeletonComponent->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(ShotMontage, 1.0f);
	}

	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.0f, GetOwner(), 0.0f, FName("Noise"));
}

void ABaseGun::PlayTracerParticle()
{
	FVector TracerDirection = -BulletDirection;
	MuzzleTipLocation = GunSkeletonComponent->GetBoneLocation("MuzzleTip", EBoneSpaces::WorldSpace);
	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		TracerParticle,
		MuzzleTipLocation,
		TracerDirection.ToOrientationRotator(),
		FVector(1.0f, 1.0f, 1.0f),
		true,
		true);

	NiagaraComponent->SetVectorParameter("BulletVelocity", FVector(BulletSpeed, 0.0f, 0.0f));
	NiagaraComponent->SetFloatParameter("BulletSize", BulletSize);
}

void ABaseGun::TurnOffFlash()
{
	FlashLight->SetVisibility(false);
}

bool ABaseGun::CanEditRandomVariance() const
{
	return FireMode == EFireMode::SemiAuto;
}

void ABaseGun::ToggleLaser(bool bIsOn)
{
	LaserActor->GetChildActor()->SetActorTickEnabled(bIsOn);
	LaserActor->GetChildActor()->SetActorHiddenInGame(!bIsOn);
}

void ABaseGun::RagdollGun()
{
	ToggleLaser(false);
	GunSkeletonComponent->SetVisibility(false);
	StaticMeshComp->SetVisibility(true);
	StaticMeshComp->SetCollisionProfileName(TEXT("BlockAll"));
	StaticMeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StaticMeshComp->SetSimulatePhysics(true);
}
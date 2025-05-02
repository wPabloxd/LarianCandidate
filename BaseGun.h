// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimMontage.h"
#include "NiagaraComponent.h"
#include "Components/SpotLightComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BaseGun.generated.h"

UENUM(BlueprintType)
enum class EFireMode : uint8
{
	FullAuto    UMETA(DisplayName = "FullAuto"),
	SemiAuto    UMETA(DisplayName = "SemiAuto"),
	Burst       UMETA(DisplayName = "Burst"),
	EnergyCharge  UMETA(DisplayName = "EnergyCharge"),
};
class ARegularBullet;
class ABulletActor;

UCLASS()
class NEONSWORD_API ABaseGun : public AActor
{
	GENERATED_BODY()
	
public:

	ABaseGun();

	void StartFiring()
	{
		bIsFiring = true;
	}

	void StopFiring()
	{
		bIsFiring = false;
	}

	void SetMissing(bool bNewState) 
	{
		bIsMissing = bNewState;
	}
	bool GetIsReloading() 
	{
		return bIsReloading;
	}

	void StartReload();

	USkeletalMeshComponent* GetSkeletalMeshComponent()
	{
		return GunSkeletonComponent;
	}

	float GetBulletDamage() 
	{
		return Damage;
	}

	float GetBulletSize() 
	{
		return BulletSize;
	}

	bool GetIsDefectable()
	{
		return bIsDeflectable;
	}

	void RagdollGun();

protected:

	virtual void BeginPlay() override;


public:

	virtual void Tick(float DeltaTime) override;

private:

	void FiringModeHandler();

	void FullAutoShooting();
	void SemiAutoShooting();
	void BurstShooting();
	void EnergyChargeShooting();

	void Reloaded();

	bool bIsMissing = true;
	bool bIsFiring = false;
	bool bShotCoolDown = false;
	bool bIsReloading = false;

	bool bIsBetweenBursts = false;
	int CurrentBurstCount;

	int CurrentMagBullets = 30;

	float CurrentEnergyChargeTime = 0.0f;

	void ShotFired();

	void PhysicalShot();

	FTimerHandle ReloadTimerHandle;
	FTimerHandle CadenceTimerHandle;
	void ResetShotCoolDown();

	FTimerHandle BurstTimerHandle;
	void ResetBurstCoolDown();

	FVector RandomShotDirection(float Dispersion);

	void PlayShotMontage();

	void PlayTracerParticle();

	UNiagaraComponent* NiagaraComp;

	FVector MuzzleTipLocation;

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FVector BulletDirection;

	void TurnOffFlash();
	FTimerHandle FlashLightTimerHandle;

	void ToggleLaser(bool bIsOn);

	float CurrentDeltaTime = 0.0f;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Parts")
	USkeletalMeshComponent* GunSkeletonComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Parts")
	UStaticMeshComponent* StaticMeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Parts")
	UChildActorComponent* LaserActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Parts")
	USpotLightComponent* FlashLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Properties")
	int MagCapacity = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Properties")
	float FireRate = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Properties", meta = (EditCondition = "FireMode == EFireMode::EnergyCharge"))
	float EnergyChargeTime = 3.0f;

	float OriginalFireRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile|References")
	TSubclassOf<ARegularBullet> Bullets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Properties")
	float ReloadTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Properties")
	bool bHasLaser = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile")
	int NumberOfProjectiles = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile")
	float HittingDispersion = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile")
	float MissingDispersion = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile")
	float Damage = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile")
	float BulletSpeed = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Firing Mode")
	EFireMode FireMode;

	UFUNCTION(BlueprintCallable, Category = "Gun|Firing Mode")
	bool CanEditRandomVariance() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Firing Mode", meta = (EditCondition = "CanEditRandomVariance"))	
	float RandomVarianceBetweenShots = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Firing Mode", meta = (EditCondition = "FireMode == EFireMode::Burst"))
	int ShotsPerBurst = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Firing Mode", meta = (EditCondition = "FireMode == EFireMode::Burst"))
	float TimeBetweenBursts = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	UAnimMontage* ShotMontage;

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FRotator BulletTrajectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle", meta = (AllowPrivateAccess = "true"))
	UNiagaraSystem* TracerParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile", meta = (AllowPrivateAccess = "true"))
	float BulletSize = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile|References", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ABulletActor> BulletModel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun|Projectile", meta = (AllowPrivateAccess = "true"))
	bool bIsDeflectable = true;

public:

	TSubclassOf<ABulletActor> GetBulletModel() const {
		return BulletModel;
	}
};
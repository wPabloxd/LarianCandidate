// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "GameFramework/Actor.h"
#include "PlayerShield.generated.h"

class ARegularBullet;
class ABulletActor;

UCLASS()
class NEONSWORD_API APlayerShield : public AActor
{
	GENERATED_BODY()

public:
	APlayerShield();

	void TurnOn();
	void TurnOff();

	void AddBullet(TSubclassOf<ABulletActor> BulletActor, float Damage, float Size);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	UStaticMeshComponent* RootMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	UStaticMeshComponent* ForceFieldMesh;

	TArray<UChildActorComponent*> BulletsArray;
	TArray<float> BulletsDamage;
	TArray<float> BulletsSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
	UNiagaraSystem* TracerParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	TSubclassOf<ARegularBullet> Bullets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bIsShieldLoaded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bShieldHitted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float ChargeCostPerBulletDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float ProjectileDamageMultiplier = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* FlyingBulletSoundCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* ForceFieldIdleSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* BulletDetained;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* ShieldRelease;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	UCurveLinearColor* ForceFieldColorCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	UAudioComponent* AudioComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emissive")
	float MinEmissive = 200.0f;

private:

	bool bIsTurningOn = false;
	bool bIsTurningOff = false;

	void ChangingAlpha(float DeltaTime);

	float CurrentAlpha = 0.65f;
	float AlphaSpeed = 1.0f / 1.9f;

	bool bShieldOperative = false;

	void ToggleShield(bool bIsOperative);

	void ReleaseAttack();

	UMaterialInterface* MaterialInstance;
	UMaterialInstanceDynamic* DynamicMaterial;

	void EmissiveRecover(float DeltaTime);
	bool bIsChangingEmissive = false;
	float CurrentEmissiveForce = 100.0f;

	void ChangeForceFieldColor();

	void PlaySound();

	void StopSound();

	void ResetHit();

public:
	virtual void Tick(float DeltaTime) override;
};

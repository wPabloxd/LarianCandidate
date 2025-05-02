// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "GameFramework/Actor.h"
#include "RegularBullet.generated.h"

class ABaseGun;

UCLASS()
class NEONSWORD_API ARegularBullet : public AActor
{
	GENERATED_BODY()


public:
	ARegularBullet();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void PhysicalShot();

public:
	virtual void Tick(float DeltaTime) override;

	FVector BulletDirection = FVector::Zero();
	float Damage = 0;
	float Speed = 0;
	bool bIsEnemy = true;

	UPROPERTY(BlueprintReadOnly)
	AActor* OwningActor;

private:

	FTimerHandle SelfDestroy;

	float DistanceTraveledInThisFrame = 0;
	FVector CurrentBulletLocation = FVector::Zero();

};

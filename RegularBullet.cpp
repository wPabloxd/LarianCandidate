
#include "RegularBullet.h"
#include <NeonSword/NeonSwordCharacter.h>
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include <Kismet/GameplayStatics.h>
#include "BaseGun.h"
#include <NeonSword/IA/BasicEnemy.h>

ARegularBullet::ARegularBullet()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARegularBullet::BeginPlay()
{
	Super::BeginPlay();
	CurrentBulletLocation = GetActorLocation();
	GetWorld()->GetTimerManager().SetTimer(SelfDestroy, [this]()
	{
		if (IsValid(this))
		{
			Destroy();
		}
	}, 1.0f, false);
}

void ARegularBullet::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DistanceTraveledInThisFrame = Speed * DeltaTime;
	PhysicalShot();
}

void ARegularBullet::PhysicalShot()
{
	FVector EndLocation = CurrentBulletLocation + (-BulletDirection * DistanceTraveledInThisFrame);

	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		CurrentBulletLocation,
		EndLocation,
		ECC_Visibility,
		TraceParams
	);
	//DrawDebugLine(GetWorld(), CurrentBulletLocation, EndLocation, FColor::Red, false, 1, 0, 5);
	CurrentBulletLocation = EndLocation;


	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor && HitActor->IsA(ANeonSwordCharacter::StaticClass()) && bIsEnemy)
		{
			ANeonSwordCharacter* PlayerCharacter = Cast<ANeonSwordCharacter>(HitActor);
			UGameplayStatics::ApplyDamage(HitActor, Damage, OwningActor->GetOwner()->GetInstigatorController(), this, UDamageType::StaticClass());
			Destroy();
		}
		else if (HitActor && HitActor->IsA(ABasicEnemy::StaticClass()) && !bIsEnemy)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("DAMAGE")));
			ABasicEnemy* Enemy = Cast<ABasicEnemy>(HitActor);
			UGameplayStatics::ApplyDamage(HitActor, Damage, OwningActor->GetOwner()->GetOwner()->GetInstigatorController(), this, UDamageType::StaticClass());
			Destroy();
		}
	}
}

void ARegularBullet::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(SelfDestroy);
}
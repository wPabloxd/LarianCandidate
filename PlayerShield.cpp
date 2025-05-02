#include "PlayerShield.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NeonSword/Gun/RegularBullet.h"
#include "Sound/SoundBase.h"
#include "Curves/CurveLinearColor.h"
#include "Components/AudioComponent.h"
#include "Curves/RichCurve.h"
#include <Kismet/GameplayStatics.h>
#include "Camera/CameraComponent.h"
#include "NeonSword/Gun/BulletActor.h"
#include <NeonSword/NeonSwordCharacter.h>
#include <NeonSword/IA/BasicEnemy.h>

APlayerShield::APlayerShield()
{
	PrimaryActorTick.bCanEverTick = true;

	RootMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RootMesh"));
	RootComponent = RootMesh;

	ForceFieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ForceFieldMesh"));
	ForceFieldMesh->SetupAttachment(RootMesh);

	RootMesh->SetCollisionProfileName(TEXT("NoCollision"));
	ForceFieldMesh->SetCollisionProfileName(TEXT("NoCollision"));
}

void APlayerShield::BeginPlay()
{
	Super::BeginPlay();

	MaterialInstance = ForceFieldMesh->GetMaterial(0);
	DynamicMaterial = UMaterialInstanceDynamic::Create(MaterialInstance, nullptr);
	ForceFieldMesh->SetMaterial(0, DynamicMaterial);

	DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveMultiplier"), MinEmissive);
}

void APlayerShield::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsTurningOn || bIsTurningOff)
	{
		ChangingAlpha(DeltaTime);
	}
	if (bIsChangingEmissive) 
	{
		EmissiveRecover(DeltaTime);
	}

	ChangeForceFieldColor();
}

void APlayerShield::ChangeForceFieldColor()
{
	ANeonSwordCharacter* Character = Cast<ANeonSwordCharacter>(GetOwner()->GetOwner());

	//FLinearColor InitialColor = FLinearColor(0.0f, 0.5f, 1.0f);
	//FLinearColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Red, InitialColor, Character->GetChargeAmount() / 100.0f);
	
	DynamicMaterial->SetVectorParameterValue(TEXT("ForceFieldColor"), Character->GetCurrentChargeColor());

	//FRichCurve* RedCurve = &ForceFieldColorCurve->FloatCurves[0];
	//FRichCurve* BlueCurve = &ForceFieldColorCurve->FloatCurves[2];

	//FKeyHandle RedKeyHandle1 = RedCurve->FindKey(0.0f, 0.1f);
	//FKeyHandle RedKeyHandle2 = RedCurve->FindKey(1.0f, 0.1f);
	//FKeyHandle BlueKeyHandle1 = BlueCurve->FindKey(0.0f, 0.1f);
	//FKeyHandle BlueKeyHandle2 = BlueCurve->FindKey(1.0f, 0.1f);

	//ForceFieldColorCurve->AdjustHue = Character->GetChargeAmount() *2 ;

	//RedCurve->SetKeyValue(RedKeyHandle1, 1.0f - (Character->GetChargeAmount() / 100.0f));
	//RedCurve->SetKeyValue(RedKeyHandle2, 1.0f - (Character->GetChargeAmount() / 100.0f));
	//BlueCurve->SetKeyValue(BlueKeyHandle1, Character->GetChargeAmount() / 100.0f);
	//BlueCurve->SetKeyValue(BlueKeyHandle2, Character->GetChargeAmount() / 100.0f);


	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("ALPHA:")));
}

void APlayerShield::ChangingAlpha(float DeltaTime)
{
	if (CurrentAlpha > 0.3f && bIsTurningOn)
	{
		CurrentAlpha -= AlphaSpeed * DeltaTime;

		CurrentAlpha = FMath::Clamp(CurrentAlpha, 0.3f, 0.65f);
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("ALPHA: %f"), CurrentAlpha));

		if (DynamicMaterial)
		{
			DynamicMaterial->SetScalarParameterValue(TEXT("Alpha"), CurrentAlpha);
		}
		if (CurrentAlpha == 0.3f)
		{
			bIsTurningOn = false;
		}
		if (CurrentAlpha <= 0.45f) 
		{
			ToggleShield(true);
		}
	}
	else if (CurrentAlpha < 0.65f && bIsTurningOff)
	{
		CurrentAlpha += AlphaSpeed * DeltaTime * 4;

		CurrentAlpha = FMath::Clamp(CurrentAlpha, 0.3f, 0.65f);
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("ALPHA: %f"), CurrentAlpha));

		if (DynamicMaterial)
		{
			DynamicMaterial->SetScalarParameterValue(TEXT("Alpha"), CurrentAlpha);
		}
		if (CurrentAlpha == 0.65f)
		{
			bIsTurningOff = false;
			ToggleShield(false);
		}
	}
}

void APlayerShield::TurnOn()
{
	SetActorHiddenInGame(false);
	PrimaryActorTick.bCanEverTick = true;
	bIsTurningOn = true;
	bIsTurningOff = false;
	bIsShieldLoaded = false;
	ChangeForceFieldColor();
	
	PlaySound();
	//UGameplayStatics::PlaySoundAtLocation(this, ForceFieldIdleSound, GetActorLocation());
}

void APlayerShield::TurnOff()
{
	bIsTurningOff = true;
	bIsTurningOn = false;
	bShieldOperative = false;

	if (!BulletsArray.IsEmpty())
	{
		ReleaseAttack();
	}

	StopSound();
}

void APlayerShield::PlaySound()
{
	if (ForceFieldIdleSound)
	{
		if (!AudioComponent)
		{
			AudioComponent = NewObject<UAudioComponent>(this);
			AudioComponent->RegisterComponent(); 
			AudioComponent->bAutoDestroy = false;
		}

		AudioComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		AudioComponent->SetSound(ForceFieldIdleSound);
		AudioComponent->Play();
	}
}

void APlayerShield::StopSound()
{
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
	}
}

void APlayerShield::AddBullet(TSubclassOf<ABulletActor> BulletActor, float Damage, float Size)
{
	UChildActorComponent* NewChildActor = NewObject<UChildActorComponent>(this);
	if (NewChildActor)
	{
		NewChildActor->SetChildActorClass(BulletActor);
		NewChildActor->AttachToComponent(ForceFieldMesh, FAttachmentTransformRules::KeepRelativeTransform);
		
		float Radius = FMath::Sqrt(FMath::FRand()) * 35.0f;
		float RandomAngle = FMath::FRand() * 2 * PI;
		float OffsetX = FMath::Cos(RandomAngle) * Radius;
		float OffsetY = FMath::Sin(RandomAngle) * Radius;

		FVector RandomLocation(OffsetX, OffsetY, 0.0f);
		NewChildActor->AddRelativeLocation(RandomLocation);

		NewChildActor->SetRelativeScale3D(FVector(1.43f, 1.43f, 6.64f));
		NewChildActor->RegisterComponent();
		BulletsArray.Add(NewChildActor);
		BulletsDamage.Add(Damage);
		BulletsSize.Add(Size);
	}

	bIsShieldLoaded = true;
	bIsChangingEmissive = true;
	CurrentEmissiveForce = 5000.0f;

	ANeonSwordCharacter* Character = Cast<ANeonSwordCharacter>(GetOwner()->GetOwner());
	Character->AddChargeAmount(Damage * -ChargeCostPerBulletDamageMultiplier, false);

	ChangeForceFieldColor();
	UGameplayStatics::PlaySoundAtLocation(this, BulletDetained, GetActorLocation());

	bShieldHitted = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &APlayerShield::ResetHit);
}

void APlayerShield::ResetHit()
{
	bShieldHitted = false;
}

void APlayerShield::EmissiveRecover(float DeltaTime)
{
	CurrentEmissiveForce -= 50000.f * DeltaTime;
	CurrentEmissiveForce = FMath::Clamp(CurrentEmissiveForce, MinEmissive, 5000.0f);
	DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveMultiplier"), CurrentEmissiveForce);

	if (CurrentEmissiveForce == MinEmissive)
		bIsChangingEmissive = false;
}

void APlayerShield::ToggleShield(bool bIsOperative)
{
	if (!bIsOperative)
	{
		PrimaryActorTick.bCanEverTick = false;
		
		SetActorHiddenInGame(true);
	}
	else
	{
		bShieldOperative = true;
	}
}

void APlayerShield::ReleaseAttack()
{
	ANeonSwordCharacter* Character = Cast<ANeonSwordCharacter>(GetOwner()->GetOwner());

	Character->CameraShake(1.0f);
	Character->SetInvencibilityTime(0.3f);

	//FRotator Direction = Character->GetFirstPersonCameraComponent()->GetComponentRotation();
	FVector ObjectivePosition = Character->GetObjective(80.0f);
	//FVector Direction = (ObjectivePosition - Character->GetFirstPersonCameraComponent()->GetComponentLocation()).GetSafeNormal();
	int Index = 0;

	UGameplayStatics::PlaySoundAtLocation(this, ShieldRelease, GetActorLocation());

	for (UChildActorComponent* Bullet : BulletsArray)
	{
		FVector BulletDirection = (ObjectivePosition - Bullet->GetComponentLocation()).GetSafeNormal();

		UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			TracerParticle,
			Bullet->GetChildActor()->GetActorLocation(),
			BulletDirection.ToOrientationRotator(),
			FVector(1.0f, 1.0f, 1.0f),
			true,
			true);

		NiagaraComponent->SetVectorParameter("BulletVelocity", FVector(8000.0f, 0.0f, 0.0f));
		NiagaraComponent->SetFloatParameter("BulletSize", BulletsSize[Index]);

		FTransform SpawnTransform;
		//SpawnTransform.SetLocation(Character->GetFirstPersonCameraComponent()->GetComponentLocation());
		SpawnTransform.SetLocation(Bullet->GetChildActor()->GetActorLocation());
		SpawnTransform.SetRotation(BulletDirection.ToOrientationQuat());

		ARegularBullet* PhysicalBullet = GetWorld()->SpawnActorDeferred<ARegularBullet>(
			Bullets,
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);

		if (PhysicalBullet)
		{
			PhysicalBullet->Damage = BulletsDamage[Index] * ProjectileDamageMultiplier;
			PhysicalBullet->Speed = 8000.0f;
			PhysicalBullet->BulletDirection = -BulletDirection;
			PhysicalBullet->bIsEnemy = false;
			PhysicalBullet->OwningActor = this;
			UGameplayStatics::FinishSpawningActor(PhysicalBullet, SpawnTransform);
		}

		UGameplayStatics::PlaySoundAtLocation(this, FlyingBulletSoundCue, GetActorLocation());

		Bullet->DestroyChildActor();
		Index++;
	}
	BulletsArray.Empty();
	BulletsDamage.Empty();
	BulletsSize.Empty();
}
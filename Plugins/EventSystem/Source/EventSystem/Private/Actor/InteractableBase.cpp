// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/InteractableBase.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Component/InteractionHandlerComponent.h" 
#include "Types/InteractionType.h"
#include "UI/InteractionHUDWidget.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

// Sets default values
AInteractableBase::AInteractableBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 変数の初期化
	bIsPlayerNearby = false;
	bIsBeingLookedAt = false;
	bCanInteract = true;
	bIsOneShot = false;
	TargetSequence = nullptr;

	// コンポーネントの作成とアタッチ
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;

	//
	ProximitySensor = CreateDefaultSubobject<USphereComponent>(TEXT("ProximitySensor"));
	ProximitySensor->SetupAttachment(RootComponent);
	ProximitySensor->SetSphereRadius(600.f); // 適宜調整

	Widget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget"));
	Widget->SetupAttachment(RootComponent);
	Widget->SetWidgetSpace(EWidgetSpace::World); // 3D空間用
}

// Called when the game starts or when spawned
void AInteractableBase::BeginPlay()
{
	Super::BeginPlay();

	// Overlapイベントのバインド
	if (ProximitySensor)
	{
		ProximitySensor->OnComponentBeginOverlap.AddDynamic(this, &AInteractableBase::OnProximityOverlapBegin);
		ProximitySensor->OnComponentEndOverlap.AddDynamic(this, &AInteractableBase::OnProximityOverlapEnd);
	}

	// プレイヤーのInteractionHandlerComponentのイベントにバインド
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		UInteractionHandlerComponent* InteractionHandler = PlayerPawn->FindComponentByClass<UInteractionHandlerComponent>();
		if (InteractionHandler)
		{
			InteractionHandler->OnInteractionUpdate.AddDynamic(
				this,
				&AInteractableBase::ReceiveInteractionUpdate
			);
		}
	}

	// 自身のタイプを渡して、UI更新イベントを発火させる
	if(Widget)
	{
		CachedInteractionHUD = Cast<UInteractionHUDWidget>(Widget->GetUserWidgetObject());
		if (CachedInteractionHUD)
		{
			CachedInteractionHUD->UpdateTextPrompt(InteractType);
		}
	}

	// 初期状態チェックを次のフレームに遅延
	// レベルブループリントで生成されたアクターの初期化を待つため
	GetWorldTimerManager().SetTimerForNextTick(this, &AInteractableBase::CheckInitialProximityOverlaps);
}

void AInteractableBase::CheckInitialProximityOverlaps()
{
	if (!ProximitySensor)
	{
		return;
	}

	// 既にオーバーラップしているアクターを取得
	TArray<AActor*> OverlappingActors;
	ProximitySensor->GetOverlappingActors(OverlappingActors);

	// プレイヤーのポーンを取得
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return;
	}

	// プレイヤーが既にオーバーラップしているかチェック
	if (OverlappingActors.Contains(PlayerPawn))
	{
		bIsPlayerNearby = true;
	}
}

// Called every frame
void AInteractableBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ビルボード対応（画像3枚目）
	if (Widget)
	{
		APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
		if (CameraManager)
		{
			FVector WidgetLoc = Widget->GetComponentLocation();
			FVector CameraLoc = CameraManager->GetCameraLocation();

			// Find Look At Rotation
			FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(WidgetLoc, CameraLoc);
			Widget->SetWorldRotation(LookAtRot);

			// Scaleを 0.1, 0.1, 0.1 に固定
			Widget->SetWorldScale3D(FVector(0.1f, 0.1f, 0.1f));
		}
	}

	// 毎フレームUI更新
	RefreshUIState();
}

// 接近センサーの処理
void AInteractableBase::OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor && OtherActor == PlayerPawn)
	{
		bIsPlayerNearby = true;
	}
}

// 接近センサーから離れたときの処理
void AInteractableBase::OnProximityOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor && OtherActor == PlayerPawn)
	{
		bIsPlayerNearby = false;
	}
}

// 注目対象が変わったときの処理
void AInteractableBase::ReceiveInteractionUpdate(EInteractionUIState NewState, EInteractionType ActionType, AActor* TargetActor)
{
	// TargetActorが自分自身かチェック
	bIsBeingLookedAt = (TargetActor == this);
}

// UI状態の更新処理
void AInteractableBase::RefreshUIState()
{
	if (!Widget || !CachedInteractionHUD || !bCanInteract)
	{
		if (CachedInteractionHUD)
		{
			CachedInteractionHUD->SetUIState(EInteractionUIState::None);
		}
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		CachedInteractionHUD->SetUIState(EInteractionUIState::None);
		return;
	}

	// ProximitySensor への視線をチェック
	if (IsProximitySensorVisible(PC))
	{
		if (bIsBeingLookedAt)
		{
			CachedInteractionHUD->SetUIState(EInteractionUIState::Active);
		}
		else if (bIsPlayerNearby)
		{
			CachedInteractionHUD->SetUIState(EInteractionUIState::Focus);
		}
		else
		{
			CachedInteractionHUD->SetUIState(EInteractionUIState::None);
		}
	}
	else
	{
		CachedInteractionHUD->SetUIState(EInteractionUIState::None);
	}
}

bool AInteractableBase::IsProximitySensorVisible(APlayerController* PC)
{
	if (!PC || !PC->PlayerCameraManager || !ProximitySensor)
	{
		return false;
	}

	// カメラ位置から ProximitySensor への視線チェック
	FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	FVector End = ProximitySensor->GetComponentLocation();

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);  // 自分自身は無視
	QueryParams.AddIgnoredActor(PC->GetPawn());  // プレイヤーも無視

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	// 何もヒットしない = 視線が通っている
	return !bHit;
}

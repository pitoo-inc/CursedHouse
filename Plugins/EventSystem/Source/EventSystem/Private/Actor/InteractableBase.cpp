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

	// 初期状態で既にオーバーラップしているアクターをチェック
	CheckInitialProximityOverlaps();
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
	// Widgetコンポーネントが存在しない場合は早期リターン
	if (!Widget)
	{
		return;
	}

	// ウィジェットからUWBP_InteractionHUDを取得
	if (!CachedInteractionHUD)
	{
		return;
	}

	// インタラクション不可の場合はUI非表示
	if (!bCanInteract)
	{
		CachedInteractionHUD->SetUIState(EInteractionUIState::None);
		return;
	}

	// プレイヤーコントローラーを取得
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		CachedInteractionHUD->SetUIState(EInteractionUIState::None);
		return;
	}

	// プレイヤーの視線が通っているかチェック
	if (PC->LineOfSightTo(this, FVector::ZeroVector, false))
	{
		// 注視されている場合: アクティブ状態
		if (bIsBeingLookedAt)
		{
			CachedInteractionHUD->SetUIState(EInteractionUIState::Active);
		}
		// プレイヤーが近くにいる場合: フォーカス状態
		else if (bIsPlayerNearby)
		{
			CachedInteractionHUD->SetUIState(EInteractionUIState::Focus);
		}
		// どちらでもない場合: 非表示
		else
		{
			CachedInteractionHUD->SetUIState(EInteractionUIState::None);
		}
	}
	else
	{
		// 視線が通っていない場合: 非表示
		CachedInteractionHUD->SetUIState(EInteractionUIState::None);
	}
}

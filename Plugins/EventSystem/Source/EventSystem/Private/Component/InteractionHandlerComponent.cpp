// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/InteractionHandlerComponent.h"
#include "Types/InteractionType.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Interface/InteractableInterface.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"

// Sets default values for this component's properties
UInteractionHandlerComponent::UInteractionHandlerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UInteractionHandlerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UInteractionHandlerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ScanForInteractables();
}

// 注目しているオブジェクトを毎フレームスキャンして、UIの状態を更新する関数
void UInteractionHandlerComponent::ScanForInteractables()
{
	// まずはオーナーがPawnかどうかを確認。プレイヤー以外のアクターに付けることも想定しているため。
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	// カメラコンポーネントがあればそこから、なければPawnの位置と向きからトレース開始点と方向を決定
	FVector Start;
	FVector ForwardVector;

	// ★変更点0: カメラコンポーネントがあればそこからトレースするように！VRで頭の位置を基準にするため。
	UCameraComponent* CameraComp = OwnerPawn->FindComponentByClass<UCameraComponent>();
	if (CameraComp)
	{
		Start = CameraComp->GetComponentLocation();
		ForwardVector = CameraComp->GetForwardVector();
	}
	else
	{
		Start = OwnerPawn->GetActorLocation() + FVector(0, 0, 50.0f);
		ForwardVector = OwnerPawn->GetViewRotation().Vector();
	}

	// ★変更点1: トレースの長さを直接 ActiveDistance にする！
	FVector End = Start + (ForwardVector * ActiveDistance);

	FHitResult HitResult;
	TArray<AActor*> IgnoreActors = { GetOwner() };

	// SphereTraceSingle を使って、StartからEndまでの間にInteractableInterfaceを持つアクターがあるかをチェック
	bool bHit = UKismetSystemLibrary::SphereTraceSingle(
		GetWorld(), Start, End, TraceRadius,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, IgnoreActors, EDrawDebugTrace::ForOneFrame, HitResult, true
	);

	EInteractionUIState NewState = EInteractionUIState::None;
	EInteractionType ActionType = EInteractionType::None;
	AActor* HitActor = nullptr;

	// トレースが当たって、かつ当たったアクターがインタラクト可能なときだけUIをActiveにする
	if (bHit && HitResult.GetActor() && HitResult.GetActor()->Implements<UInteractableInterface>())
	{
		HitActor = HitResult.GetActor();
		ActionType = IInteractableInterface::Execute_GetInteractionType(HitActor);

		// ★変更点2: 当たった時点で距離圏内確定なので、無条件でActive！
		NewState = EInteractionUIState::Active;
	}
	// else は初期値で None と nullptr になっているので省略可能ですが、
	// 安全のために残しておいてもOKです。
	else {
		NewState = EInteractionUIState::None;
		ActionType = EInteractionType::None;
		HitActor = nullptr;
	}

	// UIの状態か注目対象が変わったときだけ、デリゲートを呼び出してUIを更新する
	if (NewState != CurrentUIState || HitActor != CurrentTarget.Get())
	{
		CurrentUIState = NewState;
		CurrentTarget = HitActor;
		OnInteractionUpdate.Broadcast(CurrentUIState, ActionType, HitActor);
	}
}

// プレイヤーが [E] を押したときの処理
void UInteractionHandlerComponent::PerformInteract()
{
	// 注目している対象があって、UI状態がActiveのときだけインタラクトを実行
	if (CurrentUIState == EInteractionUIState::Active && CurrentTarget.IsValid())
	{
		IInteractableInterface::Execute_OnInteract(CurrentTarget.Get(), Cast<APawn>(GetOwner()));
	}
}
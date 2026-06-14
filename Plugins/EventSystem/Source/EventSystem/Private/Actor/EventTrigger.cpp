// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/EventTrigger.h"
#include "Interface/ActivatableInterface.h"
#include "Interface/StatefulActivatableInterface.h"
#include "Component/ConditionSourceComponent.h"


// Sets default values
AEventTrigger::AEventTrigger()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	//default root componentを作成してアタッチ
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

}

// Called when the game starts or when spawned
void AEventTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerData)
	{
		// OneShotタイプのターゲットの状態を管理するマップを初期化
		for (const FEventTarget& EventTarget : TriggerData->EventTargets)
		{
			if (AActor* TargetActor = EventTarget.TargetActor.Get())
			{
				// OneShotタイプのみマップに追加
				if (EventTarget.TargetType == ETargetType::OneShot)
				{
					OneShotActivationStates.Add(TargetActor, false);
				}
			}
		}
		BindToConditionSourceDelegates();
	}

	//root componentがない場合は、シーンルートを作成してアタッチ
	if (!GetRootComponent())
	{
		USceneComponent* SceneRoot = NewObject<USceneComponent>(this, TEXT("DefaultSceneRoot"));
		SceneRoot->SetupAttachment(RootComponent);
		SetRootComponent(SceneRoot);
	}
}

// 条件ソースコンポーネントの状態変化イベントにバインドする関数
void AEventTrigger::BindToConditionSourceDelegates()
{
	// TriggerDataに設定されたすべての条件ソースに対して、状態変化イベントにバインド
	for (const FConditionSource& ConditionSource : TriggerData->ConditionSources)
	{
		// 条件ソースアクターを取得
		AActor* ConditionSourceActor = ConditionSource.SourceActor.Get();
		if (ConditionSourceActor)
		{
			// 条件ソースコンポーネントを取得
			UConditionSourceComponent* ConditionSourceComp = Cast<UConditionSourceComponent>(ConditionSourceActor->GetComponentByClass(UConditionSourceComponent::StaticClass()));
			if (ConditionSourceComp)
			{
			// 条件ソースの状態変化イベントにバインド
			ConditionSourceComp->OnConditionStateChanged.AddDynamic(this, &AEventTrigger::OnConditionStateChanged);
			ConditionSourceComp->SourceBehavior = ConditionSource.SourceBehavior;
			}
		}
	}
}

// 条件ソースの状態変化イベントに対応する関数
void AEventTrigger::OnConditionStateChanged(UConditionSourceComponent* ConditionSourceComponent, bool bConditionMet)
{
	// 現在のパズル条件を評価
	bool bCurrentConditionMet = EvaluateCondition(ConditionSourceComponent);

	// 【Stateful制御のための条件変化フラグ】
	bool bConditionJustMet = !bLastConditionMet && bCurrentConditionMet;
	bool bConditionJustLost = bLastConditionMet && !bCurrentConditionMet;

	// TriggerDataに設定されたすべてのターゲットアクターに対して、条件の変化に応じてイベントをトリガー
	for (const FEventTarget& EventTarget : TriggerData->EventTargets)
	{
		if (AActor* TargetActor = EventTarget.TargetActor.Get())
		{
			switch (EventTarget.TargetType)
			{
			case ETargetType::EveryTime:
				// EveryTime: 条件が満たされた瞬間のみ実行
				if (bConditionJustMet)
				{
					if (TargetActor->GetClass()->ImplementsInterface(UActivatableInterface::StaticClass()))
					{
						IActivatableInterface::Execute_OnActivate(TargetActor,true, ConditionSourceComponent->GetOwner());
					}
				}
				break;

			case ETargetType::OneShot:
				// OneShot: 条件が満たされた瞬間、かつまだ実行されていなければ実行
				if (bConditionJustMet && !OneShotActivationStates.FindRef(TargetActor))
				{
				if (TargetActor->GetClass()->ImplementsInterface(UActivatableInterface::StaticClass()))
				{
					IActivatableInterface::Execute_OnActivate(TargetActor, true, ConditionSourceComponent->GetOwner());
				}
				OneShotActivationStates.Add(TargetActor, true);
				}
				break;

			case ETargetType::Stateful:
				// Stateful: 条件が変化したときのみ、新しい状態を通知
				if (bConditionJustMet || bConditionJustLost)
				{
					// SetActivationStateインターフェースを呼び出し
					if (TargetActor->GetClass()->ImplementsInterface(UStatefulActivatableInterface::StaticClass()))
					{
						IStatefulActivatableInterface::Execute_SetActivationState(TargetActor, bCurrentConditionMet, ConditionSourceComponent->GetOwner());
					}
				}
				break;
			}
		}
	}

	// 次回の評価のために、現在の条件を保存
	bLastConditionMet = bCurrentConditionMet;
}

// パズルの条件を評価する関数
bool AEventTrigger::EvaluateCondition(UConditionSourceComponent* ChangedComponent)
{
	if (!TriggerData) return false;

	// 条件の論理タイプに応じて、条件ソースの状態を評価
	// AND: すべての条件ソースが満たされている必要がある
	if (TriggerData->ConditionLogic == EConditionLogic::AND)
	{
		for (const FConditionSource& ConditionSource : TriggerData->ConditionSources)
		{
			AActor* ConditionSourceActor = ConditionSource.SourceActor.Get();
			if (!ConditionSourceActor) continue;
			UConditionSourceComponent* ConditionSourceComp = Cast<UConditionSourceComponent>(ConditionSourceActor->GetComponentByClass(UConditionSourceComponent::StaticClass()));
			if (ConditionSourceComp && !ConditionSourceComp->bConditionMet)
			{
				return false;
			}
		}
		return true;
	}
	// OR条件：1つでも満たされていればOK
	else if (TriggerData->ConditionLogic == EConditionLogic::OR)
	{
		for (const FConditionSource& ConditionSource : TriggerData->ConditionSources)
		{
			AActor* ConditionSourceActor = ConditionSource.SourceActor.Get();
			if (!ConditionSourceActor) continue;
			UConditionSourceComponent* ConditionSourceComp = Cast<UConditionSourceComponent>(ConditionSourceActor->GetComponentByClass(UConditionSourceComponent::StaticClass()));
			if (ConditionSourceComp && ConditionSourceComp->bConditionMet)
			{
				return true;
			}
		}
		return false;
	}
	// COMMON条件：すべての条件ソースが同じ状態である必要がある（すべて満たされているか、すべて満たされていないか）
	// 例えば、3つのスイッチがあって、COMMON条件の場合、すべてのスイッチがONのときにイベントがトリガーされる。もし途中で1つだけOFFになったら、全体の条件は満たされなくなる。
	else if (TriggerData->ConditionLogic == EConditionLogic::COMMON)
	{
		for (const FConditionSource& ConditionSource : TriggerData->ConditionSources)
		{
			AActor* ConditionSourceActor = ConditionSource.SourceActor.Get();
			if (!ConditionSourceActor) continue;
			UConditionSourceComponent* ConditionSourceComp = Cast<UConditionSourceComponent>(ConditionSourceActor->GetComponentByClass(UConditionSourceComponent::StaticClass()));
			if (ConditionSourceComp)
			{
				// もし今回状態が変化したコンポーネントがあれば、その状態を他のすべてのConditionSourceCompにもセットする
				if (ChangedComponent && ConditionSourceComp != ChangedComponent)
				{
					ConditionSourceComp->bConditionMet = ChangedComponent->bConditionMet;
				}
			}
		}
		return ChangedComponent->bConditionMet;
	}


	// Sequence条件：指定された順番で条件ソースが満たされる必要がある
	else if (TriggerData->ConditionLogic == EConditionLogic::Sequence)
	{
		if (!ChangedComponent) return false;

		// まだ規定回数に達していない場合
		if (CurrentPressCount < TriggerData->ConditionSources.Num())
		{
			// 今回押されるべき「正解のアクター」を取得
			AActor* ExpectedActor = TriggerData->ConditionSources[CurrentPressCount].SourceActor.Get();

			// 押されたスイッチが正解のアクターと違う場合、こっそり失敗フラグを立てる
			if (ChangedComponent->GetOwner() != ExpectedActor)
			{
				bIsSequenceFailed = true;
			}

			// 押された回数を増やす
			CurrentPressCount++;

			// 規定回数（すべてのスイッチ）が押されたか？
			if (CurrentPressCount >= TriggerData->ConditionSources.Num())
			{
				if (bIsSequenceFailed)
				{
					// 残念！途中で間違えていたので、すべて押し終わったこのタイミングでリセット！
					// （※ここで「ブッブー！」という音を鳴らすイベントを呼ぶと最高です）
					ResetSequence();
					return false;
				}
				else
				{
					// パーフェクト！一度も間違えずに最後まで押せた！
					return true;
				}
			}
			else
			{
				// まだ途中なので全体の条件としてはfalseのまま待機
				return false;
			}
		}
	}

	return false;
}

void AEventTrigger::ResetSequence()
{
	CurrentPressCount = 0;
	bIsSequenceFailed = false; // フラグもリセット

	if (!TriggerData) return;

	//// パズルに関わるすべてのスイッチを強制的にOFFに戻す
	//for (const FConditionSource& ConditionSource : TriggerData->ConditionSources)
	//{
	//	if (AActor* ConditionSourceActor = ConditionSource.SourceActor.Get())
	//	{
	//		if (UConditionSourceComponent* Comp = Cast<UConditionSourceComponent>(ConditionSourceActor->GetComponentByClass(UConditionSourceComponent::StaticClass())))
	//		{
	//			Comp->bConditionMet = false;
	//			// ※必要に応じてBP側に「見た目を戻して」とイベントを送る
	//		}
	//	}
	//}
}
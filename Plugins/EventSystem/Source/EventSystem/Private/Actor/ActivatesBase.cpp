// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/ActivatesBase.h"

// Sets default values
AActivatesBase::AActivatesBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 変数の初期化
	TargetSequence = nullptr;

	// コンポーネントの作成とアタッチ
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;

}

// Called when the game starts or when spawned
void AActivatesBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AActivatesBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


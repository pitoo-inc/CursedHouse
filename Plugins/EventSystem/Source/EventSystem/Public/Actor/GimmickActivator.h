// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/GimmickTriggerBase.h"
#include "Interface/ActivatableInterface.h"
#include "Interface/StatefulActivatableInterface.h"
#include "GimmickActivator.generated.h"

/**
 * GimmickTriggerBaseを継承し、ActivatesBaseの機能を追加
 */
UCLASS()
class EVENTSYSTEM_API AGimmickActivator : public AGimmickTriggerBase, public IActivatableInterface, public IStatefulActivatableInterface
{
	GENERATED_BODY()

public:
	AGimmickActivator();
};
// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/GimmickTriggerBase.h"
#include "Component/ConditionSourceComponent.h"


AGimmickTriggerBase::AGimmickTriggerBase()
{
	ConditionSource = CreateDefaultSubobject<UConditionSourceComponent>(TEXT("ConditionSource"));
}

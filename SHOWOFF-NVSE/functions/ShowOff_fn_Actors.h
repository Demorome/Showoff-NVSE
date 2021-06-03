﻿#pragma once

DEFINE_CMD_ALT_COND_PLUGIN(GetNumActorsInRangeFromRef, , "Returns the amount of actors that are a certain distance nearby to the calling reference.", 1, kParams_OneFloat_OneInt);
DEFINE_CMD_ALT_COND_PLUGIN(GetNumCombatActorsFromActor, , "Returns the amount of actors that are allies/targets to the calling actor, with optional filters.", 1, kParams_OneFloat_OneInt);
DEFINE_COMMAND_PLUGIN(GetCreatureTurningSpeed, , 0, 1, kParams_OneOptionalActorBase);  //copied after GetCreatureCombatSkill from JG
DEFINE_COMMAND_PLUGIN(SetCreatureTurningSpeed, , 0, 2, kParams_OneFloat_OneOptionalActorBase);
DEFINE_COMMAND_PLUGIN(GetCreatureFootWeight, , 0, 1, kParams_OneOptionalActorBase);
DEFINE_COMMAND_PLUGIN(SetCreatureFootWeight, , 0, 2, kParams_OneFloat_OneOptionalActorBase);
DEFINE_COMMAND_PLUGIN(SetCreatureReach, , 0, 2, kParams_OneInt_OneOptionalActorBase);
DEFINE_COMMAND_PLUGIN(SetCreatureBaseScale, , 0, 2, kParams_OneFloat_OneOptionalActorBase);
DEFINE_CMD_ALT_COND_PLUGIN(GetNumCompassHostilesInRange, , "Returns the amount of hostile actors that are a certain distance nearby to the player that appear on the compass.", 0, kParams_OneOptionalFloat_OneOptionalInt);



//Code ripped from both JIP (GetActorsByProcessingLevel) and SUP.
UINT32 __fastcall GetNumActorsInRangeFromRefCALL(TESObjectREFR* const thisObj, float const range, UInt32 const flags)
{
	if (range <= 0) return 0;
	if (!thisObj) return 0;

#define DebugGetNumActorsInRangeFromRef 1;
	
	UInt32 numActors = 0;
	bool const noDeadActors = flags & 1;
	//bool const something = flags & 2;
	
	MobileObject** objArray = g_processManager->objects.data, ** arrEnd = objArray;
	objArray += g_processManager->beginOffsets[0];  //Only objects in High process.
	arrEnd += g_processManager->endOffsets[0];

	for (; objArray != arrEnd; objArray++)
	{
		auto actor = (Actor*)*objArray;
		if (actor && actor->IsActor() && actor != thisObj)
		{
#if DebugGetNumActorsInRangeFromRef 
			Console_Print("Current actor >>> %08x (%s)", actor->refID, actor->GetName());
#endif
			
			if (noDeadActors && actor->GetDead())
				continue;

			if (GetDistance3D(thisObj, actor) <= range)
				numActors++;
		}
	}

	// Player is not included in the looped array, so we need to check for it outside the loop.
	if (thisObj != g_thePlayer)
	{
		if (noDeadActors)
		{
			if (g_thePlayer->GetDead())
				return numActors;
		}
		if (GetDistance3D(thisObj, g_thePlayer) <= range)
		{
			numActors++;
		}
	}

#undef DebugGetNumActorsInRangeFromRef
	return numActors; 
}

bool Cmd_GetNumActorsInRangeFromRef_Eval(COMMAND_ARGS_EVAL)
{
	*result = GetNumActorsInRangeFromRefCALL(thisObj, *(float*)&arg1, (UInt32)arg2);
	return true;
}
bool Cmd_GetNumActorsInRangeFromRef_Execute(COMMAND_ARGS)
{
	float range = 0;
	UINT32 flags = 0;
	if (ExtractArgs(EXTRACT_ARGS, &range, &flags))
		*result = GetNumActorsInRangeFromRefCALL(thisObj, range, flags);
	else
		*result = 0;
	return true;
}


//Code ripped off of JIP's GetCombatActors.
UINT32 __fastcall GetNumCombatActorsFromActorCALL(TESObjectREFR* thisObj, float range, UInt32 flags)
{
	if (!thisObj) return 0;
	if (!thisObj->IsActor()) return 0;
	if (!flags) return 0;
	//Even if the calling actor is dead, they could still have combat targets, so we don't filter that out.
	
	UINT32 numActors = 0;
	bool const getAllies = flags & 0x1;
	bool const getTargets = flags & 0x2;
	
	Actor* actor;
	if (range <= 0)  //ignore distance.
	{
		if (thisObj == g_thePlayer)
		{
			CombatActors* cmbActors = g_thePlayer->combatActors;
			if (!cmbActors) return numActors;
			if (getAllies)
			{
				numActors += cmbActors->allies.size;   //thisObj is its own combat ally, for whatever reason...
				numActors--;                           //So we decrement it by one to get rid of that.
			}
			if (getTargets)
			{
				numActors += cmbActors->targets.size;
			}
		}
		else
		{
			actor = (Actor*)thisObj;
			if (getAllies && actor->combatAllies)
			{
				numActors += actor->combatAllies->size;
			}
			if (getTargets && actor->combatTargets)
			{
				numActors += actor->combatTargets->size;
			}
		}
	}
	else  //---Account for distance.
	{
		UINT32 count;
		
		if (thisObj == g_thePlayer)
		{
			CombatActors* cmbActors = g_thePlayer->combatActors;
			if (!cmbActors) return numActors;
			if (getAllies)
			{
				CombatAlly* allies = cmbActors->allies.data;
				for (count = cmbActors->allies.size; count; count--, allies++)
				{
					actor = allies->ally;
					if (actor && (actor != thisObj))
					{
						if (GetDistance3D(thisObj, actor) <= range)
						{
							numActors++;
						}
					}
				}
			}
			if (getTargets)
			{
				CombatTarget* targets = cmbActors->targets.data;
				for (count = cmbActors->targets.size; count; count--, targets++)
				{
					actor = targets->target;
					if (actor)
					{
						if (GetDistance3D(thisObj, actor) <= range)
						{
							numActors++;
						}
					}
				}
			}
		}
		else
		{
			actor = (Actor*)thisObj;
			Actor** actorsArr = NULL;
			if (getAllies && actor->combatAllies)
			{
				actorsArr = actor->combatAllies->data;
				if (actorsArr)
				{
					count = actor->combatAllies->size;
					for (; count; count--, actorsArr++)   //can I merge these two loops, to be easier to debug?
					{
						actor = *actorsArr;
						if (actor && (actor != thisObj))  //thisObj is its own combat ally, for whatever reason...
						{
							if (GetDistance3D(thisObj, actor) <= range)
							{
								numActors++;
							}
						}
					}
				}
			}
			if (getTargets && actor->combatTargets)  
			{
				actorsArr = actor->combatTargets->data;
				if (actorsArr)
				{
					count = actor->combatTargets->size;
					for (; count; count--, actorsArr++)   //can I merge these two loops, to be easier to debug?
					{
						actor = *actorsArr;
						if (actor)  //thisObj cannot be its own target, no need to check against that.
						{
							if (GetDistance3D(thisObj, actor) <= range)
							{
								numActors++;
							}
						}
					}
				}
			}

		}
	}
	return numActors;
}

bool Cmd_GetNumCombatActorsFromActor_Eval(COMMAND_ARGS_EVAL)
{
	*result = GetNumCombatActorsFromActorCALL(thisObj, *(float*)&arg1, (UInt32)arg2);
	return true;
}
bool Cmd_GetNumCombatActorsFromActor_Execute(COMMAND_ARGS)
{
	float range;
	UINT32 flags;
	if (ExtractArgs(EXTRACT_ARGS, &range, &flags))
		*result = GetNumCombatActorsFromActorCALL(thisObj, range, flags);
	else
		*result = 0;
	return true;
}


bool Cmd_GetCreatureTurningSpeed_Execute(COMMAND_ARGS)
{
	*result = 0;
	TESCreature* creature = NULL;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &creature)) return true;
	if (!creature)
	{
		if (!thisObj || !thisObj->IsActor()) return true;
		creature = (TESCreature*)((Actor*)thisObj)->GetActorBase();
	}
	if IS_TYPE(creature, TESCreature)
		*result = creature->turningSpeed;
	return true;
}


bool Cmd_SetCreatureTurningSpeed_Execute(COMMAND_ARGS)
{
	*result = 0;
	TESCreature* creature = NULL;
	float turningSpeed = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &turningSpeed, &creature)) return true;
	if (!creature)
	{
		if (!thisObj || !thisObj->IsActor()) return true;
		creature = (TESCreature*)((Actor*)thisObj)->GetActorBase();
	}
	if IS_TYPE(creature, TESCreature)
	{
		creature->turningSpeed = turningSpeed;
		*result = 1;
	}
	return true;
}


bool Cmd_GetCreatureFootWeight_Execute(COMMAND_ARGS)
{
	*result = 0;
	TESCreature* creature = NULL;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &creature)) return true;
	if (!creature)
	{
		if (!thisObj || !thisObj->IsActor()) return true;
		creature = (TESCreature*)((Actor*)thisObj)->GetActorBase();
	}
	if IS_TYPE(creature, TESCreature)
		*result = creature->footWeight;
	return true;
}


bool Cmd_SetCreatureFootWeight_Execute(COMMAND_ARGS)
{
	*result = 0;
	TESCreature* creature = NULL;
	float footWeight = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &footWeight, &creature)) return true;
	if (!creature)
	{
		if (!thisObj || !thisObj->IsActor()) return true;
		creature = (TESCreature*)((Actor*)thisObj)->GetActorBase();
	}
	if IS_TYPE(creature, TESCreature)
	{
		creature->footWeight = footWeight;
		*result = 1;
	}
	return true;
}


bool Cmd_SetCreatureReach_Execute(COMMAND_ARGS)
{
	*result = 0;
	TESCreature* creature = NULL;
	UInt32 reach = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &reach, &creature)) return true;
	if (!creature)
	{
		if (!thisObj || !thisObj->IsActor()) return true;
		creature = (TESCreature*)((Actor*)thisObj)->GetActorBase();
	}
	if IS_TYPE(creature, TESCreature)
	{
		creature->attackReach = reach;
		*result = 1;
	}
	return true;
}


bool Cmd_SetCreatureBaseScale_Execute(COMMAND_ARGS)
{
	*result = 0;
	TESCreature* creature = NULL;
	float newVal = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &newVal, &creature)) return true;
	if (!creature)
	{
		if (!thisObj || !thisObj->IsActor()) return true;
		creature = (TESCreature*)((Actor*)thisObj)->GetActorBase();
	}
	if IS_TYPE(creature, TESCreature)
	{
		creature->baseScale = newVal;
		*result = 1;
	}
	return true;
}




float g_compassHostiles_Range = 0;  //necessary since can't pass float as void*

//Copied JG's GetNearestCompassHostile code.
bool Cmd_GetNumCompassHostilesInRange_Eval(COMMAND_ARGS_EVAL)
{
	*result = 0;
	float range = *(float*)&arg1;  //check if this works!
	UInt32 skipInvisible = (UInt32)arg2;
	UInt32 numHostiles = 0;

	if (!range && g_compassHostiles_Range)
	{
		range = g_compassHostiles_Range;  //g_compassHostiles_Range is always 0, except when it is fed a value in _Execute.
	}
	g_compassHostiles_Range = 0;

	//To avoid counting "compass targets" that are super far away and can't even be seen on compass (I assume).
	float fSneakMaxDistance = *(float*)(0x11CD7D8 + 4);
	float fSneakExteriorDistanceMult = *(float*)(0x11CDCBC + 4);
	bool isInterior = g_thePlayer->GetParentCell()->IsInterior();
	float interiorDistanceSquared = fSneakMaxDistance * fSneakMaxDistance;
	float exteriorDistanceSquared = (fSneakMaxDistance * fSneakExteriorDistanceMult) * (fSneakMaxDistance * fSneakExteriorDistanceMult);
	float maxDist = isInterior ? interiorDistanceSquared : exteriorDistanceSquared;

	NiPoint3* playerPos = g_thePlayer->GetPos();
	auto iter = g_thePlayer->compassTargets->Begin();
	for (; !iter.End(); ++iter)
	{
		PlayerCharacter::CompassTarget* target = iter.Get();
		if (target->isHostile)
		{
			if (skipInvisible > 0 && (target->target->avOwner.Fn_02(kAVCode_Invisibility) > 0 || target->target->avOwner.Fn_02(kAVCode_Chameleon) > 0)) {
				continue;
			}
			auto distToPlayer = target->target->GetPos()->CalculateDistSquared(playerPos);
			if (distToPlayer < maxDist)
			{
				if (range)
				{
					if (distToPlayer < range)
						numHostiles++;
				}
				else
				{
					numHostiles++;
				}
			}
		}
	}
	*result = numHostiles;
	return true;
}
bool Cmd_GetNumCompassHostilesInRange_Execute(COMMAND_ARGS)
{
	*result = 0;
	g_compassHostiles_Range = 0;
	UInt32 skipInvisible = 0;

	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &g_compassHostiles_Range, &skipInvisible)) return true;

	return Cmd_GetNumCompassHostilesInRange_Eval(thisObj, 0, (void*)skipInvisible, result);
}




#ifdef _DEBUG



DEFINE_CMD_ALT_COND_PLUGIN(HasAnyScriptPackage, , , 1, NULL);
bool Cmd_HasAnyScriptPackage_Eval(COMMAND_ARGS_EVAL)
{
	*result = 0;
	if (thisObj->IsActor())
	{
		ExtraDataList* xList = &thisObj->extraDataList;
		//*result = ThisStdCall<UINT32>(0x41CB10, xList);
		ExtraPackage* xPackage = GetExtraTypeJIP(&thisObj->extraDataList, Package);
		if (xPackage)
		{
			*result = xPackage->unk10[2];  //kill meh, doesn't work. 0x41CB10 is the best lead I have.
		}
		//bool const bTest = ThisStdCall<bool>(0x674D40, package);
		//Console_Print("ActorHasAnyScriptPackage TEST >> %d", bTest);
		//*result = ?
	} 
	return true;
}
bool Cmd_HasAnyScriptPackage_Execute(COMMAND_ARGS)
{
	return Cmd_HasAnyScriptPackage_Eval(thisObj, 0, 0, result);
}




#endif
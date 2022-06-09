#pragma once
#include "ShowOffNVSE.h"
#include "utility.h"

#if 0
static EquipData FindEquipped(TESObjectREFR* thisObj, FormMatcher& matcher) {
	ExtraContainerChanges* pContainerChanges = static_cast<ExtraContainerChanges*>(thisObj->extraDataList.GetByType(kExtraData_ContainerChanges));
	return (pContainerChanges) ? pContainerChanges->FindEquipped(matcher) : EquipData();
}
#endif

static EquipDataSet GetEquippedItems(TESObjectREFR* actorRef, UInt32 const filterFlags = 0) {
	auto const pContainerChanges = dynamic_cast<ExtraContainerChanges*>(actorRef->extraDataList.GetByType(kExtraData_ContainerChanges));
	return pContainerChanges ? FindEquippedItems(pContainerChanges, filterFlags) : EquipDataSet();
}

typedef TESBipedModelForm::EPartBit EquippedItemIndex;
typedef TESBipedModelForm::ESlot EquippedItemSlot;

UInt32 __fastcall GetNumBrokenEquippedItems_Call(TESObjectREFR* const thisObj, float threshold, UInt32 const flags)
{
	if (!IS_ACTOR(thisObj)) return 0;
	threshold /= 100.0F;  //expecting a number like 35, reduce to 0.35
	UInt32 numBrokenItems = 0;  //return value.
	auto eqItems = GetEquippedItems(thisObj, flags);
	for (auto iter : eqItems)
	{
		if (g_ShowFuncDebug)
			Console_Print("GetNumBrokenEquippedItems - iter form: [%08X] (%s)", iter.pForm, iter.pForm->GetName());

		auto const pHealth = DYNAMIC_CAST(iter.pForm, TESForm, TESHealthForm);  // base health
		if (!pHealth) continue;
		float baseHealth = pHealth->health;

		//todo: modify baseHealth if the item is a weapon by checking if it has the weapon mod equipped (check xData)
		// Check if Jazz's code at https://discord.com/channels/711228477382328331/816602410012639262/869359398978469911 gets released (could be used here).

		ExtraHealth* pXHealth = iter.pExtraData ? (ExtraHealth*)iter.pExtraData->GetByType(kExtraData_Health) : NULL; // modified health data
		if (pXHealth)  // If there's no pXHealth, it's at 100% health (no modified health extra data).
		{
			float const currentHealth = pXHealth->health;
			if ((currentHealth / baseHealth) <= threshold) numBrokenItems++;
			if (g_ShowFuncDebug)
				Console_Print("GetNumBrokenEquippedItems - health %% check being performed on %s. %%: %f vs %f threshold", iter.pForm->GetName(), (pXHealth->health / (float)pHealth->health), threshold);
		}
		else if (threshold >= 1.0F) numBrokenItems++;
	}
	if (IsConsoleMode())
		Console_Print("GetNumBrokenEquippedItems >> %u", numBrokenItems);
	return numBrokenItems;
}

DEFINE_CMD_ALT_COND_PLUGIN(GetNumBrokenEquippedItems, GetNumBrokenEq, "", true, kParams_OneOptionalFloat_OneOptionalInt);
bool Cmd_GetNumBrokenEquippedItems_Eval(COMMAND_ARGS_EVAL)
{
	float const threshold = *(float*)&arg1;
	auto const flags = (UInt32)arg2;
	*result = GetNumBrokenEquippedItems_Call(thisObj, threshold, flags);
	return true;
}
bool Cmd_GetNumBrokenEquippedItems_Execute(COMMAND_ARGS)
{
	*result = 0;
	float threshold = 0;
	UInt32 flags = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &threshold, &flags)) return true;
	*result = GetNumBrokenEquippedItems_Call(thisObj, threshold, flags);
	return true;
}


DEFINE_CMD_ALT_COND_PLUGIN(GetEquippedItemsAsBitMask, GetOccupiedEquipSlots, , true, NULL);
bool Cmd_GetEquippedItemsAsBitMask_Eval(COMMAND_ARGS_EVAL)
{
	*result = 0;
	if (!IS_ACTOR(thisObj)) return true;
	UInt32 equipSlotMask = 0;  //return value.
	auto eqItems = GetEquippedItems(thisObj, FindEquipped::iFilter_AllSlotsNoSlotless);
	for (auto const& iter : eqItems)
	{
		auto form = iter.pForm;
		auto const formSlotMask = GetFormEquipSlotMask(form);
		equipSlotMask |= formSlotMask;
		if (g_ShowFuncDebug)
			Console_Print("GetEquippedItemsAsBitMask - Form: %s, mask to add: %x", form->GetName(), formSlotMask);
	}
	*result = equipSlotMask;
	if (IsConsoleMode())
		Console_Print("GetEquippedItemsAsBitMask >> %x", equipSlotMask);
	return true;
}
bool Cmd_GetEquippedItemsAsBitMask_Execute(COMMAND_ARGS)
{
	return Cmd_GetEquippedItemsAsBitMask_Eval(thisObj, 0, 0, result);
}

DEFINE_CMD_ALT_PLUGIN(UnequipItems, TakeThatShitOff, "", true, kParams_FourOptionalInts);
//todo: Could use a lot more testing
bool Cmd_UnequipItems_Execute(COMMAND_ARGS)
{
	UInt32 flags = 0, noEquip = 0, hideMessage = 0, triggerOnUnequip = 1;
	
	if (!ExtractArgs(EXTRACT_ARGS, &flags, &noEquip, &hideMessage, &triggerOnUnequip)
		|| NOT_ACTOR(thisObj))
	{
		return true;
	}
	
	auto eqItems = GetEquippedItems(thisObj, flags);
	for (auto const& iter : eqItems)
	{
		ExtraDataList* xData = triggerOnUnequip ? iter.pExtraData : nullptr;
		((Actor*)thisObj)->UnequipItem(iter.pForm, 1, xData, 1, noEquip != 0, hideMessage != 0);
	}
	return true;
}

DEFINE_CMD_ALT_PLUGIN(GetEquippedItems, TheFuckShitAreYouWearing, "", true, kParams_OneOptionalInt);
bool Cmd_GetEquippedItems_Execute(COMMAND_ARGS)
{
	UInt32 flags = 0;
	*result = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &flags) || NOT_ACTOR(thisObj)) return true;
	Vector<ArrayElementR> elems;
	auto eqItems = GetEquippedItems(thisObj, flags);
	for (auto const& iter : eqItems)
	{
		ArrayElementR elem = iter.pForm;
		elems.Append(elem);
	}
	auto const array = CreateArray(elems.Data(), elems.Size(), scriptObj);
	AssignArrayResult(array, result);
	return true;
}

DEFINE_CMD_ALT_PLUGIN(GetEquippedItemRefs, TheFuckShitAreYouWearingRefs, "", true, kParams_OneOptionalInt);
bool Cmd_GetEquippedItemRefs_Execute(COMMAND_ARGS)
{
	UInt32 flags = 0;
	*result = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &flags) || NOT_ACTOR(thisObj)) return true;
	Vector<ArrayElementR> elems;
	auto eqItems = GetEquippedItems(thisObj, flags);
	for (auto const& [baseItem, xData] : eqItems)
	{
		auto const itemRef = InventoryRefCreateEntry((Actor*)thisObj, baseItem, 1, xData);
		ArrayElementR elem = itemRef;
		elems.Append(elem);
	}
	auto const array = CreateArray(elems.Data(), elems.Size(), scriptObj);
	AssignArrayResult(array, result);
	return true;
}

DEFINE_CMD_ALT_PLUGIN(RemoveAllItemsShowOff, YouDontNeedAllThisShit, "", true, kParams_TwoOptionalInts_OneOptionalContainerRef_OneOptionalList);



DEFINE_CMD_COND_PLUGIN(GetEquippedWeaponType, "", true, NULL);
bool Cmd_GetEquippedWeaponType_Eval(COMMAND_ARGS_EVAL)
{
	*result = 0;
	if (!IS_ACTOR(thisObj)) return true;
	TESObjectWEAP* weapon = ((Actor*)thisObj)->GetEquippedWeapon();
	if (weapon)
		*result = weapon->eWeaponType;
	return true;
}
bool Cmd_GetEquippedWeaponType_Execute(COMMAND_ARGS)
{
	return Cmd_GetEquippedWeaponType_Eval(thisObj, 0, 0, result);
}

float __fastcall GetBaseEquippedWeight_Call(TESObjectREFR* const thisObj, UInt32 const flags, float const minWeight)
{
	float totalWeight = 0;  //return val.
	auto eqItems = GetEquippedItems(thisObj, flags);
	for (auto const& iter : eqItems)
	{
		if (!iter.pForm) continue;
		auto pWeight = DYNAMIC_CAST(iter.pForm, TESForm, TESWeightForm);
		if (pWeight)
		{
			if (pWeight->weight >= minWeight)
			{
				totalWeight += pWeight->weight;
			}
		}
	}
	if (IsConsoleMode())
		Console_Print("GetBaseEquippedWeight >> %f", totalWeight);
	return totalWeight;
}

DEFINE_CMD_COND_PLUGIN(GetBaseEquippedWeight, "", true, kParams_OneOptionalFloat_OneOptionalInt);
bool Cmd_GetBaseEquippedWeight_Eval(COMMAND_ARGS_EVAL)
{
	*result = 0;
	if (!IS_ACTOR(thisObj)) return true;
	float const minWeight = *(float*)&arg1;
	UInt32 const flags = (UInt32)arg2;
	*result = GetBaseEquippedWeight_Call(thisObj, flags, minWeight);
	return true;
}
bool Cmd_GetBaseEquippedWeight_Execute(COMMAND_ARGS)
{
	*result = 0;
	float minWeight = 0;
	UInt32 flags = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &minWeight, &flags)) return true;
	*result = GetBaseEquippedWeight_Call(thisObj, flags, minWeight);
	return true;
}

// Code structure lifted from 0x4D0900 (GetInventoryWeight)
float __fastcall GetCalculatedEquippedWeight_Call(TESObjectREFR* const thisObj, UInt32 const flags, float const minWeight)
{
	float totalWeight = 0;  //return val.
	bool isHardcore = g_thePlayer->isHardcore;
	if (!thisObj || !((Actor*)thisObj)->baseProcess) return 0.0F;
	auto eqItems = GetEquippedItems(thisObj, flags);
	for (auto const& iter : eqItems)
	{
		TESForm* item = iter.pForm;
		if (!item) continue;
		float itemWeight = 0;

		bool const isWeapon = IS_TYPE(item, TESObjectWEAP);
		if (isWeapon) {
			ContChangesEntry* weapInfo = nullptr;
			// Gather more information about the weapon reference (weapon mods).
			if (!(weapInfo = ((Actor*)thisObj)->baseProcess->GetWeaponInfo())) continue;
			if (item = weapInfo->type) {
				bool const hasDecreaseWeightMod = ThisStdCall<bool>(0x4BDA70, weapInfo, TESObjectWEAP::kWeaponModEffect_DecreaseWeight);
				itemWeight = ThisStdCall<double>(0x4BE380, (TESObjectWEAP*)item, hasDecreaseWeightMod);  //GetWeaponModdedWeight
				if (itemWeight >= 10.0) {
					float heavyWeaponWeightMult = 1.0;
					ApplyPerkModifiers(kPerkEntry_AdjustHeavyWeaponWeight, (Actor*)g_thePlayer, &heavyWeaponWeightMult);
					itemWeight = itemWeight * heavyWeaponWeightMult;
				}
			}
		}
		else {
			itemWeight = CdeclCall<double>(0x48EBC0, item, isHardcore);  // GetItemWeight. isHardcore check only affects ammo, but whatever.
		}

		if (itemWeight > 0.0F)
		{
			float hasPackRatFlt = 0.0;
			ApplyPerkModifiers(kPerkEntry_ModifyLightItems, (Actor*)thisObj, &hasPackRatFlt);
			if (hasPackRatFlt > 0.0)
			{
				float const fPackRatThreshold = *(float*)(0x11C6478 + 4);
				float const fPackRatModifier = *(float*)(0x11C64A8 + 4);
				if (fPackRatThreshold >= (double)itemWeight)
					itemWeight = itemWeight * fPackRatModifier;
			}
#if 0		// todo: figure out wtf 0x4D0D83 does.
			if (isWeapon && weapInfo)
			{
				//NOTE: Game does some weird jank to account for multiple DecreaseWeight effects.
				//I just really have no idea what's going on at 0x4D0D83 .
				bool const hasDecreaseWeightEffect = ThisStdCall<bool>(0x4BDA70, weapInfo, TESObjectWEAP::kWeaponModEffect_DecreaseWeight);
				if (hasDecreaseWeightEffect) {
					itemWeight *= ThisStdCall<float>(0x4BCF60, weapInfo->type, TESObjectWEAP::kWeaponModEffect_DecreaseWeight, 1);
				}
			}
#endif
			if (g_ShowFuncDebug)
				Console_Print("GetCalculatedEquippedWeight - Item: %s, Calculated Weight: %f", item->GetName(), itemWeight);

			if (itemWeight >= minWeight)
				totalWeight += itemWeight;
		}
	}
	if (IsConsoleMode())
		Console_Print("GetCalculatedEquippedWeight >> %f", totalWeight);
	return totalWeight;
}

DEFINE_CMD_COND_PLUGIN(GetCalculatedEquippedWeight, "Accounts for perk effects + weapon mods.", true, kParams_OneOptionalFloat_OneOptionalInt);
bool Cmd_GetCalculatedEquippedWeight_Eval(COMMAND_ARGS_EVAL)
{
	*result = 0;
	if (!IS_ACTOR(thisObj)) return true;
	float const minWeight = *(float*)&arg1;
	UInt32 const flags = (UInt32)arg2;
	*result = GetCalculatedEquippedWeight_Call(thisObj, flags, minWeight);
	return true;
}
bool Cmd_GetCalculatedEquippedWeight_Execute(COMMAND_ARGS)
{
	*result = 0;
	float minWeight = 0;
	UInt32 flags = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &minWeight, &flags)) return true;
	*result = GetCalculatedEquippedWeight_Call(thisObj, flags, minWeight);
	return true;
}

#if 0
DEFINE_COMMAND_PLUGIN(UnequipItemsFromBitMask, , 1, 1, kParams_OneInt);
bool Cmd_UnequipItemsFromBitMask_Execute(COMMAND_ARGS)
{
	UInt32 flags = 0, noEquip = 0, noMessage = 1;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &flags, &noEquip, &noMessage)) return true;
	if (!IS_ACTOR(thisObj)) return true;
	Actor* actor = (Actor*)thisObj;
	for (UInt32 slotIdx = EquippedItemIndex::ePart_Head; slotIdx <= EquippedItemIndex::ePart_BodyAddon3; slotIdx++)
	{
		MatchBySlot matcher(slotIdx);
		EquipData equipD = FindEquipped(thisObj, matcher);
		if (equipD.pForm)
		{
			if (flags & TESBipedModelForm::MaskForSlot(slotIdx))
			{
				//Unequip item if its matching flag is set.
				actor->UnequipItem(equipD.pForm, 1, xData, 1, noEquip != 0, noMessage != 0);
			}
		}
	}
	return true;
}
#endif


DEFINE_COMMAND_PLUGIN(GetIngestibleConsumeSound, "", false, kParams_OneOptionalForm);
bool Cmd_GetIngestibleConsumeSound_Execute(COMMAND_ARGS)
{
	*result = 0;
	TESForm* form = nullptr;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &form))
	{
		return true;
	}
	
	if (form = TryExtractBaseForm(form, thisObj))
	{
		if (auto const ingestible = DYNAMIC_CAST(form, TESForm, AlchemyItem))
		{
			REFR_RES = ingestible->consumeSound->refID;
		}
	}
	return true;
}

DEFINE_COMMAND_PLUGIN(SetIngestibleConsumeSound, "", false, kParams_OneForm_OneOptionalForm);
bool Cmd_SetIngestibleConsumeSound_Execute(COMMAND_ARGS)
{
	//Returns if the change was successful or not.
	*result = false;
	
	TESSound* newSound;
	TESForm* form = nullptr;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &newSound, &form) || NOT_TYPE(newSound, TESSound))
	{
		return true;
	}
	
	if (form = TryExtractBaseForm(form, thisObj))
	{
		if (auto const ingestible = DYNAMIC_CAST(form, TESForm, AlchemyItem))
		{
			ingestible->consumeSound = newSound;
			*result = true;
		}
	}
	return true;
}

//assumes the form arg is a valid item baseform.
TESObjectREFR* __fastcall GetEquippedItemRefForItem_Call(Actor* actor, TESForm* baseItem)
{
	auto const contChanges = dynamic_cast<ExtraContainerChanges*>(actor->extraDataList.GetByType(kExtraData_ContainerChanges));
	if (contChanges->data && contChanges->data->objList)
	{
		for (auto itemIter = contChanges->data->objList->Begin(); !itemIter.End(); ++itemIter)  // see GetHotkeyItem from NVSE for similar loop example.
		{
			if (baseItem != itemIter->type)
				continue;

			// Loop thru xData to check if item is Equipped.
			for (auto xDataIter = itemIter->extendData->Begin(); !xDataIter.End(); ++xDataIter)
			{
				if (xDataIter->HasType(kExtraData_Worn))
				{
					return InventoryRefCreateEntry(actor, itemIter->type, itemIter->countDelta, xDataIter.Get());
				}
			}
		}
	}
	return nullptr;
}

DEFINE_COMMAND_PLUGIN(GetEquippedItemRefForItem, "Returns the equipped inv ref for the base item if that base item is equipped.", true, kParams_OneForm);
bool Cmd_GetEquippedItemRefForItem_Execute(COMMAND_ARGS)
{
	*result = 0;
	TESForm* itemForm;
	if (ExtractArgsEx(EXTRACT_ARGS_EX, &itemForm) && itemForm
		&& IS_ACTOR(thisObj)
		&& itemForm->IsItem() && !IS_REFERENCE(itemForm))
	{
		if (auto const itemRef = GetEquippedItemRefForItem_Call((Actor*)thisObj, itemForm))
		{
			REFR_RES = itemRef->refID;
		}
	}
	return true;
}

// NOTE: Does not account for DestructibleObject data, which can store its own health.
DEFINE_CMD_COND_PLUGIN(GetItemCanHaveHealth, "Returns 1 if the item form can be given health data, 0 otherwise.", false, kParams_OneOptionalForm);
bool Cmd_GetItemCanHaveHealth_Eval(COMMAND_ARGS_EVAL)
{
	*result = 0;	//canHaveHealth
	if (auto const base_form = TryExtractBaseForm((TESForm*)arg1, thisObj))
	{
		if (base_form->IsItem() 
			&& DYNAMIC_CAST(base_form, TESForm, TESHealthForm) // can be true for actors and armors/weapons
			)
		{
			*result = 1;
		}
	}
	return true;
}
bool Cmd_GetItemCanHaveHealth_Execute(COMMAND_ARGS)
{
	*result = 0;	//canHaveHealth
	TESForm* form = nullptr;
	if (ExtractArgsEx(EXTRACT_ARGS_EX, &form))
	{
		return Cmd_GetItemCanHaveHealth_Eval(thisObj, form, nullptr, result);
	}
	return true;
}


DEFINE_CMD_COND_PLUGIN(GetCalculatedItemValue, "Returns the item's value, affected by condition and any attached weapon mods (and extra).", \
	false, kParams_OneOptionalInt_OneOptionalObject);
bool Cmd_GetCalculatedItemValue_Eval(COMMAND_ARGS_EVAL)
{
	*result = -1;
	auto baseItem = (TESForm*)arg2;	// optional arg
	auto const itemRef = thisObj;	// trust it is an item ref...

	if (baseItem = TryExtractBaseForm(baseItem, thisObj);
		baseItem && baseItem->IsItem())
	{
		auto const invRef = InventoryRefGetForID(thisObj->refID);

		auto itemVal = -1.0;
		if (auto const bAccountForBarterChanges = (UInt32)arg1;
			bAccountForBarterChanges && BarterMenu::Get() && invRef)
		{
			auto const brtMenu = BarterMenu::Get();

			// BUG: above may not work reliably when used as a condition (function may be evaluated before barter mult perk effect is applied).
			itemVal = brtMenu->CalculateItemPrice(invRef->data.entry);
		}
		else
		{
			// Count = 1, so this doesn't account for stacked item references.
			//copied after GetCalculatedWeaponDamage from JIP, thx for the pointer c6.
			ContChangesEntry tempEntry(NULL, 1, baseItem);
			if (invRef)
			{
				tempEntry = *invRef->data.entry;
			}
			else if (itemRef)
			{
				ExtraContainerChanges::ExtendDataList extendData;
				extendData.Init(&itemRef->extraDataList);
				tempEntry.extendData = &extendData;
			}

			// Calculate Item Price, without barter mults. 
			// Accounts for item mods and condition as well.
			itemVal = ThisStdCall<double>(0x4BD400, &tempEntry);
		}

		*result = itemVal;
	}

	return true;
}
bool Cmd_GetCalculatedItemValue_Execute(COMMAND_ARGS)
{
	*result = -1;
	UInt32 bAccountForBarterChanges = 0;
	TESForm* item = nullptr;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &bAccountForBarterChanges, &item))
		return true;

	return Cmd_GetCalculatedItemValue_Eval(thisObj, (void*)bAccountForBarterChanges, item, result);
}

DEFINE_COMMAND_PLUGIN(GetEquippedWeapon, "", true, nullptr);
bool Cmd_GetEquippedWeapon_Execute(COMMAND_ARGS)
{
	auto weap = ((Actor*)thisObj)->GetEquippedWeapon();
	if (!weap)
		weap = g_fistsWeapon;
	REFR_RES = weap->refID;
	return true;
}

DEFINE_COMMAND_PLUGIN(GetEquippedWeaponRef, "", true, nullptr);
bool Cmd_GetEquippedWeaponRef_Execute(COMMAND_ARGS)
{
	*result = 0;
	//Copying JIP's GetEquippedItemRef
	auto weapInfo = ((Actor*)thisObj)->baseProcess->GetWeaponInfo();
	if (weapInfo && weapInfo->extendData)
	{
		if (auto const invRef = InventoryRefCreateEntry(thisObj, weapInfo->type, weapInfo->countDelta, weapInfo->extendData->GetFirstItem()))
			REFR_RES = invRef->refID;
	}
	return true;
}














#if _DEBUG


bool CanItemRepairTarget(TESForm* repairingItem, TESForm* itemToRepair)
{
	// Works even if the forms aren't guaranteed to be items, and if either are null.
	return ThisStdCall_B(0x47BB50, (TESObjectARMO*)repairingItem, (TESObjectARMO*)itemToRepair);
}

DEFINE_CMD_COND_PLUGIN(GetItemCanRepairTarget, "", false, kParams_TwoOptionalObjects);
bool Cmd_GetItemCanRepairTarget_Eval(COMMAND_ARGS_EVAL)
{
	TESForm* repairingItem = thisObj;
	TESForm* itemTarget = nullptr;
	if (!repairingItem)
	{
		repairingItem = (TESForm*)arg1;
		itemTarget = (TESForm*)arg2;
	}
	else
	{
		itemTarget = (TESForm*)arg1;
	}

	*result = CanItemRepairTarget(repairingItem, itemTarget);
	return true;
}
bool Cmd_GetItemCanRepairTarget_Execute(COMMAND_ARGS)
{
	TESForm* repairingItem = nullptr;	// if thisObj is non-null, then this is actually itemTarget.
	TESForm* itemTarget = nullptr;
	
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &repairingItem, &itemTarget))
		return true;
	
	return Cmd_GetItemCanRepairTarget_Eval(thisObj, repairingItem, itemTarget, result);
}

DEFINE_CMD_COND_PLUGIN(GetItemCanBeRepairedByTarget, "Inverse of GetItemCanRepairTarget.", false, kParams_TwoOptionalObjects);
bool Cmd_GetItemCanBeRepairedByTarget_Eval(COMMAND_ARGS_EVAL)
{
	TESForm* itemToRepair = thisObj;
	TESForm* itemTarget = nullptr;	// target = repairingItem
	if (!itemToRepair)
	{
		itemToRepair = (TESForm*)arg1;
		itemTarget = (TESForm*)arg2;
	}
	else
	{
		itemTarget = (TESForm*)arg1;
	}

	*result = CanItemRepairTarget(itemTarget, itemToRepair);
	return true;
}
bool Cmd_GetItemCanBeRepairedByTarget_Execute(COMMAND_ARGS)
{
	TESForm* itemToRepair = nullptr;	// if thisObj is non-null, then this is actually itemTarget.
	TESForm* itemTarget = nullptr;

	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &itemToRepair, &itemTarget))
		return true;

	return Cmd_GetItemCanBeRepairedByTarget_Eval(thisObj, itemToRepair, itemTarget, result);
}

// Will be able to get the inventory reference's original extendDataList.
TESObjectREFR* __fastcall CreateRefForStackWithoutCopy(TESObjectREFR* container, ContChangesEntry* menuEntry)
{
	if (container && menuEntry)
	{
		InventoryRef::Data const data{ menuEntry->type, menuEntry, menuEntry->extendData ? menuEntry->extendData->GetFirstItem() : nullptr };
		auto const invRef = InventoryRefCreate(container, data, false);
		return invRef->tempRef;
	}
	return nullptr;
}

DEFINE_COMMAND_PLUGIN(GetSelectedItemRefSO, "", false, kParams_OneOptionalInt);
//code copied from JIP LN and tweaked to avoid copying the ExtendDataList* (needed for some functions).
//actually doesn't even seem to make a difference? idk
bool Cmd_GetSelectedItemRefSO_Execute(COMMAND_ARGS)
{
	*result = 0;
	UInt32 menuID = 0;
	if (!ExtractArgsEx(EXTRACT_ARGS_EX, &menuID))
		return true;
	TESForm* itemRef = nullptr;
	if (!menuID) 
		menuID = g_interfaceManager->GetTopVisibleMenuID();
	switch (menuID)
	{
	case kMenuType_Inventory:
	{
		if (auto const invMenu = InventoryMenu::Get())
		{
			if (invMenu->itemList.selected)
				itemRef = CreateRefForStackWithoutCopy(g_thePlayer, InventoryMenu::Selection());
		}
		break;
	}
	case kMenuType_Container:
	{
		if (auto const cntMenu = ContainerMenu::GetSingleton())
		{
			if (cntMenu->leftItems.selected || cntMenu->rightItems.selected)
				itemRef = CreateRefForStackWithoutCopy(cntMenu->leftItems.selected ? g_thePlayer : cntMenu->containerRef, ContainerMenu::GetSelection());
		}
		break;
	}
	case kMenuType_Repair:
	{
		if (auto const rprMenu = RepairMenu::Get())
		{
			if (rprMenu->repairItems.selected)
				itemRef = CreateRefForStackWithoutCopy(g_thePlayer, rprMenu->repairItems.GetSelected());
		}
		break;
	}
	case kMenuType_Barter:
	{
		if (auto const brtMenu = BarterMenu::Get())
		{
			if (brtMenu->leftItems.selected || brtMenu->rightItems.selected)
				itemRef = CreateRefForStackWithoutCopy(brtMenu->leftItems.selected ? g_thePlayer : brtMenu->merchantRef->GetMerchantContainer(), BarterMenu::Selection());
		}
		break;
	}
	case kMenuType_RepairServices:
	{
		if (auto const rpsMenu = RepairServicesMenu::Get())
		{
			if (rpsMenu->itemList.selected)
				itemRef = CreateRefForStackWithoutCopy(g_thePlayer, rpsMenu->itemList.GetSelected());
		}
		break;
	}
	case kMenuType_ItemMod:
	{
		if (auto const modMenu = ItemModMenu::Get())
		{
			if (modMenu->itemModList.selected)
				itemRef = CreateRefForStackWithoutCopy(g_thePlayer, modMenu->itemModList.GetSelected());
		}
		break;
	}
	default:
		return true;
	}
	if (itemRef)
		REFR_RES = itemRef->refID;
	return true;
}







DEFINE_CMD_COND_PLUGIN(GetCalculatedItemWeight, "", false, kParams_OneOptionalObject);

bool tList_IsEmpty_ReturnFalse_Hook(tList<void*>* thisList, void* edx)
{
	return false;
}

bool Cmd_GetCalculatedItemWeight_Eval(COMMAND_ARGS_EVAL)
{
	*result = -1;

	//todo: Check 0x57728A, call GetInventoryWeight with a pseudo ExtraContainerChanges::Data (?).
	if (thisObj)
	{
		if (auto const invRef = InventoryRefGetForID(thisObj->refID))
		{
			if (auto contChangesData = ExtraContainerChanges::Data::Create(invRef->containerRef))
			{
				contChangesData->objList->Append(invRef->data.entry);

				// feeble attempt to fix potential multithreading issues, since code is being overwritten here.
				ScopedLock lockCodeOverwrites(g_Lock);
				
				// Skip code which adds up the weight for each item in the owner / container.
				WriteRelCall(0x4D09CB, (UInt32)tList_IsEmpty_ReturnFalse_Hook);
				
				//Via supreme jank, call GetInventoryWeight.
				//Has to be done, since there is no other function that can return the modified weight,
				//and writing a new one could get invalidated by hooks/changes from other plugins.
				auto itemWeight = ThisStdCall<double>(0x4D0900, contChangesData, g_thePlayer->isHardcore);

				// Undo previous code change.
				WriteRelCall(0x4D09CB, 0x8256D0);


				*result = itemWeight;

				//todo: implement
				//contChangesData->Destroy();
			}
		}
	}
	else if (auto const baseItem = (TESForm*)arg1)
	{
		*result = baseItem->GetModifiedWeight(g_thePlayer->isHardcore);
	}

	return true;
}


bool Cmd_GetCalculatedItemWeight_Execute(COMMAND_ARGS)
{
	*result = -1;
	TESForm* baseItem;
	if (!ExtractArgs(EXTRACT_ARGS, &baseItem))
		return true;

	return Cmd_GetCalculatedItemWeight_Eval(thisObj, baseItem, nullptr, result);
}



DEFINE_COMMAND_PLUGIN(GetCalculatedItemHealth, "", false, kParams_OneOptionalForm_OneOptionalInt);
bool Cmd_GetCalculatedItemHealth_Execute(COMMAND_ARGS)
{
	*result = -1;
	TESForm* form = nullptr;
	UINT32 returnAsPercent = false;
	if (thisObj && ExtractArgs(EXTRACT_ARGS, &form, &returnAsPercent))
	{
		//todo: wtf do I do
		ContChangesEntry tempEntry(NULL, 1, thisObj->baseForm);  //copying after GetCalculatedWeaponDamage from JIP, and c6.
		ExtraContainerChanges::ExtendDataList extendData;
		extendData.Init(&thisObj->extraDataList);
		tempEntry.extendData = &extendData;

		//double __thiscall ContChangesEntry::GetHealthPerc(ContChangesEntry * this, int a2)
		*result = ThisStdCall<double>(0x4BCDB0, &tempEntry, returnAsPercent != 0);
	}
	return true;
}


#endif

//--------------------------------------------------------------------------------
// 
// Filename    : PacketFactoryManager.cpp 
// Written By  : Reiot
// 
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "PacketAssert.h"
#include "PacketFactoryManager.h"
#include "StringStream.h"

#include "Cpackets/CGAttack.h"
#include "Cpackets/CGAddGearToMouse.h"
#include "Cpackets/CGAddInventoryToMouse.h"
#include "Cpackets/CGAddMouseToGear.h"
#include "Cpackets/CGAddMouseToInventory.h"
#include "Cpackets/CGAddMouseToQuickSlot.h"
#include "Cpackets/CGAddMouseToZone.h"
#include "Cpackets/CGAddQuickSlotToMouse.h"
#include "Cpackets/CGAddZoneToMouse.h"
#include "Cpackets/CGAddZoneToInventory.h"
#include "Cpackets/CGBloodDrain.h"
#include "Cpackets/CGUnburrow.h"
#include "Cpackets/CGUntransform.h"
#include "Cpackets/CGCastingSkill.h"
#include "Cpackets/CGConnect.h"
#include "Cpackets/CGDissectionCorpse.h"
#include "Cpackets/CGDropMoney.h"
#include "Cpackets/CGGetOffMotorCycle.h"
#include "Cpackets/CGGlobalChat.h"
#include "Cpackets/CGLearnSkill.h"
#include "Cpackets/CGLogout.h"
#include "Cpackets/CGMakeItem.h"
#include "Cpackets/CGMove.h"
#include "Cpackets/CGNPCAskAnswer.h"
#include "Cpackets/CGNPCTalk.h"
#include "Cpackets/CGPickupMoney.h"
#include "Cpackets/CGReady.h"
#include "Cpackets/CGReloadFromInventory.h"
#include "Cpackets/CGReloadFromQuickSlot.h"
#include "Cpackets/CGRideMotorCycle.h"
#include "Cpackets/CGSay.h"
// Exchange System Packets
#include "Cpackets/CGExchangeList.h"
#include "Cpackets/CGExchangeBuy.h"
#include "Gpackets/GCExchangeBuy.h"
#include "Gpackets/GCExchangeList.h"
#include "Cpackets/CGShopRequestBuy.h"
#include "Cpackets/CGShopRequestList.h"
#include "Cpackets/CGShopRequestSell.h"
#include "Cpackets/CGSkillToInventory.h"
#include "Cpackets/CGThrowBomb.h"
#include "Gpackets/GCThrowBombOK1.h"
#include "Gpackets/GCThrowBombOK2.h"
#include "Gpackets/GCThrowBombOK3.h"
#include "Cpackets/CGThrowItem.h"
#include "Cpackets/CGUseBonusPoint.h"
#include "Cpackets/CGUsePotionFromInventory.h"
#include "Cpackets/CGUsePotionFromQuickSlot.h"
#include "Cpackets/CGRequestRepair.h"
#include "Cpackets/CGVisible.h"
#include "Cpackets/CGVerifyTime.h"
#include "Cpackets/CGRequestNewbieItem.h"
#include "Cpackets/CGWithdrawTax.h"
#include "Cpackets/CGTypeStringList.h"
#include "Cpackets/CGUseItemFromGear.h"

#include "Cpackets/CLCreatePC.h"
#include "Cpackets/CLDeletePC.h"
#include "Cpackets/CLGetPCList.h"
#include "Cpackets/CLLogin.h"
#include "Cpackets/CLLogout.h"
#include "Cpackets/CLQueryPlayerID.h"
#include "Cpackets/CLQueryCharacterName.h"
#include "Cpackets/CLRegisterPlayer.h"
#include "Cpackets/CLSelectPC.h"
#include "Cpackets/CLVersionCheck.h"
#include "Cpackets/CLGetServerList.h"
#include "Cpackets/CLChangeServer.h"
#include "Cpackets/CLReconnectLogin.h"
#include "Cpackets/CLGetWorldList.h"
#include "Cpackets/CLSelectWorld.h"
#include "Cpackets/CLSelectServer.h"


#include "Gpackets/GCAddBat.h"
#include "Gpackets/GCAddBurrowingCreature.h"
#include "Gpackets/GCAddEffect.h"
#include "Gpackets/GCAddEffectToTile.h"
#include "Gpackets/GCAddGearToInventory.h"
#include "Gpackets/GCAddGearToZone.h"
#include "Gpackets/GCAddInstalledMineToZone.h"
#include "Gpackets/GCAddMonster.h"
#include "Gpackets/GCAddMonsterCorpse.h"
#include "Gpackets/GCAddMonsterFromBurrowing.h"
#include "Gpackets/GCAddMonsterFromTransformation.h"
#include "Gpackets/GCAddNewItemToZone.h"
#include "Gpackets/GCAddNPC.h"
#include "Gpackets/GCAddSlayer.h"
#include "Gpackets/GCAddSlayerCorpse.h"
#include "Gpackets/GCAddVampire.h"
#include "Gpackets/GCAddVampireCorpse.h"
#include "Gpackets/GCAddVampireFromBurrowing.h"
#include "Gpackets/GCAddVampireFromTransformation.h"
#include "Gpackets/GCAddWolf.h"
#include "Gpackets/GCAddInjuriousCreature.h"
#include "Gpackets/GCAttackArmsOK1.h"
#include "Gpackets/GCAttackArmsOK2.h"
#include "Gpackets/GCAttackArmsOK3.h"
#include "Gpackets/GCAttackArmsOK4.h"
#include "Gpackets/GCAttackArmsOK5.h"
#include "Gpackets/GCAttackMeleeOK1.h"
#include "Gpackets/GCAttackMeleeOK2.h"
#include "Gpackets/GCAttackMeleeOK3.h"
#include "Gpackets/GCAttack.h"
#include "Gpackets/GCBloodDrainOK1.h"
#include "Gpackets/GCBloodDrainOK2.h"
#include "Gpackets/GCBloodDrainOK3.h"
#include "Gpackets/GCCannotAdd.h"
#include "Gpackets/GCCannotUse.h"
#include "Gpackets/GCCastingSkill.h"
#include "Gpackets/GCCreateItem.h"
#include "Gpackets/GCCreatureDied.h"
#include "Gpackets/GCChangeShape.h"
#include "Gpackets/GCChangeDarkLight.h"
#include "Gpackets/GCChangeWeather.h"
#include "Gpackets/GCCrossCounterOK1.h"
#include "Gpackets/GCCrossCounterOK2.h"
#include "Gpackets/GCCrossCounterOK3.h"
#include "Gpackets/GCDeleteInventoryItem.h"
#include "Gpackets/GCDeleteObject.h"
#include "Gpackets/GCRemoveCorpseHead.h"
#include "Gpackets/GCRemoveInjuriousCreature.h"
#include "Gpackets/GCDeleteandPickUpOK.h"
#include "Gpackets/GCDeleteEffectFromTile.h"
#include "Gpackets/GCDisconnect.h"
#include "Gpackets/GCDropItemToZone.h"
#include "Gpackets/GCFastMove.h"
#include "Gpackets/GCFakeMove.h"
#include "Gpackets/GCGetDamage.h"
#include "Gpackets/GCGetOffMotorCycle.h"
#include "Gpackets/GCGetOffMotorCycleFailed.h"
#include "Gpackets/GCGetOffMotorCycleOK.h"
#include "Gpackets/GCGlobalChat.h"

#include "Gpackets/GCHPRecoveryStartToSelf.h"
#include "Gpackets/GCHPRecoveryStartToOthers.h"
#include "Gpackets/GCHPRecoveryEndToSelf.h"
#include "Gpackets/GCHPRecoveryEndToOthers.h"

#include "Gpackets/GCKnockBack.h"
#include "Gpackets/GCKnocksTargetBackOK1.h"
#include "Gpackets/GCKnocksTargetBackOK2.h"
#include "Gpackets/GCKnocksTargetBackOK4.h"
#include "Gpackets/GCKnocksTargetBackOK5.h"
#include "Gpackets/GCLearnSkillFailed.h"
#include "Gpackets/GCLearnSkillOK.h"
#include "Gpackets/GCLearnSkillReady.h"
#include "Gpackets/GCLightning.h"
#include "Gpackets/GCMakeItemOK.h"
#include "Gpackets/GCMakeItemFail.h"
#include "Gpackets/GCMineExplosionOK1.h"
#include "Gpackets/GCMineExplosionOK2.h"
#include "Gpackets/GCModifyInformation.h"
#include "Gpackets/GCMorph1.h"
#include "Gpackets/GCMorphSlayer2.h"
#include "Gpackets/GCMorphVampire2.h"
#include "Gpackets/GCMove.h"
#include "Gpackets/GCMoveOK.h"
#include "Gpackets/GCMoveError.h"

#include "Gpackets/GCMPRecoveryStart.h"
#include "Gpackets/GCMPRecoveryEnd.h"

#include "Gpackets/GCNPCAsk.h"
#include "Gpackets/GCNPCSay.h"
#include "Gpackets/GCRealWearingInfo.h"
#include "Gpackets/GCReconnect.h"
#include "Gpackets/GCReconnectLogin.h"
#include "Gpackets/GCRemoveEffect.h"
#include "Gpackets/GCRemoveFromGear.h"
#include "Gpackets/GCRideMotorCycle.h"
#include "Gpackets/GCRideMotorCycleFailed.h"
#include "Gpackets/GCRideMotorCycleOK.h"
#include "Gpackets/GCReloadOK.h"
#include "Gpackets/GCSay.h"
#include "Gpackets/GCSearchMotorcycleOK.h"
#include "Gpackets/GCSearchMotorcycleFail.h"
#include "Gpackets/GCSetPosition.h"
// Shop Interface
#include "Gpackets/GCShopBuyOK.h"
#include "Gpackets/GCShopBuyFail.h"
#include "Gpackets/GCShopList.h"
#include "Gpackets/GCShopListMysterious.h"
#include "Gpackets/GCShopSold.h"
#include "Gpackets/GCShopVersion.h"
#include "Gpackets/GCShopBought.h"
#include "Gpackets/GCShopSellOK.h"
#include "Gpackets/GCShopSellFail.h"
#include "Gpackets/GCShopMarketCondition.h"

#include "Gpackets/GCSkillFailed1.h"
#include "Gpackets/GCSkillFailed2.h"
#include "Gpackets/GCSkillToInventoryOK1.h"
#include "Gpackets/GCSkillToInventoryOK2.h"
#include "Gpackets/GCSkillToObjectOK1.h"
#include "Gpackets/GCSkillToObjectOK2.h"
#include "Gpackets/GCSkillToObjectOK3.h"
#include "Gpackets/GCSkillToObjectOK4.h"
#include "Gpackets/GCSkillToObjectOK5.h"
#include "Gpackets/GCSkillToObjectOK6.h"
#include "Gpackets/GCSkillToSelfOK1.h"
#include "Gpackets/GCSkillToSelfOK2.h"
#include "Gpackets/GCSkillToSelfOK3.h"
#include "Gpackets/GCSkillToTileOK1.h"
#include "Gpackets/GCSkillToTileOK2.h"
#include "Gpackets/GCSkillToTileOK3.h"
#include "Gpackets/GCSkillToTileOK4.h"
#include "Gpackets/GCSkillToTileOK5.h"
#include "Gpackets/GCSkillToTileOK6.h"
#include "Gpackets/GCSystemMessage.h"
#include "Gpackets/GCUnburrowOK.h"
#include "Gpackets/GCUnburrowFail.h"
#include "Gpackets/GCUntransformOK.h"
#include "Gpackets/GCUntransformFail.h"
#include "Gpackets/GCTeachSkillInfo.h"
#include "Gpackets/GCUpdateInfo.h"
#include "Gpackets/GCUseBonusPointFail.h"
#include "Gpackets/GCUseBonusPointOK.h"
#include "Gpackets/GCUseOK.h"
#include "Gpackets/GCTakeOff.h"
#include "Gpackets/GCThrowItemOK1.h"
#include "Gpackets/GCThrowItemOK2.h"
#include "Gpackets/GCThrowItemOK3.h"
#include "Gpackets/GCVisibleOK.h"
#include "Gpackets/GCVisibleFail.h"
#include "Gpackets/GCNPCInfo.h"
#include "Gpackets/GCNPCAskVariable.h"
#include "Gpackets/GCWarScheduleList.h"
#include "Gpackets/GCWarList.h"
#include "Gpackets/GCShowUnionInfo.h"

#include "Lpackets/LCCreatePCError.h"
#include "Lpackets/LCCreatePCOK.h"
#include "Lpackets/LCDeletePCError.h"
#include "Lpackets/LCDeletePCOK.h"
#include "Lpackets/LCLoginError.h"
#include "Lpackets/LCLoginOK.h"
#include "Lpackets/LCPCList.h"
#include "Lpackets/LCQueryResultPlayerID.h"
#include "Lpackets/LCQueryResultCharacterName.h"
#include "Lpackets/LCReconnect.h"
#include "Lpackets/LCRegisterPlayerError.h"
#include "Lpackets/LCRegisterPlayerOK.h"
#include "Lpackets/LCSelectPCError.h"

#include "Lpackets/LCVersionCheckOK.h"
#include "Lpackets/LCVersionCheckError.h"
#include "Lpackets/LCServerList.h"
#include "Lpackets/LCWorldList.h"

// added by elcastle 2000-11-29
//#include "Cpackets/CGDialUp.h"
//#include "Cpackets/CGPhoneDisconnect.h"
//#include "Cpackets/CGPhoneSay.h"
#include "Cpackets/CGWhisper.h"

//#include "Gpackets/GCPhoneConnected.h"
//#include "Gpackets/GCRing.h"
//#include "Gpackets/GCPhoneDisconnected.h"
//#include "Gpackets/GCPhoneConnectionFailed.h"
//#include "Gpackets/GCPhoneSay.h"
#include "Gpackets/GCWhisper.h"
#include "Gpackets/GCWhisperFailed.h"

// added by elca 2000-11-29
#include "Gpackets/GCSkillInfo.h"

// added by elca 2000-12-09
#include "Gpackets/GCStatusCurrentHP.h"

// added by elca 2000-12-09
#include "Cpackets/CGSetSlayerHotKey.h"
#include "Cpackets/CGSetVampireHotKey.h"


// added by elca 2001-06-26
#include "Cpackets/CGSelectPortal.h"

// 2001-01-08 김성민
#include "Cpackets/CGMouseToStash.h"
#include "Cpackets/CGStashToMouse.h"
#include "Cpackets/CGStashList.h"
#include "Gpackets/GCStashList.h"
#include "Cpackets/CGStashDeposit.h"
#include "Cpackets/CGStashWithdraw.h"
#include "Cpackets/CGStashRequestBuy.h"
#include "Gpackets/GCStashSell.h"

#include "Cpackets/CGTradePrepare.h"
#include "Cpackets/CGTradeAddItem.h"
#include "Cpackets/CGTradeRemoveItem.h"
#include "Cpackets/CGTradeMoney.h"
#include "Cpackets/CGTradeFinish.h"
#include "Gpackets/GCTradePrepare.h"
#include "Gpackets/GCTradeAddItem.h"
#include "Gpackets/GCTradeRemoveItem.h"
#include "Gpackets/GCTradeMoney.h"
#include "Gpackets/GCTradeFinish.h"
#include "Gpackets/GCTradeError.h"
#include "Gpackets/GCTradeVerify.h"

#include "Cpackets/CGSkillToObject.h"
#include "Cpackets/CGSkillToSelf.h"
#include "Cpackets/CGSkillToTile.h"

#include "Cpackets/CGSilverCoating.h"
#include "Gpackets/GCNPCResponse.h"

#include "Gpackets/GCNPCSayDynamic.h"
#include "Gpackets/GCNPCAskDynamic.h"

#include "Cpackets/CGUseItemFromInventory.h"
#include "Cpackets/CGSelectWayPoint.h"
#include "Gpackets/GCAddHelicopter.h"
#include "Gpackets/GCAddVampirePortal.h"
#include "Gpackets/GCEnterVampirePortal.h"

#include "Cpackets/CGSelectTileEffect.h"

#include "Cpackets/CGPartyInvite.h"
#include "Cpackets/CGPartyLeave.h"

#include "Gpackets/GCPartyInvite.h"
#include "Gpackets/GCPartyLeave.h"
#include "Gpackets/GCPartyJoined.h"
#include "Gpackets/GCPartyError.h"

#include "Cpackets/CGResurrect.h"

#include "Cpackets/CGRequestIP.h"
#include "Gpackets/GCRequestedIP.h"

#include "Gpackets/GCRequestFailed.h"

#include "Gpackets/GCOtherModifyInfo.h"


	#include "Rpackets/CRConnect.h"
	#include "Rpackets/RCConnectVerify.h"
	#include "Rpackets/CRRequest.h"
	#include "Rpackets/RCPositionInfo.h"
	#include "Rpackets/CRDisconnect.h"
	#include "Rpackets/RCSay.h"
	#include "Rpackets/RCStatusHP.h"
	#include "Rpackets/CRWhisper.h"
	#include "Rpackets/RCRequestVerify.h"
	#include "Rpackets/RCRequestedFile.h"
	#include "Rpackets/RCCharacterInfo.h"

// 길드관련 2002. 05. 31. bezz
//#include "Gpackets/GCShowGuildRegist.h"
#include "Cpackets/CGRegistGuild.h"
#include "Gpackets/GCWaitGuildList.h"
#include "Gpackets/GCShowWaitGuildInfo.h"
#include "Gpackets/GCShowGuildInfo.h"
#include "Cpackets/CGSelectGuild.h"
#include "Cpackets/CGTryJoinGuild.h"
#include "Gpackets/GCShowGuildJoin.h"
#include "Cpackets/CGJoinGuild.h"
#include "Gpackets/GCShowMessageBox.h"
//#include "Gpackets/GCModifyMoney.h"
//#include "Cpackets/CGQuitGuild.h"
#include "Cpackets/CGRequestGuildMemberList.h"
#include "Cpackets/CGSelectGuildMember.h"
//#include "Cpackets/CGExpelGuildMember.h"
#include "Cpackets/CGModifyGuildMember.h"
#include "Gpackets/GCActiveGuildList.h"
#include "Gpackets/GCShowGuildMemberInfo.h"
#include "Gpackets/GCGuildMemberList.h"
#include "Gpackets/GCModifyGuildMemberInfo.h"

#include "Cpackets/CGRelicToObject.h"

#include "Cpackets/CGPortCheck.h"

#include "Cpackets/CGGuildChat.h"
#include "Gpackets/GCGuildChat.h"

#include "Cpackets/CGAddItemToItem.h"
#include "Gpackets/GCAddItemToItemVerify.h"

#include "Cpackets/CGRequestInfo.h"
#include "Gpackets/GCNoticeEvent.h"

#include "Cpackets/CGModifyGuildIntro.h"
#include "Cpackets/CGModifyGuildMemberIntro.h"

#include "Cpackets/CGSelectRankBonus.h"
#include "Gpackets/GCSelectRankBonusOK.h"
#include "Gpackets/GCSelectRankBonusFailed.h"
#include "Gpackets/GCRankBonusInfo.h"

#include "Cpackets/CGSkillToNamed.h"

#include "Cpackets/CGUseMessageItemFromInventory.h"

#include "Gpackets/GCHolyLandBonusInfo.h"
#include "Gpackets/GCBloodBibleStatus.h"
#include "Gpackets/GCKickMessage.h"
#include "Gpackets/GCTimeLimitItemInfo.h"
#include "Gpackets/GCSelectQuestID.h"
#include "Cpackets/CGSelectQuest.h"
#include "Gpackets/GCQuestStatus.h"
#include "Gpackets/GCMonsterKillQuestInfo.h"
#include "Cpackets/CGLotterySelect.h"
#include "Gpackets/GCNotifyWin.h"
#include "Gpackets/GCGoodsList.h"
#include "Cpackets/CGTakeOutGood.h"
#include "Gpackets/GCTakeOutOK.h"
#include "Gpackets/GCTakeOutFail.h"
#include "Gpackets/GCAddOusters.h"
#include "Cpackets/CGMixItem.h"
#include "Gpackets/GCAddOustersCorpse.h"
#include "Cpackets/CGAbsorbSoul.h"

#include "Cpackets/CGDownSkill.h"
#include "Gpackets/GCDownSkillOK.h"
#include "Gpackets/GCDownSkillFailed.h"

#include "Gpackets/GCMiniGameScores.h"
#include "Cpackets/CGSubmitScore.h"
#include "Cpackets/CGFailQuest.h"
#include "Cpackets/CGAddItemToCodeSheet.h"
#include "Gpackets/GCFlagWarStatus.h"
#include "Gpackets/GCOtherGuildName.h"
#include "Gpackets/GCSweeperBonusInfo.h"

#include "Cpackets/CGSelectRegenZone.h"
#include "Gpackets/GCRegenZoneStatus.h"
#include "Gpackets/GCPetInfo.h"
#include "Cpackets/CGTameMonster.h"
#include "Cpackets/CGPetGamble.h"
#include "Cpackets/CGCrashReport.h"
#include "Gpackets/GCPetUseSkill.h"
#include "Gpackets/GCPartyPosition.h"
#include "Cpackets/CGPartyPosition.h"
#include "Gpackets/GCPartySay.h"
#include "Cpackets/CGPartySay.h"

//#include "Gpackets/GGServerChat.h"

#include "Gpackets/GCPetStashList.h"
#include "Cpackets/CGWithdrawPet.h"
#include "Cpackets/CGDepositPet.h"
#include "Gpackets/GCPetStashVerify.h"
#include "Cpackets/CGSMSSend.h"
#include "Cpackets/CGAddSMSAddress.h"
#include "Cpackets/CGDeleteSMSAddress.h"
#include "Cpackets/CGSMSAddressList.h"
#include "Gpackets/GCSMSAddressList.h"
#include "Gpackets/GCAddressListVerify.h"

#include "Gpackets/GCNicknameList.h"
#include "Gpackets/GCAddNickname.h"
#include "Cpackets/CGModifyNickname.h"
#include "Cpackets/CGSelectNickname.h"
#include "Gpackets/GCNicknameVerify.h"
#include "Gpackets/GCModifyNickname.h"

#include "Gpackets/GCGQuestStatusInfo.h"
#include "Gpackets/GCGQuestStatusModify.h"
#include "Cpackets/CGGQuestAccept.h"
#include "Cpackets/CGGQuestCancel.h"
#include "Gpackets/GCExecuteElement.h"
#include "Gpackets/GCGQuestInventory.h"

#include "Upackets/CURequestLoginMode.h"
#include "Upackets/UCRequestLoginMode.h"

#include "Gpackets/GCAuthKey.h"
#include "Cpackets/CGAuthKey.h"

#include "Cpackets/CGUseItemFromGQuestInventory.h"

#include "Cpackets/CGAcceptUnion.h"
#include "Cpackets/CGAppointSubmaster.h"
#include "Cpackets/CGDenyUnion.h"
#include "Cpackets/CGExpelGuild.h"
#include "Cpackets/CGQuitUnion.h"
#include "Cpackets/CGQuitUnionAccept.h"
#include "Cpackets/CGQuitUnionDeny.h"
#include "Cpackets/CGRequestGuildList.h"
#include "Cpackets/CGRequestUnion.h"
#include "Cpackets/CGRequestUnionInfo.h"
#include "Gpackets/GCGuildResponse.h"
#include "Gpackets/GCUnionOfferList.h"

#include "Gpackets/GCBloodBibleList.h"
#include "Gpackets/GCBloodBibleSignInfo.h"

#include "Gpackets/GCRequestPowerPointResult.h"
#include "Gpackets/GCUsePowerPointResult.h"

#include "Cpackets/CGStoreClose.h"
#include "Cpackets/CGRequestStoreInfo.h"
#include "Cpackets/CGBuyStoreItem.h"
#include "Gpackets/GCMyStoreInfo.h"
#include "Gpackets/GCOtherStoreInfo.h"
#include "Gpackets/GCRemoveStoreItem.h"
#include "Gpackets/GCAddStoreItem.h"
#include "Cpackets/CGDisplayItem.h"
#include "Cpackets/CGUndisplayItem.h"
#include "Cpackets/CGStoreSign.h"
#include "Cpackets/CGStoreOpen.h"
#include "Gpackets/GCSubInventoryInfo.h"

//add by viva 
#include "Cpackets/CGConnectSetKey.h"
#include "Gpackets/GCFriendChatting.h"
//end

//////////////////////////////////////////////////////////////////////
//
// constructor
//
//////////////////////////////////////////////////////////////////////
PacketFactoryManager::PacketFactoryManager () 
: m_Factories(NULL) , m_Size(Packet::PACKET_MAX)
{
	__BEGIN_TRY

	Assert(m_Size > 0);
	
	// 패킷팩토리배열을 생성한다.
	m_Factories = new PacketFactory*[ m_Size ];
	
	// 팩토리에 대한 포인터들을 NULL 로 초기화한다.
	for (int i = 0 ; i < m_Size ; i ++)
		m_Factories[i] = NULL;

	m_Names.assign(m_Size, std::string());

	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
//
// destructor
//
//////////////////////////////////////////////////////////////////////
PacketFactoryManager::~PacketFactoryManager () 
{
		
	Assert(m_Factories != NULL);

	// 각각의 패킷팩토리들을 삭제한다.
	for (int i = 0 ; i < m_Size ; i ++) 
	{
		if (m_Factories[i] != NULL)
		{
			delete m_Factories[i];
			m_Factories[i] = NULL;
		}
	}
	
	// 패킷팩토리배열을 삭제한다.
	if (m_Factories != NULL)
	{
		delete [] m_Factories;
		m_Factories = NULL;
	}
			
}


//////////////////////////////////////////////////////////////////////
//
// 정의된 모든 패킷팩토리들을 여기에 추가한다.
//
//////////////////////////////////////////////////////////////////////
void PacketFactoryManager::init ()
{
	__BEGIN_TRY
		
	addFactory( DE_PACKET_FACTORY(GCAddBat) );
	addFactory( DE_PACKET_FACTORY(GCAddBurrowingCreature) );
	addFactory( DE_PACKET_FACTORY(GCAddEffect) );
	addFactory( DE_PACKET_FACTORY(GCAddEffectToTile) );
	addFactory( DE_PACKET_FACTORY(GCAddGearToInventory) );
	addFactory( DE_PACKET_FACTORY(GCAddGearToZone) );
	addFactory( DE_PACKET_FACTORY(GCAddInstalledMineToZone) );
	addFactory( DE_PACKET_FACTORY(GCAddMonster) );
	addFactory( DE_PACKET_FACTORY(GCAddMonsterCorpse) );
	addFactory( DE_PACKET_FACTORY(GCAddMonsterFromBurrowing) );
	addFactory( DE_PACKET_FACTORY(GCAddMonsterFromTransformation) );
	addFactory( DE_PACKET_FACTORY(GCAddNewItemToZone) );
	addFactory( DE_PACKET_FACTORY(GCAddNPC) );
	addFactory( DE_PACKET_FACTORY(GCAddSlayer) );
	addFactory( DE_PACKET_FACTORY(GCAddSlayerCorpse) );
	addFactory( DE_PACKET_FACTORY(GCAddVampire) );
	addFactory( DE_PACKET_FACTORY(GCAddVampireCorpse) );
	addFactory( DE_PACKET_FACTORY(GCAddVampireFromBurrowing) );
	addFactory( DE_PACKET_FACTORY(GCAddVampireFromTransformation) );
	addFactory( DE_PACKET_FACTORY(GCAddWolf) );
    addFactory( DE_PACKET_FACTORY(GCAttack) );
    addFactory( DE_PACKET_FACTORY(GCAttackArmsOK1) );
    addFactory( DE_PACKET_FACTORY(GCAttackArmsOK2) );
    addFactory( DE_PACKET_FACTORY(GCAttackArmsOK3) );
    addFactory( DE_PACKET_FACTORY(GCAttackArmsOK4) );
    addFactory( DE_PACKET_FACTORY(GCAttackArmsOK5) );
    addFactory( DE_PACKET_FACTORY(GCAttackMeleeOK1) );
    addFactory( DE_PACKET_FACTORY(GCAttackMeleeOK2) );
    addFactory( DE_PACKET_FACTORY(GCAttackMeleeOK3) );
	addFactory( DE_PACKET_FACTORY(GCBloodDrainOK1) );
	addFactory( DE_PACKET_FACTORY(GCBloodDrainOK2) );
	addFactory( DE_PACKET_FACTORY(GCBloodDrainOK3) );
    addFactory( DE_PACKET_FACTORY(GCCastingSkill) );
    addFactory( DE_PACKET_FACTORY(GCCannotAdd) );
    addFactory( DE_PACKET_FACTORY(GCCannotUse) );
    addFactory( DE_PACKET_FACTORY(GCChangeShape) );
    addFactory( DE_PACKET_FACTORY(GCChangeDarkLight) );
    addFactory( DE_PACKET_FACTORY(GCChangeWeather) );
    addFactory( DE_PACKET_FACTORY(GCCreateItem) );
    addFactory( DE_PACKET_FACTORY(GCCreatureDied) );
    addFactory( DE_PACKET_FACTORY(GCCrossCounterOK1) );
    addFactory( DE_PACKET_FACTORY(GCCrossCounterOK2) );
    addFactory( DE_PACKET_FACTORY(GCCrossCounterOK3) );
	addFactory( DE_PACKET_FACTORY(GCDeleteandPickUpOK) );
	addFactory( DE_PACKET_FACTORY(GCDeleteInventoryItem) );
	addFactory( DE_PACKET_FACTORY(GCDeleteObject) );
	addFactory( DE_PACKET_FACTORY(GCRemoveCorpseHead) );
	addFactory( DE_PACKET_FACTORY(GCRemoveInjuriousCreature) );
	addFactory( DE_PACKET_FACTORY(GCDeleteEffectFromTile) );
	addFactory( DE_PACKET_FACTORY(GCDisconnect) );
	addFactory( DE_PACKET_FACTORY(GCDropItemToZone) );
	addFactory( DE_PACKET_FACTORY(GCFastMove) );
	addFactory( DE_PACKET_FACTORY(GCFakeMove) );
	addFactory( DE_PACKET_FACTORY(GCGetDamage) );
	addFactory( DE_PACKET_FACTORY(GCGetOffMotorCycle) );
	addFactory( DE_PACKET_FACTORY(GCGetOffMotorCycleOK) );
	addFactory( DE_PACKET_FACTORY(GCGetOffMotorCycleFailed) );
	addFactory( DE_PACKET_FACTORY(GCGlobalChat) );
	addFactory( DE_PACKET_FACTORY(GCHPRecoveryStartToSelf) );
	addFactory( DE_PACKET_FACTORY(GCHPRecoveryStartToOthers) );
	addFactory( DE_PACKET_FACTORY(GCHPRecoveryEndToSelf) );
	addFactory( DE_PACKET_FACTORY(GCHPRecoveryEndToOthers) );
 	addFactory( DE_PACKET_FACTORY(GCLightning) );
	addFactory( DE_PACKET_FACTORY(GCLearnSkillFailed) );
	addFactory( DE_PACKET_FACTORY(GCLearnSkillOK) );
	addFactory( DE_PACKET_FACTORY(GCLearnSkillReady) );
	addFactory( DE_PACKET_FACTORY(GCKnockBack) );
	addFactory( DE_PACKET_FACTORY(GCKnocksTargetBackOK1) );
	addFactory( DE_PACKET_FACTORY(GCKnocksTargetBackOK2) );
	addFactory( DE_PACKET_FACTORY(GCKnocksTargetBackOK4) );
	addFactory( DE_PACKET_FACTORY(GCKnocksTargetBackOK5) );
	addFactory( DE_PACKET_FACTORY(GCMakeItemOK) );
	addFactory( DE_PACKET_FACTORY(GCMakeItemFail) );
	addFactory( DE_PACKET_FACTORY(GCMineExplosionOK1) );
	addFactory( DE_PACKET_FACTORY(GCMineExplosionOK2) );
	addFactory( DE_PACKET_FACTORY(GCModifyInformation) );
	addFactory( DE_PACKET_FACTORY(GCMorph1) );
	addFactory( DE_PACKET_FACTORY(GCMorphSlayer2) );
	addFactory( DE_PACKET_FACTORY(GCMorphVampire2) );
	addFactory( DE_PACKET_FACTORY(GCMove) );
	addFactory( DE_PACKET_FACTORY(GCMoveError) );
	addFactory( DE_PACKET_FACTORY(GCMoveOK) );
	addFactory( DE_PACKET_FACTORY(GCMPRecoveryStart) );
	addFactory( DE_PACKET_FACTORY(GCMPRecoveryEnd) );
	addFactory( DE_PACKET_FACTORY(GCNPCAsk) );
	addFactory( DE_PACKET_FACTORY(GCNPCSay) );
	addFactory( DE_PACKET_FACTORY(GCRealWearingInfo) );
	addFactory( DE_PACKET_FACTORY(GCReconnect) );
	addFactory( DE_PACKET_FACTORY(GCReconnectLogin) );
	addFactory( DE_PACKET_FACTORY(GCRemoveEffect) );
	addFactory( DE_PACKET_FACTORY(GCRemoveFromGear) );
	addFactory( DE_PACKET_FACTORY(GCRideMotorCycle) );
	addFactory( DE_PACKET_FACTORY(GCRideMotorCycleOK) );
	addFactory( DE_PACKET_FACTORY(GCRideMotorCycleFailed) );
	addFactory( DE_PACKET_FACTORY(GCReloadOK) );
	addFactory( DE_PACKET_FACTORY(GCUnburrowOK) );
	addFactory( DE_PACKET_FACTORY(GCUnburrowFail) );
	addFactory( DE_PACKET_FACTORY(GCUntransformOK) );
	addFactory( DE_PACKET_FACTORY(GCUntransformFail) );
	addFactory( DE_PACKET_FACTORY(GCUseBonusPointFail) );
	addFactory( DE_PACKET_FACTORY(GCUseBonusPointOK) );
	addFactory( DE_PACKET_FACTORY(GCSay) );
	addFactory( DE_PACKET_FACTORY(GCSearchMotorcycleOK) );
	addFactory( DE_PACKET_FACTORY(GCSearchMotorcycleFail) );
	addFactory( DE_PACKET_FACTORY(GCSetPosition) );
	// shop interface
	addFactory( DE_PACKET_FACTORY(GCShopVersion) );
	addFactory( DE_PACKET_FACTORY(GCShopList) );
	addFactory( DE_PACKET_FACTORY(GCShopListMysterious) );
	addFactory( DE_PACKET_FACTORY(GCShopBuyOK) );
	addFactory( DE_PACKET_FACTORY(GCShopBuyFail) );
	addFactory( DE_PACKET_FACTORY(GCShopSold) );
	addFactory( DE_PACKET_FACTORY(GCShopSellFail) );
	addFactory( DE_PACKET_FACTORY(GCShopSellOK) );
	addFactory( DE_PACKET_FACTORY(GCShopBought) );
	addFactory( DE_PACKET_FACTORY(GCShopMarketCondition) );

	addFactory( DE_PACKET_FACTORY(GCSkillFailed1) );
	addFactory( DE_PACKET_FACTORY(GCSkillFailed2) );
	addFactory( DE_PACKET_FACTORY(GCSkillToInventoryOK1) );
	addFactory( DE_PACKET_FACTORY(GCSkillToInventoryOK2) );
	addFactory( DE_PACKET_FACTORY(GCSkillToObjectOK1) );
	addFactory( DE_PACKET_FACTORY(GCSkillToObjectOK2) );
	addFactory( DE_PACKET_FACTORY(GCSkillToObjectOK3) );
	addFactory( DE_PACKET_FACTORY(GCSkillToObjectOK4) );
	addFactory( DE_PACKET_FACTORY(GCSkillToObjectOK5) );
	addFactory( DE_PACKET_FACTORY(GCSkillToObjectOK6) );
	addFactory( DE_PACKET_FACTORY(GCSkillToSelfOK1) );
	addFactory( DE_PACKET_FACTORY(GCSkillToSelfOK2) );
	addFactory( DE_PACKET_FACTORY(GCSkillToSelfOK3) );
	addFactory( DE_PACKET_FACTORY(GCSkillToTileOK1) );
	addFactory( DE_PACKET_FACTORY(GCSkillToTileOK2) );
	addFactory( DE_PACKET_FACTORY(GCSkillToTileOK3) );
	addFactory( DE_PACKET_FACTORY(GCSkillToTileOK4) );
	addFactory( DE_PACKET_FACTORY(GCSkillToTileOK5) );
	addFactory( DE_PACKET_FACTORY(GCSkillToTileOK6) );
	addFactory( DE_PACKET_FACTORY(GCSystemMessage) );
	addFactory( DE_PACKET_FACTORY(GCTakeOff) );
	addFactory( DE_PACKET_FACTORY(GCTeachSkillInfo) );
	addFactory( DE_PACKET_FACTORY(GCThrowItemOK1) );
	addFactory( DE_PACKET_FACTORY(GCThrowItemOK2) );
	addFactory( DE_PACKET_FACTORY(GCThrowItemOK3) );
	addFactory( DE_PACKET_FACTORY(GCUpdateInfo) );
	addFactory( DE_PACKET_FACTORY(GCUseOK) );

	// Add by elcastle 2000-11-29
//	addFactory(new GCPhoneConnectedFactory());
//	addFactory(new GCRingFactory());
//	addFactory(new GCPhoneDisconnectedFactory());
//	addFactory(new GCPhoneConnectionFailedFactory());
//	addFactory(new GCPhoneSayFactory());
	addFactory( DE_PACKET_FACTORY(GCWhisper) );
	addFactory( DE_PACKET_FACTORY(GCWhisperFailed) );

	// Add by elcastle 2000-12-2
	addFactory( DE_PACKET_FACTORY(GCSkillInfo) );

	// Add by elcastle 2000-12-9
	addFactory( DE_PACKET_FACTORY(GCStatusCurrentHP) );

    addFactory( DE_PACKET_FACTORY(GCStashList) );
    addFactory( DE_PACKET_FACTORY(GCStashSell) );

	addFactory( DE_PACKET_FACTORY(GCTradePrepare) );
	addFactory( DE_PACKET_FACTORY(GCTradeAddItem) );
	addFactory( DE_PACKET_FACTORY(GCTradeRemoveItem) );
	addFactory( DE_PACKET_FACTORY(GCTradeMoney) );
	addFactory( DE_PACKET_FACTORY(GCTradeFinish) );
	addFactory( DE_PACKET_FACTORY(GCTradeError) );
	addFactory( DE_PACKET_FACTORY(GCTradeVerify) );
	addFactory( DE_PACKET_FACTORY(GCVisibleOK) );
	addFactory( DE_PACKET_FACTORY(GCVisibleFail) );

	addFactory( DE_PACKET_FACTORY(GCNPCResponse) );

	addFactory( DE_PACKET_FACTORY(GCNPCSayDynamic) );
	addFactory( DE_PACKET_FACTORY(GCNPCAskDynamic) );

	addFactory( DE_PACKET_FACTORY(GCAddHelicopter) );
	addFactory( DE_PACKET_FACTORY(GCAddVampirePortal) );
	addFactory( DE_PACKET_FACTORY(GCAddInjuriousCreature) );
	addFactory( DE_PACKET_FACTORY(GCEnterVampirePortal) );

	addFactory( DE_PACKET_FACTORY(GCPartyInvite) );
	addFactory( DE_PACKET_FACTORY(GCPartyLeave) );
	addFactory( DE_PACKET_FACTORY(GCPartyJoined) );
	addFactory( DE_PACKET_FACTORY(GCPartyError) );

	addFactory( DE_PACKET_FACTORY(GCRequestedIP) );
	addFactory( DE_PACKET_FACTORY(GCRequestFailed) );

	addFactory( DE_PACKET_FACTORY(GCOtherModifyInfo) );
	addFactory( DE_PACKET_FACTORY(GCThrowBombOK1) );
	addFactory( DE_PACKET_FACTORY(GCThrowBombOK2) );
	addFactory( DE_PACKET_FACTORY(GCThrowBombOK3) );
	
	addFactory( DE_PACKET_FACTORY(LCCreatePCError) );
	addFactory( DE_PACKET_FACTORY(LCCreatePCOK) );
	addFactory( DE_PACKET_FACTORY(LCDeletePCError) );
	addFactory( DE_PACKET_FACTORY(LCDeletePCOK) );
	addFactory( DE_PACKET_FACTORY(LCLoginError) );
	addFactory( DE_PACKET_FACTORY(LCLoginOK) );
	addFactory( DE_PACKET_FACTORY(LCPCList) );
	addFactory( DE_PACKET_FACTORY(LCQueryResultPlayerID) );
	addFactory( DE_PACKET_FACTORY(LCQueryResultCharacterName) );
	addFactory( DE_PACKET_FACTORY(LCReconnect) );
	addFactory( DE_PACKET_FACTORY(LCRegisterPlayerError) );
	addFactory( DE_PACKET_FACTORY(LCRegisterPlayerOK) );
	addFactory( DE_PACKET_FACTORY(LCSelectPCError) );
	addFactory( DE_PACKET_FACTORY(LCVersionCheckOK) );
	addFactory( DE_PACKET_FACTORY(LCVersionCheckError) );
	addFactory( DE_PACKET_FACTORY(LCServerList) );
	addFactory( DE_PACKET_FACTORY(LCWorldList) );

		addFactory( DE_PACKET_FACTORY(CRConnect) );
		addFactory( DE_PACKET_FACTORY(RCConnectVerify) );
		addFactory( DE_PACKET_FACTORY(CRRequest) );
		addFactory( DE_PACKET_FACTORY(RCPositionInfo) );
		addFactory( DE_PACKET_FACTORY(CRDisconnect) );
		addFactory( DE_PACKET_FACTORY(RCSay) );		
		addFactory( DE_PACKET_FACTORY(RCStatusHP) );		
		addFactory( DE_PACKET_FACTORY(CRWhisper) );
		addFactory( DE_PACKET_FACTORY(RCRequestVerify) );
		addFactory( DE_PACKET_FACTORY(RCRequestedFile) );
		addFactory( DE_PACKET_FACTORY(RCCharacterInfo) );

//	addFactory( new GCShowGuildRegistFactory() );
	addFactory( DE_PACKET_FACTORY(GCWaitGuildList) );
	addFactory( DE_PACKET_FACTORY(GCShowGuildInfo) );
	addFactory( DE_PACKET_FACTORY(GCShowGuildJoin) );
	addFactory( DE_PACKET_FACTORY(GCShowMessageBox) );
//	addFactory( new GCModifyMoneyFactory() );
	addFactory( DE_PACKET_FACTORY(GCShowWaitGuildInfo) );
	addFactory( DE_PACKET_FACTORY(GCActiveGuildList) );

	addFactory( DE_PACKET_FACTORY(GCShowGuildMemberInfo) );
	addFactory( DE_PACKET_FACTORY(GCGuildChat) );
	addFactory( DE_PACKET_FACTORY(GCGuildMemberList) );
	addFactory( DE_PACKET_FACTORY(GCModifyGuildMemberInfo) );

	addFactory( DE_PACKET_FACTORY(GCAddItemToItemVerify) );

	// 2002.9.2
	addFactory( DE_PACKET_FACTORY(GCNoticeEvent) );

	addFactory( DE_PACKET_FACTORY(GCSelectRankBonusOK) );
	addFactory( DE_PACKET_FACTORY(GCSelectRankBonusFailed) );
	addFactory( DE_PACKET_FACTORY(GCRankBonusInfo) );

	addFactory( DE_PACKET_FACTORY(GCNPCInfo) );
	addFactory( DE_PACKET_FACTORY(GCNPCAskVariable) );
	addFactory( DE_PACKET_FACTORY(GCWarScheduleList) );
	addFactory( DE_PACKET_FACTORY(GCWarList) );

	addFactory( DE_PACKET_FACTORY(GCHolyLandBonusInfo) );
	addFactory( DE_PACKET_FACTORY(GCBloodBibleStatus) );
	addFactory( DE_PACKET_FACTORY(GCKickMessage) );
	addFactory( DE_PACKET_FACTORY(GCTimeLimitItemInfo) );
	addFactory( DE_PACKET_FACTORY(GCSelectQuestID) );
	addFactory( DE_PACKET_FACTORY(GCQuestStatus) );
	addFactory( DE_PACKET_FACTORY(GCMonsterKillQuestInfo) );
	addFactory( DE_PACKET_FACTORY(GCNotifyWin) );
	addFactory( DE_PACKET_FACTORY(GCGoodsList) );
	addFactory( DE_PACKET_FACTORY(GCTakeOutOK) );
	addFactory( DE_PACKET_FACTORY(GCTakeOutFail) );
	addFactory( DE_PACKET_FACTORY(GCAddOusters) );
	addFactory( DE_PACKET_FACTORY(GCAddOustersCorpse) );
	addFactory( DE_PACKET_FACTORY(GCDownSkillOK) );
	addFactory( DE_PACKET_FACTORY(GCDownSkillFailed) );

	addFactory( DE_PACKET_FACTORY(GCMiniGameScores) );
	addFactory( DE_PACKET_FACTORY(GCFlagWarStatus) );
	addFactory( DE_PACKET_FACTORY(GCOtherGuildName) );
	addFactory( DE_PACKET_FACTORY(GCSweeperBonusInfo) );
	addFactory( DE_PACKET_FACTORY(GCRegenZoneStatus) );
	addFactory( DE_PACKET_FACTORY(GCPetInfo) );
	addFactory( DE_PACKET_FACTORY(GCPetUseSkill) );

	addFactory( DE_PACKET_FACTORY(GCPartyPosition) );
	addFactory( DE_PACKET_FACTORY(GCPartySay) );

//	addFactory( new GGServerChatFactory() );

	addFactory( DE_PACKET_FACTORY(GCPetStashList) );
	addFactory( DE_PACKET_FACTORY(GCPetStashVerify) ); 

	addFactory( DE_PACKET_FACTORY(CGSMSSend) );

	addFactory( DE_PACKET_FACTORY(GCSMSAddressList) );
	addFactory( DE_PACKET_FACTORY(GCAddressListVerify) );

	addFactory( DE_PACKET_FACTORY(GCNicknameList) );
	addFactory( DE_PACKET_FACTORY(GCAddNickname) );

	addFactory( DE_PACKET_FACTORY(GCNicknameVerify) );
	addFactory( DE_PACKET_FACTORY(GCModifyNickname) );

	addFactory( DE_PACKET_FACTORY(GCGQuestStatusInfo) );
	addFactory( DE_PACKET_FACTORY(GCGQuestStatusModify) );

	addFactory( DE_PACKET_FACTORY(GCExecuteElement) );
	addFactory( DE_PACKET_FACTORY(GCGQuestInventory) );

	addFactory( DE_PACKET_FACTORY(CURequestLoginMode) );
	addFactory( DE_PACKET_FACTORY(UCRequestLoginMode) );

	addFactory( DE_PACKET_FACTORY(GCAuthKey) );

	addFactory( DE_PACKET_FACTORY(GCGuildResponse) );
	addFactory( DE_PACKET_FACTORY(GCUnionOfferList) );
	addFactory( DE_PACKET_FACTORY(GCShowUnionInfo) );

	addFactory( DE_PACKET_FACTORY(GCBloodBibleList) );
	addFactory( DE_PACKET_FACTORY(GCBloodBibleSignInfo) );


	// 석민씨 작업
	addFactory( DE_PACKET_FACTORY(CGDisplayItem) );
	addFactory( DE_PACKET_FACTORY(CGUndisplayItem) );
	addFactory( DE_PACKET_FACTORY(CGStoreSign) );
	addFactory( DE_PACKET_FACTORY(CGStoreOpen) );
	addFactory( DE_PACKET_FACTORY(CGStoreClose) );
	addFactory( DE_PACKET_FACTORY(CGRequestStoreInfo) );
	addFactory( DE_PACKET_FACTORY(CGBuyStoreItem) );
	addFactory( DE_PACKET_FACTORY(GCMyStoreInfo) );
	addFactory( DE_PACKET_FACTORY(GCOtherStoreInfo) );
	addFactory( DE_PACKET_FACTORY(GCRemoveStoreItem) );
	addFactory( DE_PACKET_FACTORY(GCAddStoreItem) );
	// 끝
		
	addFactory( DE_PACKET_FACTORY(GCRequestPowerPointResult) );
	addFactory( DE_PACKET_FACTORY(GCUsePowerPointResult) );

	addFactory( DE_PACKET_FACTORY(GCSubInventoryInfo) );
		
	//add by viva
	addFactory( DE_PACKET_FACTORY(CGConnectSetKey) );
	addFactory( DE_PACKET_FACTORY(GCFriendChatting) );
	addFactory( DE_PACKET_FACTORY(GCUseSkillCardOK) );
	//end

	// Exchange System Packets
	addFactory( DE_PACKET_FACTORY(CGExchangeList) );
	addFactory( DE_PACKET_FACTORY(CGExchangeBuy) );
	addFactory( DE_PACKET_FACTORY(GCExchangeBuy) );
	// Was never registered: the server's listing reply was unroutable even
	// though the client had (a stub of) the class. Found during the 1.4
	// exchange-layout reconciliation.
	addFactory( DE_PACKET_FACTORY(GCExchangeList) );

#if defined(__OUTPUT_INIT__) && __OUTPUT_INIT__
	cout << toString() << endl;
#endif
	
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// 팩토리 객체를 특정 인덱스에 추가한다.
//
//////////////////////////////////////////////////////////////////////
void PacketFactoryManager::addFactory (PacketFactory * pFactory, const char * packetName)
{
	__BEGIN_TRY

	if (pFactory == nullptr)
		throw InvalidProtocolException("null packet factory");
	if (packetName == nullptr)
		throw InvalidProtocolException("null packet name");
	const PacketID_t packetID = pFactory->getPacketID();
	if (packetID >= m_Size)
		throw InvalidProtocolException("packet factory id out of range");
	if (m_Factories[packetID] != NULL) {
		StringStream msg;
		msg << "duplicate packet factories, " << packetName;
		throw Error(msg.toString());
	}

	// 패킷팩토리를 등록한다.
	m_Factories[packetID] = pFactory;
	m_Names[packetID] = packetName;

	__END_CATCH
}

void PacketFactoryManager::addFactory (PacketFactory * pFactory)
{
	__BEGIN_TRY

	if (pFactory == nullptr)
		throw InvalidProtocolException("null packet factory");
#ifdef __DEBUG_OUTPUT__
	addFactory(pFactory, pFactory->getPacketName().c_str());
#else
	addFactory(pFactory, "");
#endif

	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
//
// 패킷아이디로 패킷객체를 생성한다.
//
//////////////////////////////////////////////////////////////////////
Packet * PacketFactoryManager::createPacket (PacketID_t packetID) 
{
	__BEGIN_TRY

	// 패킷 아이디가 범위를 넘어섬으로 인해서 Seg.Fault 가 발생하지 않도록.
	// 이런 사용자는 당장 짤라야 한다.
	if (packetID >= m_Size || m_Factories[packetID] == NULL) {
		StringStream msg;
		msg << "packet factory [" << packetID << "] not exist.";
		throw InvalidProtocolException(msg.toString());
	}

	return m_Factories[packetID]->createPacket();
			
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// 패킷아이디로 특정 패킷의 최대 크기를 리턴한다.
//
//////////////////////////////////////////////////////////////////////
PacketSize_t PacketFactoryManager::getPacketMaxSize (PacketID_t packetID) 
{
	__BEGIN_TRY

	// 패킷 아이디가 범위를 넘어섬으로 인해서 Seg.Fault 가 발생하지 않도록.
	// 이런 사용자는 당장 짤라야 한다.
	if (packetID >= m_Size || m_Factories[packetID] == NULL) {
		StringStream msg;
		msg << "invalid packet id (" << packetID << ")";
		throw InvalidProtocolException(msg.toString());
	}

	return m_Factories[packetID]->getPacketMaxSize();
			
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// whether a factory is registered for the id
//
//////////////////////////////////////////////////////////////////////
bool PacketFactoryManager::hasFactory (PacketID_t packetID) const noexcept
{
	return packetID < m_Size && m_Factories[packetID] != NULL;
}


//////////////////////////////////////////////////////////////////////
//
// 패킷아이디로 특정 패킷의 이름을 리턴한다.
//
//////////////////////////////////////////////////////////////////////
string PacketFactoryManager::getPacketName (PacketID_t packetID) const
{
	__BEGIN_TRY

	// 패킷 아이디가 범위를 넘어섬으로 인해서 Seg.Fault 가 발생하지 않도록.
	// 이런 사용자는 당장 짤라야 한다.
	if (packetID >= m_Size || m_Factories[packetID] == NULL) {
		StringStream msg;
		msg << "invalid packet id = (" << packetID << ")";
		throw InvalidProtocolException(msg.toString());
	}

	return m_Names[packetID];

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
string PacketFactoryManager::toString () const
{
	__BEGIN_TRY

	StringStream msg;

	msg << "PacketFactoryManager(\n";

	for (int i = 0 ; i < m_Size ; i ++)
		msg << "PacketFactories[" << i << "] == " << (m_Factories[i] == NULL ? "NULL" : m_Factories[i]->getPacketName()) << "\n";

	msg << ")";

	return msg.toString();

	__END_CATCH
}
#endif

//////////////////////////////////////////////////
// global variable declaration
//////////////////////////////////////////////////

PacketFactoryManager * g_pPacketFactoryManager = NULL;

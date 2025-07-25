/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AW35615_SYFS_HEADER_H
#define AW35615_SYFS_HEADER_H

#include <linux/types.h>

static const char TYPEC_STATE_TBL[][30] = {
	{"Disabled"},
	{"ErrorRecovery"},
	{"Unattached"},
	{"AttachWaitSink"},
	{"AttachedSink"},
	{"AttachWaitSource"},
	{"AttachedSource"},
	{"TrySource"},
	{"TryWaitSink"},
	{"TrySink"},
	{"TryWaitSource"},
	{"AudioAccessory"},
	{"DebugAccessorySource"},
	{"AttachWaitAccessory"},
	{"PoweredAccessory"},
	{"UnsupportedAccessory"},
	{"DelayUnattached"},
	{"UnattachedSource"},
	{"DebugAccessorySink"},
	{"AttachWaitDebSink"},
	{"AttachedDebSink"},
	{"AttachWaitDebSource"},
	{"AttachedDebSource"},
	{"TryDebSource"},
	{"TryWaitDebSink"},
	{"UnattachedDebSource"},
	{"IllegalCable"},
	{"AttachVbusOnlyok"}
};
static const size_t NUM_TYPEC_STATES = ARRAY_SIZE(TYPEC_STATE_TBL);

static const char PE_STATE_TBL[][30] = {
	{"peDisabled"},
	{"peErrorRecovery"},
	{"peSourceHardReset"},
	{"peSourceSendHardReset"},
	{"peSourceSoftReset"},
	{"peSourceSendSoftReset"},
	{"peSourceStartup"},
	{"peSourceSendCaps"},
	{"peSourceDiscovery"},
	{"peSourceDisabled"},
	{"peSourceTransitionDefault"},
	{"peSourceNegotiateCap"},
	{"peSourceCapabilityResponse"},
	{"peSourceWaitNewCapabilities"},
	{"peSourceTransitionSupply"},
	{"peSourceReady"},
	{"peSourceGiveSourceCaps"},
	{"peSourceGetSinkCaps"},
	{"peSourceSendPing"},
	{"peSourceGotoMin"},
	{"peSourceGiveSinkCaps"},
	{"peSourceGetSourceCaps"},
	{"peSourceSendDRSwap"},
	{"peSourceEvaluateDRSwap"},
	{"peSourceAlertReceived"},
	{"peSinkHardReset"},
	{"peSinkSendHardReset"},
	{"peSinkSoftReset"},
	{"peSinkSendSoftReset"},
	{"peSinkTransitionDefault"},
	{"peSinkStartup"},
	{"peSinkDiscovery"},
	{"peSinkWaitCaps"},
	{"peSinkEvaluateCaps"},
	{"peSinkSelectCapability"},
	{"peSinkTransitionSink"},
	{"peSinkReady"},
	{"peSinkGiveSinkCap"},
	{"peSinkGetSourceCap"},
	{"peSinkGetSinkCap"},
	{"peSinkGiveSourceCap"},
	{"peSinkSendDRSwap"},
	{"peSinkAlertReceived"},
	{"peSinkEvaluateDRSwap"},
	{"peSourceSendVCONNSwap"},
	{"peSourceEvaluateVCONNSwap"},
	{"peSinkSendVCONNSwap"},
	{"peSinkEvaluateVCONNSwap"},
	{"peSourceSendPRSwap"},
	{"peSourceEvaluatePRSwap"},
	{"peSinkSendPRSwap"},
	{"peSinkEvaluatePRSwap"},
	{"peGetCountryCodes"},
	{"peGiveCountryCodes"},
	{"peNotSupported"},
	{"peGetPPSStatus"},
	{"peGivePPSStatus"},
	{"peGiveCountryInfo"},
	{"peSourceGiveSourceCapExt"},
	{"peGiveVdm"},
	{"peUfpVdmGetIdentity"},
	{"peUfpVdmSendIdentity"},
	{"peUfpVdmGetSvids"},
	{"peUfpVdmSendSvids"},
	{"peUfpVdmGetModes"},
	{"peUfpVdmSendModes"},
	{"peUfpVdmEvaluateModeEntry"},
	{"peUfpVdmModeEntryNak"},
	{"peUfpVdmModeEntryAck"},
	{"peUfpVdmModeExit"},
	{"peUfpVdmModeExitNak"},
	{"peUfpVdmModeExitAck"},
	{"peUfpVdmAttentionRequest"},
	{"peDfpUfpVdmIdentityRequest"},
	{"peDfpUfpVdmIdentityAcked"},
	{"peDfpUfpVdmIdentityNaked"},
	{"peDfpCblVdmIdentityRequest"},
	{"peDfpCblVdmIdentityAcked"},
	{"peDfpCblVdmIdentityNaked"},
	{"peDfpVdmSvidsRequest"},
	{"peDfpVdmSvidsAcked"},
	{"peDfpVdmSvidsNaked"},
	{"peDfpVdmModesRequest"},
	{"peDfpVdmModesAcked"},
	{"peDfpVdmModesNaked"},
	{"peDfpVdmModeEntryRequest"},
	{"peDfpVdmModeEntryAcked"},
	{"peDfpVdmModeEntryNaked"},
	{"peDfpVdmModeExitRequest"},
	{"peDfpVdmExitModeAcked"},
	{"peSrcVdmIdentityRequest"},
	{"peSrcVdmIdentityAcked"},
	{"peSrcVdmIdentityNaked"},
	{"peDfpVdmAttentionRequest"},
	{"peCblReady"},
	{"peCblGetIdentity"},
	{"peCblGetIdentityNak"},
	{"peCblSendIdentity"},
	{"peCblGetSvids"},
	{"peCblGetSvidsNak"},
	{"peCblSendSvids"},
	{"peCblGetModes"},
	{"peCblGetModesNak"},
	{"peCblSendModes"},
	{"peCblEvaluateModeEntry"},
	{"peCblModeEntryAck"},
	{"peCblModeEntryNak"},
	{"peCblModeExit"},
	{"peCblModeExitAck"},
	{"peCblModeExitNak"},
	{"peDpRequestStatus"},
	{"peDpRequestStatusAck"},
	{"peDpRequestStatusNak"},
	{"peDpRequestConfig"},
	{"peDpRequestConfigAck"},
	{"peDpRequestConfigNak"},
	{"PE_BIST_Receive_Mode"},
	{"PE_BIST_Frame_Received"},
	{"PE_BIST_Carrier_Mode_2"},
	{"PE_BIST_Test_Data"},
	{"dbgGetRxPacket"},
	{"dbgSendTxPacket"},
	{"peSendCableReset"},
	{"peSendGenericCommand"},
	{"peSendGenericData"},
	{"peGiveSourceInfo"},
	{"peGiveRevisonMessage"},
	{"peGetBatteryCap"},
	{"peGetBatteryStatus"},
	{"peGetSinkCapExt"},
	{"peGetManufacturerInfo"}
};
static const size_t NUM_PE_STATES = ARRAY_SIZE(PE_STATE_TBL);

static const char PRL_STATE_TBL[][30] = {
	{"PRLDisabled"},
	{"PRLIdle"},
	{"PRLReset"},
	{"PRLResetWait"},
	{"PRLRxWait"},
	{"PRLTxSendingMessage"},
	{"PRLTxWaitForPHYResponse"},
	{"PRLTxVerifyGoodCRC"},
	{"PRLManualRetries"},
	{"PRL_BIST_Rx_Reset_Counter"},
	{"PRL_BIST_Rx_Test_Frame"},
	{"PRL_BIST_Rx_Error_Count"},
	{"PRL_BIST_Rx_Inform_Policy"}
};
static const size_t NUM_PRL_STATE = ARRAY_SIZE(PRL_STATE_TBL);

static const char CC_TERM_TBL[][15] = {
	{"Open"},
	{"Ra"},
	{"Rdef"},
	{"R1p5"},
	{"R3p0"},
	{"Undefined"}
};
static const size_t NUM_CC_TERMS = ARRAY_SIZE(CC_TERM_TBL);

static const char PORT_TYPE_TBL[][15] = {
	{"sink"},
	{"source"},
	{"drp"},
	{"debug"},
	{"Undefined"}
};
static const char SINK_GET_Rp_TBL[][15] = {
	{"utccNone"},
	{"utccDefault"},
	{"utcc1p5A"},
	{"utcc3p0A"},
	{"utccInvalid"}
};
#endif

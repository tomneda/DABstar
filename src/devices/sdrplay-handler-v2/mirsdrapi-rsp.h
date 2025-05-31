/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

//
// Copyright (c) 2013 Mirics Ltd, All Rights Reserved
//

#ifndef MIR_SDR_H
#define MIR_SDR_H

#ifndef _MIR_SDR_QUALIFIER
#if !defined(STATIC_LIB) && (defined(_M_X64) || defined(_M_IX86)) 
#define _MIR_SDR_QUALIFIER __declspec(dllimport)
#elif defined(STATIC_LIB) || defined(__GNUC__) 
#define _MIR_SDR_QUALIFIER
#endif
#endif  // _MIR_SDR_QUALIFIER

// Application code should check that it is compiled against the same API version
// mir_sdr_ApiVersion() returns the API version 
#define MIR_SDR_API_VERSION   (f32)(2.13)

#if defined(ANDROID) || defined(__ANDROID__)
// Android requires a mechanism to request info from Java application
typedef enum
{
  mir_sdr_GetFd              = 0,
  mir_sdr_FreeFd             = 1,
  mir_sdr_DevNotFound        = 2,
  mir_sdr_DevRemoved         = 3,
  mir_sdr_GetVendorId        = 4,
  mir_sdr_GetProductId       = 5,
  mir_sdr_GetRevId           = 6,
  mir_sdr_GetDeviceId        = 7
} mir_sdr_JavaReqT;

typedef i32 (*mir_sdr_SendJavaReq_t)(mir_sdr_JavaReqT cmd, char *path, char *serNum);
#endif

typedef enum
{
  mir_sdr_Success            = 0,
  mir_sdr_Fail               = 1,
  mir_sdr_InvalidParam       = 2,
  mir_sdr_OutOfRange         = 3,
  mir_sdr_GainUpdateError    = 4,
  mir_sdr_RfUpdateError      = 5,
  mir_sdr_FsUpdateError      = 6,
  mir_sdr_HwError            = 7,
  mir_sdr_AliasingError      = 8,
  mir_sdr_AlreadyInitialised = 9,
  mir_sdr_NotInitialised     = 10,
  mir_sdr_NotEnabled         = 11,
  mir_sdr_HwVerError         = 12,
  mir_sdr_OutOfMemError      = 13,
  mir_sdr_HwRemoved          = 14
} mir_sdr_ErrT;

typedef enum
{
  mir_sdr_BW_Undefined = 0,
  mir_sdr_BW_0_200     = 200,
  mir_sdr_BW_0_300     = 300,
  mir_sdr_BW_0_600     = 600,
  mir_sdr_BW_1_536     = 1536,
  mir_sdr_BW_5_000     = 5000,
  mir_sdr_BW_6_000     = 6000,
  mir_sdr_BW_7_000     = 7000,
  mir_sdr_BW_8_000     = 8000
} mir_sdr_Bw_MHzT;

typedef enum
{
  mir_sdr_IF_Undefined = -1,
  mir_sdr_IF_Zero      = 0,
  mir_sdr_IF_0_450     = 450,
  mir_sdr_IF_1_620     = 1620,
  mir_sdr_IF_2_048     = 2048
} mir_sdr_If_kHzT;

typedef enum
{
  mir_sdr_ISOCH = 0,
  mir_sdr_BULK  = 1
} mir_sdr_TransferModeT;

typedef enum
{
  mir_sdr_CHANGE_NONE    = 0x00,
  mir_sdr_CHANGE_GR      = 0x01,
  mir_sdr_CHANGE_FS_FREQ = 0x02,
  mir_sdr_CHANGE_RF_FREQ = 0x04,
  mir_sdr_CHANGE_BW_TYPE = 0x08,
  mir_sdr_CHANGE_IF_TYPE = 0x10,
  mir_sdr_CHANGE_LO_MODE = 0x20,
  mir_sdr_CHANGE_AM_PORT = 0x40
} mir_sdr_ReasonForReinitT;

typedef enum
{
  mir_sdr_LO_Undefined = 0,
  mir_sdr_LO_Auto      = 1,
  mir_sdr_LO_120MHz    = 2,
  mir_sdr_LO_144MHz    = 3,
  mir_sdr_LO_168MHz    = 4
} mir_sdr_LoModeT;

typedef enum
{
  mir_sdr_BAND_AM_LO   = 0,
  mir_sdr_BAND_AM_MID  = 1,
  mir_sdr_BAND_AM_HI   = 2,
  mir_sdr_BAND_VHF     = 3,
  mir_sdr_BAND_3       = 4,
  mir_sdr_BAND_X       = 5,
  mir_sdr_BAND_4_5     = 6,
  mir_sdr_BAND_L       = 7
} mir_sdr_BandT;

typedef enum
{
  mir_sdr_USE_SET_GR                = 0,
  mir_sdr_USE_SET_GR_ALT_MODE       = 1,
  mir_sdr_USE_RSP_SET_GR            = 2
} mir_sdr_SetGrModeT;

typedef enum
{
  mir_sdr_RSPII_BAND_UNKNOWN = 0,
  mir_sdr_RSPII_BAND_AM_LO   = 1,
  mir_sdr_RSPII_BAND_AM_MID  = 2,
  mir_sdr_RSPII_BAND_AM_HI   = 3,
  mir_sdr_RSPII_BAND_VHF     = 4,
  mir_sdr_RSPII_BAND_3       = 5,
  mir_sdr_RSPII_BAND_X_LO    = 6,
  mir_sdr_RSPII_BAND_X_MID   = 7,
  mir_sdr_RSPII_BAND_X_HI    = 8,
  mir_sdr_RSPII_BAND_4_5     = 9,
  mir_sdr_RSPII_BAND_L       = 10
} mir_sdr_RSPII_BandT;

typedef enum
{
  mir_sdr_RSPII_ANTENNA_A = 5,
  mir_sdr_RSPII_ANTENNA_B = 6
} mir_sdr_RSPII_AntennaSelectT;

typedef enum
{
  mir_sdr_AGC_DISABLE  = 0,
  mir_sdr_AGC_100HZ    = 1,
  mir_sdr_AGC_50HZ     = 2,
  mir_sdr_AGC_5HZ      = 3
} mir_sdr_AgcControlT;

typedef enum
{
  mir_sdr_GAIN_MESSAGE_START_ID  = 0x80000000,
  mir_sdr_ADC_OVERLOAD_DETECTED  = mir_sdr_GAIN_MESSAGE_START_ID + 1,
  mir_sdr_ADC_OVERLOAD_CORRECTED = mir_sdr_GAIN_MESSAGE_START_ID + 2
} mir_sdr_GainMessageIdT;

typedef enum
{
  mir_sdr_EXTENDED_MIN_GR = 0,
  mir_sdr_NORMAL_MIN_GR   = 20
} mir_sdr_MinGainReductionT;

typedef struct 
{
   char *SerNo;
   char *DevNm;
   u8 hwVer;
   u8 devAvail;
} mir_sdr_DeviceT;

typedef struct
{
   f32 curr;
   f32 max;
   f32 min;
} mir_sdr_GainValuesT;

typedef enum
{
   mir_sdr_rspDuo_Tuner_1 = 1,
   mir_sdr_rspDuo_Tuner_2 = 2,
} mir_sdr_rspDuo_TunerSelT;

// mir_sdr_StreamInit() callback function prototypes
typedef void (*mir_sdr_StreamCallback_t)(short *xi, short *xq, u32 firstSampleNum, i32 grChanged, i32 rfChanged, i32 fsChanged, u32 numSamples, u32 reset, u32 hwRemoved, void *cbContext);
typedef void (*mir_sdr_GainChangeCallback_t)(u32 gRdB, u32 lnaGRdB, void *cbContext);

typedef mir_sdr_ErrT (*mir_sdr_Init_t)(i32 gRdB, f64 fsMHz, f64 rfMHz, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, i32 *samplesPerPacket);
typedef mir_sdr_ErrT (*mir_sdr_Uninit_t)();
typedef mir_sdr_ErrT (*mir_sdr_ReadPacket_t)(short *xi, short *xq, u32 *firstSampleNum, i32 *grChanged, i32 *rfChanged, i32 *fsChanged);
typedef mir_sdr_ErrT (*mir_sdr_SetRf_t)(f64 drfHz, i32 abs, i32 syncUpdate);
typedef mir_sdr_ErrT (*mir_sdr_SetFs_t)(f64 dfsHz, i32 abs, i32 syncUpdate, i32 reCal);
typedef mir_sdr_ErrT (*mir_sdr_SetGr_t)(i32 gRdB, i32 abs, i32 syncUpdate);
typedef mir_sdr_ErrT (*mir_sdr_SetGrParams_t)(i32 minimumGr, i32 lnaGrThreshold);
typedef mir_sdr_ErrT (*mir_sdr_SetDcMode_t)(i32 dcCal, i32 speedUp);
typedef mir_sdr_ErrT (*mir_sdr_SetDcTrackTime_t)(i32 trackTime);
typedef mir_sdr_ErrT (*mir_sdr_SetSyncUpdateSampleNum_t)(u32 sampleNum);
typedef mir_sdr_ErrT (*mir_sdr_SetSyncUpdatePeriod_t)(u32 period);
typedef mir_sdr_ErrT (*mir_sdr_ApiVersion_t)(f32 *version);
typedef mir_sdr_ErrT (*mir_sdr_ResetUpdateFlags_t)(i32 resetGainUpdate, i32 resetRfUpdate, i32 resetFsUpdate);
#if defined(ANDROID) || defined(__ANDROID__)
typedef mir_sdr_ErrT (*mir_sdr_SetJavaReqCallback_t)(mir_sdr_SendJavaReq_t sendJavaReq);   
#endif
typedef mir_sdr_ErrT (*mir_sdr_SetTransferMode_t)(mir_sdr_TransferModeT mode);
typedef mir_sdr_ErrT (*mir_sdr_DownConvert_t)(short *in, short *xi, short *xq, u32 samplesPerPacket, mir_sdr_If_kHzT ifType, u32 M, u32 preReset);
typedef mir_sdr_ErrT (*mir_sdr_SetParam_t)(u32 id, u32 value);
typedef mir_sdr_ErrT (*mir_sdr_SetPpm_t)(f64 ppm);
typedef mir_sdr_ErrT (*mir_sdr_SetLoMode_t)(mir_sdr_LoModeT loMode);                   
typedef mir_sdr_ErrT (*mir_sdr_SetGrAltMode_t)(i32 *gRidx, i32 LNAstate, i32 *gRdBsystem, i32 abs, i32 syncUpdate);
typedef mir_sdr_ErrT (*mir_sdr_DCoffsetIQimbalanceControl_t)(u32 DCenable, u32 IQenable);
typedef mir_sdr_ErrT (*mir_sdr_DecimateControl_t)(u32 enable, u32 decimationFactor, u32 wideBandSignal);
typedef mir_sdr_ErrT (*mir_sdr_AgcControl_t)(mir_sdr_AgcControlT enable, i32 setPoint_dBfs, i32 knee_dBfs, u32 decay_ms, u32 hang_ms, i32 syncUpdate, i32 LNAstate);
typedef mir_sdr_ErrT (*mir_sdr_StreamInit_t)(i32 *gRdB, f64 fsMHz, f64 rfMHz, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, i32 LNAstate, i32 *gRdBsystem, mir_sdr_SetGrModeT setGrMode, i32 *samplesPerPacket, mir_sdr_StreamCallback_t StreamCbFn, mir_sdr_GainChangeCallback_t GainChangeCbFn, void *cbContext);
typedef mir_sdr_ErrT (*mir_sdr_StreamUninit_t)(); 
typedef mir_sdr_ErrT (*mir_sdr_Reinit_t)(i32 *gRdB, f64 fsMHz, f64 rfMHz, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, mir_sdr_LoModeT loMode, i32 LNAstate, i32 *gRdBsystem, mir_sdr_SetGrModeT setGrMode, i32 *samplesPerPacket, mir_sdr_ReasonForReinitT reasonForReinit);
typedef mir_sdr_ErrT (*mir_sdr_GetGrByFreq_t)(f64 rfMHz, mir_sdr_BandT *band, i32 *gRdB, i32 LNAstate, i32 *gRdBsystem, mir_sdr_SetGrModeT setGrMode);
typedef mir_sdr_ErrT (*mir_sdr_DebugEnable_t)(u32 enable);
typedef mir_sdr_ErrT (*mir_sdr_GetCurrentGain_t)(mir_sdr_GainValuesT *gainVals);    
typedef mir_sdr_ErrT (*mir_sdr_GainChangeCallbackMessageReceived_t)(); 

typedef mir_sdr_ErrT (*mir_sdr_GetDevices_t)(mir_sdr_DeviceT *devices, u32 *numDevs, u32 maxDevs);
typedef mir_sdr_ErrT (*mir_sdr_SetDeviceIdx_t)(u32 idx);
typedef mir_sdr_ErrT (*mir_sdr_ReleaseDeviceIdx_t)();    
typedef mir_sdr_ErrT (*mir_sdr_GetHwVersion_t)(u8 *ver);
typedef mir_sdr_ErrT (*mir_sdr_RSPII_AntennaControl_t)(mir_sdr_RSPII_AntennaSelectT select); 
typedef mir_sdr_ErrT (*mir_sdr_RSPII_ExternalReferenceControl_t)(u32 output_enable);
typedef mir_sdr_ErrT (*mir_sdr_RSPII_BiasTControl_t)(u32 enable);
typedef mir_sdr_ErrT (*mir_sdr_RSPII_RfNotchEnable_t)(u32 enable);

typedef mir_sdr_ErrT (*mir_sdr_RSP_SetGr_t)(i32 gRdB, i32 LNAstate, i32 abs, i32 syncUpdate);
typedef mir_sdr_ErrT (*mir_sdr_RSP_SetGrLimits_t)(mir_sdr_MinGainReductionT minGr);

typedef mir_sdr_ErrT (*mir_sdr_AmPortSelect_t)(i32 port);

typedef mir_sdr_ErrT (*mir_sdr_rsp1a_BiasT_t)(i32 enable);
typedef mir_sdr_ErrT (*mir_sdr_rsp1a_DabNotch_t)(i32 enable);
typedef mir_sdr_ErrT (*mir_sdr_rsp1a_BroadcastNotch_t)(i32 enable);

typedef mir_sdr_ErrT (*mir_sdr_rspDuo_TunerSel_t)(mir_sdr_rspDuo_TunerSelT sel);
typedef mir_sdr_ErrT (*mir_sdr_rspDuo_ExtRef_t)(i32 enable);
typedef mir_sdr_ErrT (*mir_sdr_rspDuo_BiasT_t)(i32 enable);
typedef mir_sdr_ErrT (*mir_sdr_rspDuo_Tuner1AmNotch_t)(i32 enable);
typedef mir_sdr_ErrT (*mir_sdr_rspDuo_BroadcastNotch_t)(i32 enable);
typedef mir_sdr_ErrT (*mir_sdr_rspDuo_DabNotch_t)(i32 enable);

// API function definitions
#ifdef __cplusplus
extern "C"
{
#endif

   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_Init(i32 gRdB, f64 fsMHz, f64 rfMHz, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, i32 *samplesPerPacket);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_Uninit(void);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_ReadPacket(short *xi, short *xq, u32 *firstSampleNum, i32 *grChanged, i32 *rfChanged, i32 *fsChanged);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetRf(f64 drfHz, i32 abs, i32 syncUpdate);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetFs(f64 dfsHz, i32 abs, i32 syncUpdate, i32 reCal);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetGr(i32 gRdB, i32 abs, i32 syncUpdate);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetGrParams(i32 minimumGr, i32 lnaGrThreshold);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetDcMode(i32 dcCal, i32 speedUp);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetDcTrackTime(i32 trackTime);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetSyncUpdateSampleNum(u32 sampleNum);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetSyncUpdatePeriod(u32 period);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_ApiVersion(f32 *version);    // Called by application to retrieve version of API used to create Dll
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_ResetUpdateFlags(i32 resetGainUpdate, i32 resetRfUpdate, i32 resetFsUpdate);
#if defined(ANDROID) || defined(__ANDROID__)
   // This function provides a machanism for the Java application to set
   // the callback function used to send request to it
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetJavaReqCallback(mir_sdr_SendJavaReq_t sendJavaReq);   
#endif
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetTransferMode(mir_sdr_TransferModeT mode);   
   /*
    * This following function will only operate correctly for the parameters detailed in the table below:
    *
    *      IF freq | Signal BW | Input Sample Rate | Output Sample Rate | Required Decimation Factor 
    *     -------------------------------------------------------------------------------------------
    *       450kHz |   200kHz  |     2000kHz       |       500kHz       |          M=4
    *       450kHz |   300kHz  |     2000kHz       |       500kHz       |          M=4
    *       450kHz |   600kHz  |     2000kHz       |      1000kHz       |          M=2
    *      2048kHz |  1536kHz  |     8192kHz       |      2048kHz       |          M=4
    *
    * If preReset == 1, then the filter state will be reset to 0 before starting the filtering operation.
    */
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_DownConvert(short *in, short *xi, short *xq, u32 samplesPerPacket, mir_sdr_If_kHzT ifType, u32 M, u32 preReset);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetParam(u32 id, u32 value);   // This MAY be called before mir_sdr_Init()

   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetPpm(f64 ppm);                              // This MAY be called before mir_sdr_Init()
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetLoMode(mir_sdr_LoModeT loMode);               // This MUST be called before mir_sdr_Init()/mir_sdr_StreamInit() - otherwise use mir_sdr_Reinit()         
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetGrAltMode(i32 *gRidx, i32 LNAstate, i32 *gRdBsystem, i32 abs, i32 syncUpdate);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_DCoffsetIQimbalanceControl(u32 DCenable, u32 IQenable);
   /*
    * Valid decimation factors for the following function are 2, 4, 8, 16 or 32 only
    * Setting wideBandSignal=1 will use a slower filter but minimise the in-band roll-off
    */
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_DecimateControl(u32 enable, u32 decimationFactor, u32 wideBandSignal);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_AgcControl(mir_sdr_AgcControlT enable, i32 setPoint_dBfs, i32 knee_dBfs, u32 decay_ms, u32 hang_ms, i32 syncUpdate, i32 LNAstate);
   /*
    * mir_sdr_StreamInit() replaces mir_sdr_Init() and sets up a thread (or chain of threads) inside the API which will perform the processing chain (shown below),
    * and then use the callback function to return the data to the calling application/plug-in.
	 * Processing chain (in order):
    * mir_sdr_ReadPacket()
    * DCoffsetCorrection()     - LIF mode - enabled by default
    * DownConvert()            - automatically enabled if the parameters shown for mir_sdr_DownConvert() are selected
    * Decimate()               - disabled by default
    * DCoffsetCorrection()     - ZIF mode - enabled by default
    * IQimbalanceCorrection()  - enabled by default
    * Agc()                    - disabled by default
	 */
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_StreamInit(i32 *gRdB, f64 fsMHz, f64 rfMHz, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, i32 LNAstate, i32 *gRdBsystem, mir_sdr_SetGrModeT setGrMode, i32 *samplesPerPacket, mir_sdr_StreamCallback_t StreamCbFn, mir_sdr_GainChangeCallback_t GainChangeCbFn, void *cbContext);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_StreamUninit(void); 
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_Reinit(i32 *gRdB, f64 fsMHz, f64 rfMHz, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, mir_sdr_LoModeT loMode, i32 LNAstate, i32 *gRdBsystem, mir_sdr_SetGrModeT setGrMode, i32 *samplesPerPacket, mir_sdr_ReasonForReinitT reasonForReinit);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_GetGrByFreq(f64 rfMHz, mir_sdr_BandT *band, i32 *gRdB, i32 LNAstate, i32 *gRdBsystem, mir_sdr_SetGrModeT setGrMode);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_DebugEnable(u32 enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_GetCurrentGain(mir_sdr_GainValuesT *gainVals);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_GainChangeCallbackMessageReceived(void); 

   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_GetDevices(mir_sdr_DeviceT *devices, u32 *numDevs, u32 maxDevs);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_SetDeviceIdx(u32 idx);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_ReleaseDeviceIdx(void);   
   /*
    * Uninit: ver = 0
    * RSPI :  ver = 1
    * RSPII:  ver = 2
    */
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_GetHwVersion(u8 *ver);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_RSPII_AntennaControl(mir_sdr_RSPII_AntennaSelectT select); 
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_RSPII_ExternalReferenceControl(u32 output_enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_RSPII_BiasTControl(u32 enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_RSPII_RfNotchEnable(u32 enable);

   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_RSP_SetGr(i32 gRdB, i32 LNAstate, i32 abs, i32 syncUpdate);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_RSP_SetGrLimits(mir_sdr_MinGainReductionT minGr);

   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_AmPortSelect(i32 port);   // If called after mir_sdr_Init() a call to mir_sdr_Reinit(..., reasonForReinit = mir_sdr_CHANGE_AM_PORT)
                                                                     // is also required to change the port

   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rsp1a_BiasT(i32 enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rsp1a_DabNotch(i32 enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rsp1a_BroadcastNotch(i32 enable);

   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rspDuo_TunerSel(mir_sdr_rspDuo_TunerSelT sel);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rspDuo_ExtRef(i32 enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rspDuo_BiasT(i32 enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rspDuo_Tuner1AmNotch(i32 enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rspDuo_BroadcastNotch(i32 enable);
   _MIR_SDR_QUALIFIER mir_sdr_ErrT mir_sdr_rspDuo_DabNotch(i32 enable);

#ifdef __cplusplus
}
#endif

#endif //MIR_SDR_H

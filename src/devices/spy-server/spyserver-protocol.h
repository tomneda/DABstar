/*

SPY Server protocol structures and constants
Copyright (C) 2017 Youssef Touil youssef@live.com

*/


#ifndef	__SPYSERVER_PROTOCOL__
#define	__SPYSERVER_PROTOCOL__

#include <stdint.h>
#include <limits.h>

#define SPYSERVER_PROTOCOL_VERSION (((2) << 24) | ((0) << 16) | (1700))

#define SPYSERVER_MAX_COMMAND_BODY_SIZE (256)
#define SPYSERVER_MAX_MESSAGE_BODY_SIZE (1 << 20)
#define SPYSERVER_MAX_DISPLAY_PIXELS (1 << 15)
#define SPYSERVER_MIN_DISPLAY_PIXELS (100)
#define SPYSERVER_MAX_FFT_DB_RANGE (150)
#define SPYSERVER_MIN_FFT_DB_RANGE (10)
#define SPYSERVER_MAX_FFT_DB_OFFSET (100)

enum DeviceType
{
  DEVICE_INVALID        = 0,
  DEVICE_AIRSPY_ONE     = 1,
  DEVICE_AIRSPY_HF      = 2,
  DEVICE_RTLSDR         = 3,
};

enum CommandType
{
  CMD_HELLO             = 0,
  CMD_GET_SETTING       = 1,
  CMD_SET_SETTING       = 2,
  CMD_PING              = 3,
};

enum SettingType
{
  SETTING_STREAMING_MODE        = 0,
  SETTING_STREAMING_ENABLED     = 1,
  SETTING_GAIN                  = 2,

  SETTING_IQ_FORMAT             = 100,    // 0x64
  SETTING_IQ_FREQUENCY          = 101,    // 0x65
  SETTING_IQ_DECIMATION         = 102,    // 0x66
  SETTING_IQ_DIGITAL_GAIN       = 103,    // 0x67

  SETTING_FFT_FORMAT            = 200,    // 0xc8
  SETTING_FFT_FREQUENCY         = 201,    // 0xc9
  SETTING_FFT_DECIMATION        = 202,    // 0xca
  SETTING_FFT_DB_OFFSET         = 203,    // 0xcb
  SETTING_FFT_DB_RANGE          = 204,    // 0xcc
  SETTING_FFT_DISPLAY_PIXELS    = 205,    // 0xcd
};

enum StreamType
{
  STREAM_TYPE_STATUS    = 0,
  STREAM_TYPE_IQ        = 1,
  STREAM_TYPE_AF        = 2,
  STREAM_TYPE_FFT       = 4,
};


enum StreamingMode
{
  STREAM_MODE_IQ_ONLY       = STREAM_TYPE_IQ,                     // 0x01
  STREAM_MODE_AF_ONLY       = STREAM_TYPE_AF,                     // 0x02
  STREAM_MODE_FFT_ONLY      = STREAM_TYPE_FFT,                    // 0x04
  STREAM_MODE_FFT_IQ        = STREAM_TYPE_FFT | STREAM_TYPE_IQ,   // 0x05
  STREAM_MODE_FFT_AF        = STREAM_TYPE_FFT | STREAM_TYPE_AF,   // 0x06
};

enum StreamFormat
{
  STREAM_FORMAT_INVALID     = 0,
  STREAM_FORMAT_UINT8       = 1,
  STREAM_FORMAT_INT16       = 2,
  STREAM_FORMAT_INT24       = 3,
  STREAM_FORMAT_FLOAT       = 4,
  STREAM_FORMAT_DINT4       = 5,
};

enum MessageType
{
  MSG_TYPE_DEVICE_INFO      = 0,
  MSG_TYPE_CLIENT_SYNC      = 1,
  MSG_TYPE_PONG             = 2,
  MSG_TYPE_READ_SETTING     = 3,

  MSG_TYPE_UINT8_IQ         = 100,     // 0x64
  MSG_TYPE_INT16_IQ         = 101,     // 0x65
  MSG_TYPE_INT24_IQ         = 102,     // 0x66
  MSG_TYPE_FLOAT_IQ         = 103,     // 0x67

  MSG_TYPE_UINT8_AF         = 200,    // 0xc8
  MSG_TYPE_INT16_AF         = 201,    // 0xc9
  MSG_TYPE_INT24_AF         = 202,    // 0xca
  MSG_TYPE_FLOAT_AF         = 203,    // 0xcb

  MSG_TYPE_DINT4_FFT        = 300,    //0x12C
  MSG_TYPE_UINT8_FFT        = 301,    //0x12D
};

struct ClientHandshake
{
  u32 ProtocolVersion;
  u32 ClientNameLength;
};

struct CommandHeader
{
  u32 CommandType;
  u32 BodySize;
};

struct SettingTarget
{
  u32 StreamType;
  u32 SettingType;
};

struct MessageHeader
{
  u32 ProtocolID;
  u32 MessageType;
  u32 StreamType;
  u32 SequenceNumber;
  u32 BodySize;
};

struct DeviceInfo
{
  u32 DeviceType = 0;
  u32 DeviceSerial = 0;
  u32 MaximumSampleRate = 0;
  u32 MaximumBandwidth = 0;
  u32 DecimationStageCount = 0;
  u32 GainStageCount = 0;
  u32 MaximumGainIndex = 0;
  u32 MinimumFrequency = 0;
  u32 MaximumFrequency = 0;
  u32 Resolution = 0;
  u32 MinimumIQDecimation = 0;
  u32 ForcedIQFormat = 0;
};

struct ClientSync
{
  u32 CanControl;
  u32 Gain;
  u32 DeviceCenterFrequency;
  u32 IQCenterFrequency;
  u32 FFTCenterFrequency;
  u32 MinimumIQCenterFrequency;
  u32 MaximumIQCenterFrequency;
  u32 MinimumFFTCenterFrequency;
  u32 MaximumFFTCenterFrequency;
};

struct ComplexInt16
{
  i16 real;
  i16 imag;
};

struct ComplexUInt8
{
  u8 real;
  u8 imag;
};

enum ParserPhase {
  AcquiringHeader,
  ReadingData
};

#endif

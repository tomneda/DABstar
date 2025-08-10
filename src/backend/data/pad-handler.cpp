/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2015 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pad-handler.h"
#include "dabradio.h"
#include "data_manip_and_checks.h"
#include <QLoggingCategory>

//Q_LOGGING_CATEGORY(sLogPadHandler, "PadHandler", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogPadHandler, "PadHandler", QtWarningMsg)

class ContInd // Contents Indicator
{
public:
  void set_val(const u8 iVal) { mVal = iVal; }
  u8 get_length() const { return cLengthTable[mVal >> 5]; }
  u8 get_appl_type() const { return (mVal & 0x1F); }
private:
  u8 mVal = 0;
  static constexpr std::array<i16, 8> cLengthTable = { 4, 6, 8, 12, 16, 24, 32, 48 };
};

PadHandler::PadHandler(DabRadio * mr)
  : mpRadioInterface(mr)
{
  mpMotObject.reset(new MotObject(mr, false));

  connect(this, &PadHandler::signal_show_label, mr, &DabRadio::slot_show_label);
  connect(this, &PadHandler::signal_show_mot_handling, mr, &DabRadio::slot_show_mot_handling);

  mMscDataGroupBuffer.reserve(1024); // try to avoid future memory swapping
  mDataBuffer.reserve(1024); // try to avoid future memory swapping
  mShortPadData.reserve(16); // try to avoid future memory swapping
}

//	Data is stored reverse, we pass the vector and the index of the
//	last element of the XPad data.
//	 L0 is the "top" byte of the L field, L1 the next to top one.
void PadHandler::process_PAD(const u8 * const iBuffer, const i16 iLast, const u8 iL1, const u8 iL0)
{
  const u8 fpadType = (iL1 >> 6) & 0x3;

  if (fpadType != 0x0)
  {
    // qDebug() << "F-PAD-type" << fpadType << "not supported";
    return;
  }

  const u8 x_padInd = (iL1 >> 4) & 0x3;
  const bool CI_flag = (iL0 & 0x2) != 0;
  // const u8 L_ByteInd = L1 & 0xF;

  switch (x_padInd)
  {
  default:
    // qWarning() << "x_padInd" << x_padInd << "not supported" << "L1" << L1 << "L0" << L0;;
    break;

  case 0x1:
    // qCDebug(sLogPadHandler) << "_handle_short_PAD()" << "buffer" << iBuffer << "last" << iLast << "CI_flag" << CI_flag;
    _handle_short_PAD(iBuffer, iLast, CI_flag);
    break;

  case 0x2:
    // qCDebug(sLogPadHandler) << "_handle_variable_PAD()" << "buffer" << iBuffer << "last" << iLast << "CI_flag" << CI_flag;
    _handle_variable_PAD(iBuffer, iLast, CI_flag);
    break;
  }
}

//	Since the data is stored in reversed order, we pass
//	on the vector address and the offset of the last element
//	in that vector
//
//	shortPad's are 4  byte values. If the CI is on, then type 2
//	indicates the start of a segment. Type 3 the continuation.
//	The start of a message, i.e. segment 0 is (a.o) found by
//	a (1, 0) value of the firstSegment/lastSegment values.
//	The end of a segment might be indicated by a specific pattern
//	of these 2 values, but it is not clear to me how.
//	For me, the end of a segment is when we collected the amount
//	of values specified for the segment.
void PadHandler::_handle_short_PAD(const u8 * const iBuffer, const i16 iLast, const bool iCIFlag)
{
  i16 i;

  if (iCIFlag)
  {
    // has a CI flag
    ContInd CI;
    CI.set_val(iBuffer[iLast]); // Content Indicator
    mFirstSegment = (iBuffer[iLast - 1] & 0x40) != 0;
    mLastSegment  = (iBuffer[iLast - 1] & 0x20) != 0;
    mCharSet = (ECharacterSet)(iBuffer[iLast - 2] & 0x0F);

    if (mFirstSegment)
    {
      mDynamicLabelTextUnConverted.clear();
      _reset_charset_change();
    }

    switch (CI.get_appl_type())
    {
    default: break;

    case 0:  // end marker
      break;

    case 2:  // start of fragment, extract the length
      if (mFirstSegment && !mLastSegment)
      {
        mSegmentNumber = iBuffer[iLast - 2] >> 4;
        if (!mDynamicLabelTextUnConverted.isEmpty())
        {
          const QString dynamicLabelTextConverted = toQStringUsingCharset(mDynamicLabelTextUnConverted, mCharSet);
          emit signal_show_label(dynamicLabelTextConverted);
        }
        mDynamicLabelTextUnConverted.clear();
        _reset_charset_change();
      }
      mStillToGo = iBuffer[iLast - 1] & 0x0F;
      mShortPadData.clear();
      mShortPadData.push_back(iBuffer[iLast - 3]);
      break;

    case 3:  // continuation of fragment
      for (i = 0; (i < 3) && (mStillToGo > 0); i++)
      {
        mStillToGo--;
        mShortPadData.push_back(iBuffer[iLast - 1 - i]);
      }

      if ((mStillToGo <= 0) && (mShortPadData.size() > 1))
      {
        mDynamicLabelTextUnConverted.append((const char *)mShortPadData.data(), (qsizetype)mShortPadData.size());
        _check_charset_change();
        mShortPadData.clear();
      }
      break;
    }
  }
  else
  {  // No CI flag
    //	X-PAD field is all data
    for (i = 0; (i < 4) && (mStillToGo > 0); i++)
    {
      mShortPadData.push_back(iBuffer[iLast - i]);
      mStillToGo--;
    }

    //	at the end of a frame
    if ((mStillToGo <= 0) && (mShortPadData.size() > 0))
    {
      //	just to avoid doubling by unsollicited shortpads
      mDynamicLabelTextUnConverted.append((const char *)mShortPadData.data(), (qsizetype)mShortPadData.size());
      _check_charset_change();
      mShortPadData.clear();
      //	if we are at the end of the last segment (and the text is not empty)
      //	then show it.
      if (!mFirstSegment && mLastSegment)
      {
        if (!mDynamicLabelTextUnConverted.isEmpty())
        {
          const QString dynamicLabelTextConverted = toQStringUsingCharset(mDynamicLabelTextUnConverted, mCharSet);
          emit signal_show_label(dynamicLabelTextConverted);
        }
        mDynamicLabelTextUnConverted.clear();
        _reset_charset_change();
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////
//
//	Here we end up when F_PAD type = 00 and X-PAD Ind = 02
//	Since the data is reversed, we pass on the vector address
//	and the offset of the last element in the vector,
//	i.e. we start (downwards)  beginning at b [last];
void PadHandler::_handle_variable_PAD(const u8 * const iBuffer, const i16 iLast, const bool iCiFlag)
{
  i16 base = iLast;

  //	If an xpadfield shows with a CI_flag == 0, and if we are
  //	dealing with an msc field, the size to be taken is
  //	the size of the latest xpadfield that had a CI_flag != 0
  //fprintf(stderr, "CI_flag=%x, last=%d, ", CI_flag, last);
  if (!iCiFlag)
  {
    //fprintf(stderr,"mscGroupElement=%d, last_appType=%d\n", mscGroupElement, last_appType);
    if (mXPadLength > 0)
    {
      if (iLast < mXPadLength - 1)
      {
        // fprintf(stdout, "handle_variablePAD: last < xpadLength - 1\n");
        return;
      }

      mDataBuffer.resize(mXPadLength);
      for (i16 j = 0; j < mXPadLength; j++)
      {
        mDataBuffer[j] = iBuffer[iLast - j];
      }

      switch (mLastAppType)
      {
      case 2:   // Dynamic label segment, start of X-PAD data group
      case 3:   // Dynamic label segment, continuation of X-PAD data group
        _dynamic_label(mDataBuffer, 3);
        break;

      case 12:   // MOT, start of X-PAD data group
      case 13:   // MOT, continuation of X-PAD data group
        if (mMscGroupElement)
          _add_MSC_element(mDataBuffer);
        break;
      default: ;
      }
    }
    return;
  }
  //
  //	The CI flag in the F_PAD data is set, so we have local CI's
  //	7.4.2.2: Contents indicators are one byte long

  i16 numCI = 0;
  std::array<ContInd, 4> CI_table;

  do
  {
    CI_table[numCI].set_val(iBuffer[base--]);
    // the AppTy == 0 signals that there is no more Contents Indication message -> jump out
    // the base pointer is already decremented above
    if (CI_table[numCI].get_appl_type() == 0) break;
    numCI++;
  }
  while (numCI < 4);

  //	The space for the CI's does belong to the Cpadfield, so
  //	but do not forget to take into account the '0'field if CI_Index < 4
  //if (mscGroupElement)
  {
    mXPadLength = 0;
    for (i16 i = 0; i < numCI; i++)
    {
      mXPadLength += CI_table[i].get_length();
    }
    mXPadLength += numCI == 4 ? 4 : numCI + 1;
  }

  //	Handle the contents
  for (i16 ciIdx = 0; ciIdx < numCI; ciIdx++)
  {
    const u8 appType = CI_table[ciIdx].get_appl_type();
    const i16 length = CI_table[ciIdx].get_length();

    //	collect data, reverse the reversed bytes
    mDataBuffer.resize(length);
    for (i16 j = 0; j < length; j++)
    {
      mDataBuffer[j] = iBuffer[base - j];
    }

    switch (appType)
    {
    case 1:
      if (length == 4 && check_crc_bytes(mDataBuffer.data(), 2)) // the CRC is expected behind the length of 2!
      {
        mDataGroupLength = ((mDataBuffer[0] & 0x3F) << 8) | mDataBuffer[1];
      }
      else
      {
        qCWarning(sLogPadHandler) << "dataGroupLengthField fails on CRC check" << "length=" << length;;
      }
      break;
      
    case 2:   // Dynamic label segment, start of X-PAD data group
    case 3:   // Dynamic label segment, continuation of X-PAD data group
      // qCDebug(sLogPadHandler) << "DL, start of X-PAD data group, size " << data.size() << "appType=" << appType;
      _dynamic_label(mDataBuffer, appType);
      break;

    case 12:   // MOT, start of X-PAD data group
      // qCDebug(sLogPadHandler) << "MOT, start of X-PAD data group, size " << data.size() << "appType=" << appType;
      _new_MSC_element(mDataBuffer);
      break;

    case 13:   // MOT, continuation of X-PAD data group
      // qCDebug(sLogPadHandler) << "MOT, start of X-PAD data group, size " << data.size() << "appType=" << appType;
      _add_MSC_element(mDataBuffer);
      break;

    default: return; // sorry, we do not handle this
    }

    mLastAppType = appType;
    base -= length;

    if (base < -1) // base == 0 would be still the last readable element, so -1 is normal
    {
      qCWarning(sLogPadHandler) << "base < 0, ciIdx=" << ciIdx << ", numCI=" << numCI << "base" << base << "length" << length;
      return;
    }
  }
}

//
//	A dynamic label is created from a sequence of (dynamic) xpad
//	fields, starting with CI = 2, continuing with CI = 3
void PadHandler::_dynamic_label(const std::vector<u8> & data, u8 iApplType)
{
  i16 dataLength = 0;

  if (iApplType == 2)
  {
    // start of segment
    const u16 prefix = (data[0] << 8) | data[1];
    const u8 field_1 = (prefix >> 8) & 017;
    const u8 Cflag   = (prefix >> 12) & 01;
    const u8 first   = (prefix >> 14) & 01;
    const u8 last    = (prefix >> 13) & 01;

    dataLength = data.size() - 2; // The length with header removed

    if (first)
    {
      mSegmentNo = 1;
      mCharSet = (ECharacterSet)((prefix >> 4) & 017);
      mDynamicLabelTextUnConverted.clear();
      _reset_charset_change();
    }
    else
    {
      const i32 test = ((prefix >> 4) & 07) + 1;

      if (test != mSegmentNo + 1)
      {
        // fprintf (stderr, "mismatch %d %d\n", test, segmentno);
        mSegmentNo = -1;
        return;
      }
      mSegmentNo = ((prefix >> 4) & 07) + 1;
      // fprintf (stderr, "segment %d\n", segmentno);
    }

    if (Cflag) // special dynamic label command
    {
      const u16 command = (prefix >> 8) & 0x0f;

      if (command == 1)
      {
        // clear the display
        mDynamicLabelTextUnConverted.clear();
        _reset_charset_change();
        mSegmentNo = -1;
      }
      /*else if (command == 2)
      {
        fprintf(stdout, "DL Plus command\n");
      }
      else
      {
        fprintf(stdout, "Unknown command\n");
      }*/
    }
    else
    {
      // Dynamic text length
      const i16 totalDataLength = field_1 + 1;

      if ((i16)data.size() - 2 < totalDataLength)
      {
        dataLength = (i16)(data.size() - 2); // the length is shortened by header
        mMoreXPad = true;
      }
      else
      {
        dataLength = totalDataLength;  // no more xpad app's 3
        mMoreXPad = false;
      }

      mDynamicLabelTextUnConverted.append((const char *)&data[2], dataLength);
      _check_charset_change();

      //	if at the end, show the label
      if (last)
      {
        if (!mMoreXPad)
        {
          const QString dynamicLabelTextConverted = toQStringUsingCharset(mDynamicLabelTextUnConverted, mCharSet);
          emit signal_show_label(dynamicLabelTextConverted);

          mSegmentNo = -1;
        }
        else
        {
          mIsLastSegment = true;
        }
      }
      else
      {
        mIsLastSegment = false;
      }
      //	calculate remaining data length
      mRemainDataLength = totalDataLength - dataLength;
    }
  }
  else if (iApplType == 03 && mMoreXPad)
  {
    if (mRemainDataLength > (i16)data.size())
    {
      dataLength = data.size();
      mRemainDataLength -= data.size();
    }
    else
    {
      dataLength = mRemainDataLength;
      mMoreXPad = false;
    }

    mDynamicLabelTextUnConverted.append((const char *)data.data(), dataLength);
    _check_charset_change();

    if (!mMoreXPad && mIsLastSegment)
    {
      const QString dynamicLabelTextConverted = toQStringUsingCharset(mDynamicLabelTextUnConverted, mCharSet);
      emit signal_show_label(dynamicLabelTextConverted);
    }
  }
}

//
//	Called at the start of the msc datagroupfield,
//	the msc_length was given by the preceding appType "1"
void PadHandler::_new_MSC_element(const std::vector<u8> & data)
{
  //	if (mscGroupElement) {
  ////	   if (msc_dataGroupBuffer. size() < dataGroupLength)
  ////	      fprintf (stdout, "short ? %d %d\n",
  ////	                              msc_dataGroupBuffer. size(),
  ////	                              dataGroupLength);
  //	   msc_dataGroupBuffer. clear();
  //	   build_MSC_segment (data);
  //	   mscGroupElement	= true;
  //	   show_motHandling (true);
  //	}

  if (data.size() >= (u16)mDataGroupLength)
  {
    // msc element is single item
    mMscDataGroupBuffer.clear();
    _build_MSC_segment(data);
    mMscGroupElement = false;
    emit signal_show_mot_handling();
    //	   fprintf (stdout, "msc element is single\n");
    return;
  }

  mMscGroupElement = true;
  mMscDataGroupBuffer.clear();
  mMscDataGroupBuffer = data;
  emit signal_show_mot_handling();
}

//
void PadHandler::_add_MSC_element(const std::vector<u8> & data)
{
  const i32 currentLength = mMscDataGroupBuffer.size();

  //	just to ensure that, when a "12" appType is missing, the
  //	data of "13" appType elements is not endlessly collected.
  if (currentLength == 0)
  {
    // qCWarning(sLogPadHandler) << "MSC element without AppType 12 element";
    return;
  }

  // The GCC compiler (V13.3.0) complains about the insert() function below, but it seems correct.
  // Do a workaround with a dedicated for-loop. The reserve is obsolete as enough space is reserved,
  // and push_back() will do it if necessary.
  // mMscDataGroupBuffer.reserve(mMscDataGroupBuffer.size() + data.size());
  // mMscDataGroupBuffer.insert(mMscDataGroupBuffer.cend(), data.cbegin(), data.cend());
  for (auto & d : data)
  {
    mMscDataGroupBuffer.push_back(d);
  }

  if (mMscDataGroupBuffer.size() >= (u32)mDataGroupLength)
  {
    _build_MSC_segment(mMscDataGroupBuffer);
    mMscDataGroupBuffer.clear();
    //	   mscGroupElement	= false;
    // emit signal_show_mot_handling(false);
  }
}

void PadHandler::_build_MSC_segment(const std::vector<u8> & iData)
{
  struct SDataGrpHeader0 { u8 DataGroupType : 4;  u8 UserAccessFlag : 1; u8 SegmentFlag : 1;  u8 CrcFlag : 1; u8 ExtensionFlag : 1; };
  struct SDataGrpHeader1 { u8 RepetitionIdx : 4; u8 ContinuityIdx : 4; };

  // we have a MOT segment, let us look what is in it according to DAB 300 401 (page 37) the header (MSC data group)
  const i32 size = std::min((i32)iData.size(), mDataGroupLength);

  if (size < 2)
  {
    fprintf(stderr, "build_MSC_segment: data size < 2\n");
    return;
  }

  SDataGrpHeader0 dataGrpHeader0 = reinterpret_cast<const SDataGrpHeader0 &>(iData[0]);
  // SDataGrpHeader1 dataGrpHeader1 = reinterpret_cast<const SDataGrpHeader1 &>(iData[1]);

  if (dataGrpHeader0.CrcFlag != 0)
  {
    if (!check_crc_bytes(iData.data(), size - 2)) // the last 2 bytes contains the CRC syndrome
    {
      qCWarning(sLogPadHandler) << "Fails on CRC check -> stop decoding MSC segment";
      return;
    }
    // qCDebug(sLogPadHandler) << "CRC check ok";
  }

  i16 segmentNumber = -1; // default
  const u8 groupType = dataGrpHeader0.DataGroupType;

  if ((groupType != 3) && (groupType != 4))
  {
    return;    // do not know yet
  }

  // If the segmentflag is on, then a lastflag and segmentnumber are available, i.e. 2 bytes more.
  // Theoretically, the segment number can be as large as 16384
  u16 index = dataGrpHeader0.ExtensionFlag ? 4 : 2;
  bool lastFlag = false;      // default

  if (dataGrpHeader0.SegmentFlag != 0)
  {
    lastFlag = iData[index] & 0x80;
    segmentNumber = ((iData[index] & 0x7F) << 8) | iData[index + 1];
    // qCDebug(sLogPadHandler) << "segmentNumber" << segmentNumber << "- lastFlag" << lastFlag << "- extensionFlag" << dataGrpHeader.ExtensionFlag;
    index += 2;
  }

  u16 transportId = 0;   // default
  bool transportIdSet = false;

  //	if the user access flag is on there is a user accessfield
  if (dataGrpHeader0.UserAccessFlag != 0)
  {
    const i16 lengthIndicator = iData[index] & 0x0F;
    if ((iData[index] & 0x10) != 0)
    {
      transportId = iData[index + 1] << 8 | iData[index + 2];
      transportIdSet = true;
      // qCDebug(sLogPadHandler) << "transportId" << transportId;
      index += 3;
    }
    /*else
    {
      fprintf (stderr, "sorry no transportId\n");
    }*/
    index += (lengthIndicator - 2);
  }
  // qWarning() << "build_MSC_segment() transportId is " << transportId << "segmentNumber is " << segmentNumber;

  if (!transportIdSet)
  {
    qCritical() << "TransportId not set";
    return;
  }

  const u32 segmentSize = ((iData[index + 0] & 0x1F) << 8) | iData[index + 1];

  //	handling MOT in the PAD, we only deal here with type 3/4
  switch (groupType)
  {
  case 3:
    mpMotObject->set_header(&iData[index + 2], segmentSize, lastFlag, transportId);
    break;

  case 4:
    mpMotObject->add_body_segment(&iData[index + 2], segmentNumber, segmentSize, lastFlag, transportId);
    break;

  default:    // cannot happen
    break;
  }
}

void PadHandler::_reset_charset_change()
{
  mLastConvCharSet = ECharacterSet::Invalid;
}

void PadHandler::_check_charset_change()
{
  /*
   *  Tomneda: This check is only done to ensure that the charSet had not changed within collecting data in dynamicLabelTextUnConverted
   *  before the final conversion tu UTF8, UTF16, EbuLatin is done. The final conversions is done at last because it could happen that a
   *  multi-byte char is transmitted in two different segments and so the beforehand conversion would fail.
   *  I had this with Ensemble "Oberbayern Süd" (7A) with Service "ALPENWELLE" where "ö" was not shown correctly
   *  (with two inversed "??" instead) while a segmented UTF8 transmission.
   */

  if (mLastConvCharSet == ECharacterSet::Invalid) // first call after reset? only store current value
  {
    mLastConvCharSet = mCharSet;
  }
  else
  {
    if (mLastConvCharSet != mCharSet) // has value change? show this with an error
    {
      qCritical("CharSet changed from %d to %d", (int)mLastConvCharSet, (int)mCharSet);
    }
  }
}

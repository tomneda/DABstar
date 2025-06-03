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
#include "charsets.h"
#include "data_manip_and_checks.h"
#include <QLoggingCategory>

// Q_LOGGING_CATEGORY(sLogPadHandler, "PadHandler", QtDebugMsg)
Q_LOGGING_CATEGORY(sLogPadHandler, "PadHandler", QtWarningMsg)

PadHandler::PadHandler(DabRadio * mr)
  : mpRadioInterface(mr)
{
  mpMotObject.reset(new MotObject(mr, false));

  connect(this, &PadHandler::signal_show_label, mr, &DabRadio::slot_show_label);
  connect(this, &PadHandler::signal_show_mot_handling, mr, &DabRadio::slot_show_mot_handling);

  mMscDataGroupBuffer.reserve(1024); // try to avoid future memory swapping
}

//	Data is stored reverse, we pass the vector and the index of the
//	last element of the XPad data.
//	 L0 is the "top" byte of the L field, L1 the next to top one.
void PadHandler::process_PAD(const u8 * const iBuffer, const i16 iLast, const u8 iL1, const u8 iL0)
{
  const u8 fpadType = (iL1 >> 6) & 0x3;

  if (fpadType != 0x0)
  {
    qWarning() << "fpadType" << fpadType << "not supported";
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
    u8 CI = iBuffer[iLast];
    mFirstSegment = (iBuffer[iLast - 1] & 0x40) != 0;
    mLastSegment = (iBuffer[iLast - 1] & 0x20) != 0;
    mCharSet = iBuffer[iLast - 2] & 0x0F;
    u8 AcTy = CI & 037;  // application type

    if (mFirstSegment)
    {
      mDynamicLabelTextUnConverted.clear();
      _reset_charset_change();
    }
    switch (AcTy)
    {
    default: break;

    case 0:  // end marker
      break;
      //
    case 2:  // start of fragment, extract the length
      if (mFirstSegment && !mLastSegment)
      {
        mSegmentNumber = iBuffer[iLast - 2] >> 4;
        if (!mDynamicLabelTextUnConverted.isEmpty())
        {
          const QString dynamicLabelTextConverted = toQStringUsingCharset(mDynamicLabelTextUnConverted, (CharacterSet)mCharSet);
          emit signal_show_label(dynamicLabelTextConverted);
        }
        mDynamicLabelTextUnConverted.clear();
        _reset_charset_change();
      }
      mStillToGo = iBuffer[iLast - 1] & 0x0F;
      mShortPadData.resize(0);
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
        mDynamicLabelTextUnConverted.append((const char *)mShortPadData.data(), (int)mShortPadData.size());
        _check_charset_change();
        mShortPadData.resize(0);
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
      mDynamicLabelTextUnConverted.append((const char *)mShortPadData.data());
      _check_charset_change();
      mShortPadData.resize(0);
      //	if we are at the end of the last segment (and the text is not empty)
      //	then show it.
      if (!mFirstSegment && mLastSegment)
      {
        if (!mDynamicLabelTextUnConverted.isEmpty())
        {
          const QString dynamicLabelTextConverted = toQStringUsingCharset(mDynamicLabelTextUnConverted, (CharacterSet)mCharSet);
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
static constexpr std::array<i16, 8> lengthTable = { 4, 6, 8, 12, 16, 24, 32, 48 };


//	Since the data is reversed, we pass on the vector address
//	and the offset of the last element in the vector,
//	i.e. we start (downwards)  beginning at b [last];
void PadHandler::_handle_variable_PAD(const u8 * const iBuffer, const i16 iLast, const bool iCiFlag)
{
  i16 base = iLast;
  std::vector<u8> data;    // for the local addition

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

      data.resize(mXPadLength);
      for (i16 j = 0; j < mXPadLength; j++)
      {
        data[j] = iBuffer[iLast - j];
      }

      switch (mLastAppType)
      {
      case 2:   // Dynamic label segment, start of X-PAD data group
      case 3:   // Dynamic label segment, continuation of X-PAD data group
        _dynamic_label((u8 *)(data.data()), mXPadLength, 3);
        break;

      case 12:   // MOT, start of X-PAD data group
      case 13:   // MOT, continuation of X-PAD data group
        if (mMscGroupElement)
          _add_MSC_element(data);
        break;
      }
    }
    return;
  }
  //
  //	The CI flag in the F_PAD data is set, so we have local CI's
  //	7.4.2.2: Contents indicators are one byte long

  i16 CI_Index = 0;
  std::array<u8, 4> CI_table;

  while (((iBuffer[base] & 037) != 0) && (CI_Index < 4))
  {
    CI_table[CI_Index++] = iBuffer[base--];
  }

  if (CI_Index < 4)
  {
    // we have a "0" indicator, adjust base
    base -= 1;
  }

  //	The space for the CI's does belong to the Cpadfield, so
  //	but do not forget to take into account the '0'field if CI_Index < 4
  //if (mscGroupElement)
  {
    mXPadLength = 0;
    for (i16 i = 0; i < CI_Index; i++)
    {
      mXPadLength += lengthTable[CI_table[i] >> 5];
    }
    mXPadLength += CI_Index == 4 ? 4 : CI_Index + 1;
    //	   fprintf (stdout, "xpadLength set to %d\n", xpadLength);
  }

  //	Handle the contents
  for (i16 i = 0; i < CI_Index; i++)
  {
    u8 appType = CI_table[i] & 037;
    i16 length = lengthTable[CI_table[i] >> 5];

    //fprintf(stderr, "appType(%d) = %d, length=%d\n", i, appType, length);
    if (appType == 1)
    {
      mDataGroupLength = ((iBuffer[base] & 077) << 8) | iBuffer[base - 1];
      base -= 4;
      mLastAppType = 1;
      continue;
    }

    //	collect data, reverse the reversed bytes
    data.resize(length);
    for (i16 j = 0; j < length; j++)
    {
      data[j] = iBuffer[base - j];
    }

    switch (appType)
    {
    default: return; // sorry, we do not handle this

    case 2:   // Dynamic label segment, start of X-PAD data group
    case 3:   // Dynamic label segment, continuation of X-PAD data group
      // qCDebug(sLogPadHandler) << "DL, start of X-PAD data group, size " << data.size() << "appType=" << appType;
      _dynamic_label((u8 *)(data.data()), data.size(), CI_table[i]);
      break;

    case 12:   // MOT, start of X-PAD data group
      // qCDebug(sLogPadHandler) << "MOT, start of X-PAD data group, size " << data.size() << "appType=" << appType;
      _new_MSC_element(data);
      break;

    case 13:   // MOT, continuation of X-PAD data group
      // qCDebug(sLogPadHandler) << "MOT, start of X-PAD data group, size " << data.size() << "appType=" << appType;
      _add_MSC_element(data);
      break;
    }

    mLastAppType = appType;
    base -= length;

    if (base < 0 && i < CI_Index - 1)
    {
      // fprintf (stdout, "Hier gaat het fout, base = %d\n", base);
      return;
    }
  }
}

//
//	A dynamic label is created from a sequence of (dynamic) xpad
//	fields, starting with CI = 2, continuing with CI = 3
void PadHandler::_dynamic_label(const u8 * data, i16 length, u8 CI)
{
  i16 dataLength = 0;

  if ((CI & 037) == 02)
  {
    // start of segment
    const u16 prefix = (data[0] << 8) | data[1];
    const u8 field_1 = (prefix >> 8) & 017;
    const u8 Cflag = (prefix >> 12) & 01;
    const u8 first = (prefix >> 14) & 01;
    const u8 last = (prefix >> 13) & 01;
    dataLength = length - 2; // The length with header removed

    if (first)
    {
      mSegmentNo = 1;
      mCharSet = (prefix >> 4) & 017;
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
      u16 Command = (prefix >> 8) & 0x0f;
      if (Command == 1)
      {
        // clear the display
        mDynamicLabelTextUnConverted.clear();
        _reset_charset_change();
        mSegmentNo = -1;
      }
      /*else if (Command == 2)
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

      if (length - 2 < totalDataLength)
      {
        dataLength = length - 2; // the length is shortened by header
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
          const QString dynamicLabelTextConverted = toQStringUsingCharset(mDynamicLabelTextUnConverted, (CharacterSet)mCharSet);
          emit signal_show_label(dynamicLabelTextConverted);
          //	            fprintf (stderr, "last segment encountered\n");
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
  else if (((CI & 037) == 03) && mMoreXPad)
  {
    if (mRemainDataLength > length)
    {
      dataLength = length;
      mRemainDataLength -= length;
    }
    else
    {
      dataLength = mRemainDataLength;
      mMoreXPad = false;
    }

    mDynamicLabelTextUnConverted.append((const char *)data, dataLength);
    _check_charset_change();

    if (!mMoreXPad && mIsLastSegment)
    {
      const QString dynamicLabelTextConverted = toQStringUsingCharset(mDynamicLabelTextUnConverted, (CharacterSet)mCharSet);
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
  // we have a MOT segment, let us look what is in it according to DAB 300 401 (page 37) the header (MSC data group)
  const i32 size = iData.size() < (u32)mDataGroupLength ? iData.size() : mDataGroupLength;

  if (size < 2)
  {
    fprintf(stderr, "build_MSC_segment: data size < 2\n");
    return;
  }

  if ((iData[0] & 0x40) != 0)
  {
    if (!check_crc_bytes(iData.data(), size - 2))
    {
      qCWarning(sLogPadHandler) << "build_MSC_segment() fails on crc check";
      return;
    }
    // qCDebug(sLogPadHandler) << "build_MSC_segment() crc check ok";
  }

  i16 segmentNumber = -1; // default
  bool lastFlag = false;      // default
  const u8 groupType = iData[0] & 0xF;
  // const u8 continuityIndex = (data [1] & 0xF0) >> 4;
  // const u8 repetitionIndex =  data [1] & 0xF;

  if ((groupType != 3) && (groupType != 4))
  {
    return;    // do not know yet
  }

  // If the segmentflag is on, then a lastflag and segmentnumber are available, i.e. 2 bytes more.
  // Theoretically, the segment number can be as large as 16384
  bool extensionFlag = (iData[0] & 0x80) != 0;
  u16 index = extensionFlag ? 4 : 2;
  const bool segmentFlag = (iData[0] & 0x20) != 0;

  if (segmentFlag)
  {
    lastFlag = iData[index] & 0x80;
    segmentNumber = ((iData[index] & 0x7F) << 8) | iData[index + 1];
    // qCDebug(sLogPadHandler) << "segmentNumber" << segmentNumber << "- lastFlag" << lastFlag << "- extensionFlag" << extensionFlag;
    index += 2;
  }

  u16 transportId = 0;   // default
  bool transportIdSet = false;

  //	if the user access flag is on there is a user accessfield
  if ((iData[0] & 0x10) != 0)
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
    qCritical() << "build_MSC_segment() transportId not set";
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
  mLastConvCharSet = -1;
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

  if (mLastConvCharSet < 0) // first call after reset? only store current value
  {
    mLastConvCharSet = mCharSet;
  }
  else
  {
    if (mLastConvCharSet != mCharSet) // has value change? show this with an error
    {
      qCritical("charSet changed from %d to %d", mLastConvCharSet, mCharSet);
    }
  }
}

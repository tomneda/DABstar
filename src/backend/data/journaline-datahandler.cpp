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
 *
 */
#include "journaline-datahandler.h"
#include "dabdatagroupdecoder.h"
#include "bit-extractors.h"
#include "journaline-viewer.h"
#include <sys/time.h>
#include "dabradio.h"

static void callback_func(const DAB_DATAGROUP_DECODER_msc_datagroup_header_t * const pHeader,
                          const unsigned long iLen, const unsigned char * ipBuffer, void * pArg)
{
  (void)pHeader;
  JournalineDataHandler * const pDataHandler = static_cast<JournalineDataHandler *>(pArg);
  assert(pDataHandler != nullptr);

  NML::RawNewsObject_t rawNewsObj;
  rawNewsObj.nml_len = iLen;
  rawNewsObj.extended_header_len = 0;
  memcpy(rawNewsObj.nml, ipBuffer, iLen);

  RemoveNMLEscapeSequences escapeCodeHandler;
  NMLFactory nmlFact;
  const std::shared_ptr<NML> pNml = nmlFact.CreateNML(rawNewsObj, &escapeCodeHandler);
  pDataHandler->add_to_dataBase(pNml);
}

JournalineDataHandler::JournalineDataHandler(DabRadio * const ipDR, const i32 iSubChannel)
  : mJournalineViewer(mDataMap, iSubChannel)
{
  mDataGroupDecoder = DAB_DATAGROUP_DECODER_createDec(callback_func, this);

  connect(this, &JournalineDataHandler::signal_new_data, &mJournalineViewer, &JournalineViewer::slot_new_data);
  connect(&mJournalineViewer, &JournalineViewer::signal_window_closed, ipDR, &DabRadio::slot_handle_journaline_viewer_closed);
}

JournalineDataHandler::~JournalineDataHandler()
{
  DAB_DATAGROUP_DECODER_deleteDec(mDataGroupDecoder);
  _destroy_database();
}

//void	journaline_dataHandler::add_mscDatagroup (QByteArray &msc) {
void JournalineDataHandler::add_MSC_data_group(const std::vector<u8> & msc)
{
  const i32 len = (i32)msc.size();
  assert(len % 8 == 0);
  const i32 lenBytes = len / 8;
  auto * const buffer = make_vla(u8, lenBytes);

  for (i32 i = 0; i < lenBytes; i++)
  {
    buffer[i] = getBits_8(msc.data(), i << 3);
  }

  const u32 res = DAB_DATAGROUP_DECODER_putData(mDataGroupDecoder, len / 8, buffer);

  if (res != 0)
  {
    qWarning() << "Error in DAB_DATAGROUP_DECODER_putData" << res;
  }
}

void JournalineDataHandler::_destroy_database()
{
  for (auto i : mDataMap)
  {
    i.pElement.reset(); // not really necessary
  }
  mDataMap.clear();
}

void JournalineDataHandler::add_to_dataBase(const std::shared_ptr<NML> & ipNmlElement)
{
  switch (ipNmlElement->GetObjectType())
  {
  case NML::INVALID:
    return;

  case NML::TITLE: // this is implicitly handled with the List, Plain text or Menu
  case NML::MENU:
  case NML::PLAIN:
  case NML::LIST:
  {
    const auto objId = ipNmlElement->GetObjectId();
    const auto it = mDataMap.find(objId);
    const auto revIdxNew = ipNmlElement->GetRevisionIndex();
    const auto revIdxMap = (it != mDataMap.end()) ? it.value().pElement->revision_index : ~revIdxNew; // invert id if it is a new object to trigger the signal

    if (revIdxNew != revIdxMap)
    {
      JournalineViewer::STableElement & tblElem = mDataMap[objId]; // a not existing map element will be created and initialized here
      tblElem.pElement = ipNmlElement->get_news_ptr(); // get data ownership here
      mDataMap.insert(objId, tblElem);

      emit signal_new_data();
    }
  }
  break;

  default:
    qCritical() << "Unknown object type" << ipNmlElement->GetObjectType();
  }
}


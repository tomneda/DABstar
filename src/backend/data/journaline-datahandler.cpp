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
#include "newsobject.h"
#include <sys/time.h>

#include "dabradio.h"

static void callback_func(const DAB_DATAGROUP_DECODER_msc_datagroup_header_t * header,
                          const unsigned long len, const unsigned char * buf, void * arg)
{
  struct timeval theTime;
  gettimeofday(&theTime, NULL);
  (void)header;
  unsigned char buffer[4096];
  (void)arg;
  long unsigned int nmlSize = 0;
  NewsObject hetNieuws(len, buf, &theTime);
  hetNieuws.copyNml(&nmlSize, buffer);

  NML::RawNewsObject_t theBuffer;
  theBuffer.nml_len = nmlSize;
  theBuffer.extended_header_len = 0;

  for (uint16_t i = 0; i < len; i++)
  {
    theBuffer.nml[i] = buffer[i];
  }

  RemoveNMLEscapeSequences theRemover;
  NMLFactory nmlFact;
  NML * pNml = nmlFact.CreateNML(theBuffer, &theRemover);
  static_cast<JournalineDataHandler *>(arg)->add_to_dataBase(pNml);
  delete pNml;
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
  const i16 len = msc.size();
  auto * const buffer = make_vla(u8, len / 8);

  for (i16 i = 0; i < len / 8; i++)
  {
    buffer[i] = getBits(msc.data(), 8 * i, 8);
  }

  const u32 res = DAB_DATAGROUP_DECODER_putData(mDataGroupDecoder, len / 8, buffer);

  if (res == 0) // TODO: error happens too often?!
  {
    // qCritical() << "Error in DAB_DATAGROUP_DECODER_putData" << res;
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

void JournalineDataHandler::add_to_dataBase(const NML * const ipNmlElement)
{
  switch (ipNmlElement->GetObjectType())
  {
  case NML::INVALID:
  case NML::TITLE: // this is implicitly handled with the List, Plain text or Menu
    return;

  case NML::MENU:
  case NML::PLAIN:
  case NML::LIST:
  {
    const auto objId = ipNmlElement->GetObjectId();
    const auto it = mDataMap.find(objId);
    const auto pElem = it != mDataMap.end() ? it.value().pElement : std::make_shared<NML::News_t>();

    pElem->object_id = objId;
    pElem->object_type = ipNmlElement->GetObjectType();
    pElem->static_flag = ipNmlElement->isStatic();
    pElem->revision_index = ipNmlElement->GetRevisionIndex();
    pElem->extended_header = ipNmlElement->GetExtendedHeader();
    pElem->title = ipNmlElement->GetTitle();
    pElem->item = ipNmlElement->GetItems();

    if (it == mDataMap.end()) // we created a new element, so insert it in the map
    {
      JournalineViewer::STableElement temp{};
      temp.pElement = pElem;
      mDataMap.insert(pElem->object_id, temp);
    }

    emit signal_new_data();
  }
  break;

  default:
    qCritical() << "Unknown object type" << ipNmlElement->GetObjectType();
  }
}


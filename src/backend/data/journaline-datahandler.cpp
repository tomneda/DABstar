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
#include "journaline-screen.h"
#include "newsobject.h"
#include <sys/time.h>

static void my_callBack(const DAB_DATAGROUP_DECODER_msc_datagroup_header_t * header,
                        const unsigned long len, const unsigned char * buf, void * arg)
{
  struct timeval theTime;
  gettimeofday(&theTime, NULL);
  (void)header;
  unsigned char buffer[4096];
  (void)arg;
  long unsigned int nmlSize = 0;
  NML::RawNewsObject_t theBuffer;
  NewsObject hetNieuws(len, buf, &theTime);
  hetNieuws.copyNml(&nmlSize, buffer);
  theBuffer.nml_len = nmlSize;
  theBuffer.extended_header_len = 0;
  for (uint16_t i = 0; i < len; i++)
    theBuffer.nml[i] = buffer[i];
  RemoveNMLEscapeSequences theRemover;
  NMLFactory xxx;
  NML * ttt = xxx.CreateNML(theBuffer, &theRemover);
  static_cast<JournalineDataHandler *>(arg)->add_to_dataBase(ttt);
  delete ttt;
}

JournalineDataHandler::JournalineDataHandler()
  : mJournalineScreen(mTableElemVec)
{
  mDataGroupDecoder = DAB_DATAGROUP_DECODER_createDec(my_callBack, this);
  mTableElemVec.reserve(64);

  connect(this, &JournalineDataHandler::signal_start, &mJournalineScreen, &JournalineScreen::slot_start);
}

JournalineDataHandler::~JournalineDataHandler()
{
  DAB_DATAGROUP_DECODER_deleteDec(mDataGroupDecoder);
  _destroy_dataBase();
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

void JournalineDataHandler::_destroy_dataBase() const
{
  for (auto i : mTableElemVec)
  {
    delete i.element;
  }
}

void JournalineDataHandler::add_to_dataBase(NML * NMLelement)
{
  switch (NMLelement->GetObjectType())
  {
  case NML::INVALID:
    return;
  case NML::MENU:
  case NML::PLAIN:
  case NML::LIST:
  {
    NML::News_t * const x = new NML::News_t;

    x->object_id = NMLelement->GetObjectId();
    x->object_type = NMLelement->GetObjectType();
    x->static_flag = NMLelement->isStatic();
    x->revision_index = NMLelement->GetRevisionIndex();
    x->extended_header = NMLelement->GetExtendedHeader();
    x->title = NMLelement->GetTitle();
    x->item = NMLelement->GetItems();

    const i32 index_oldElement = _findIndex(x->object_id);

    if (index_oldElement >= 0)
    {
      NML::News_t * const p = mTableElemVec[index_oldElement].element;
      delete p;
      mTableElemVec[index_oldElement].element = x;
    }
    else
    {
      JournalineScreen::STableElement temp;
      temp.key = x->object_id;
      temp.element = x;

      mTableElemVec.push_back(temp);

      if (x->object_id == 0)
      {
        emit signal_start(mTableElemVec.size() - 1);
      }
    }
  }
  break;

  default:
    qCritical() << "Unknown object type" << NMLelement->GetObjectType();
    break;
  }
}

int JournalineDataHandler::_findIndex(int key)
{
  for (uint16_t i = 0; i < mTableElemVec.size(); i++)
  {
    if (mTableElemVec[i].key == key)
    {
      return i;
    }
  }
  return -1;
}

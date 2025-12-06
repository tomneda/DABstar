/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *
 * This file is part of the 'NewsService Journaline(R) Decoder'
 *
 * Copyright (c) 2003, 2001-2014 by Fraunhofer IIS, Erlangen, Germany
 *
 * --------------------------------------------------------------------
 *
 * For NON-COMMERCIAL USE,
 * the 'NewsService Journaline(R) Decoder' is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The 'NewsService Journaline(R) Decoder' is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the 'NewsService Journaline(R) Decoder';
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * If you use this software in a project with user interaction, please
 * provide the following text to the user in an appropriate place:
 * "Features NewsService Journaline(R) decoder technology by
 * Fraunhofer IIS, Erlangen, Germany.
 * For more information visit http://www.iis.fhg.de/dab"
 *
 * --------------------------------------------------------------------
 *
 * To use the 'NewsService Journaline(R) Decoder' software for
 * COMMERCIAL purposes, please contact Fraunhofer IIS for a
 * commercial license (see below for contact information)!
 *
 * --------------------------------------------------------------------
 *
 * Contact:
 *   Fraunhofer IIS, Department 'Broadcast Applications'
 *   Am Wolfsmantel 33, 91058 Erlangen, Germany
 *   http://www.iis.fraunhofer.de/dab
 *   mailto:bc-info@iis.fraunhofer.de
 *
 */

//////////////////////////////////////////////////////////////////////////////
/// @brief NML class
///
/// handling of NML (news markup language for Journaline) data
///
/// @file       NML.cpp
/// @author     Michael Reichenbächer <rbr@iis.fraunhofer.de>
///
/// $Id: NML.cpp,v 1.9 2001-2014/09/30 10:33:58 jcable Exp $
///
/// Module:     Journaline(R)
///
/// Copyright:  (C) 2003-2001-2014 by Fraunhofer IIS-A, IT-Services, Erlangen
///
/// Compiler:   gcc version 3.3 20030226 (prerelease)
///             Microsoft Visual C++ .NET
///
//////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <zlib.h>
#include "NML.h"
#include "Splitter.h"
#include "cpplog.h"
#include <QDebug>

static int Inflate(unsigned char * dest,
                   unsigned long * destLen,
                   const unsigned char * source,
                   unsigned long sourceLen);

/// maximum length of a raw NML object
const unsigned int NML::NML_MAX_LEN = 4092;

/// maximum number of hierarchy levels
const unsigned int NML::NML_MAX_NR_OF_LEVELS = 20; // levels 0(root menu)..19

/// maximum number of menu items
const unsigned int NML::NML_MAX_NR_OF_MENU_ITEMS = 32;

/// maximum number of watches
const unsigned int NML::MAX_NR_OF_WATCHES = NML::NML_MAX_NR_OF_MENU_ITEMS + 1;

/// minimum number of bytes in a menu or list item
const unsigned int NML::NML_MIN_NR_OF_ITEM_BYTES = 3;

/// number of header bytes
const unsigned int NML::NML_NR_OF_HEADER_BYTES = 3;

/// object id of root object
const NML::NewsObjectId_t NML::ROOT_OBJECT_ID = 0x0000;

/// textual description of NML object types
const char * NML::ObjectTypeString[] =
  { "illegal", "menu", "plain", "title", "list" };


/// generate an NML Error object from a raw news object
/// generates a fake NML object containing an error message.
/// The title of the error message will be error_msg, and the
/// body will consist of a hex dump of the erroneous NML object.
/// and a hex dump of the raw news object
/// @param oid        NML object id
/// @param rno        raw NML object
/// @param error_msg  error message
/// @return  pointer to created NML errordump object (delete after use!)
std::shared_ptr<NML> NMLFactory::CreateErrorDump(NML::NewsObjectId_t oid, const NML::RawNewsObject_t & rno, const char * error_msg)
{
  log_err << "CreateErrorDump" << endmsg;

  std::shared_ptr<NML> n = CreateError(oid, error_msg);
  n->SetErrorDump(oid, rno, error_msg);
  return n;
}


/// generate an invalid NML error object
/// The title of the error message will be title, and the
/// body will consist of a hex dump of the erroneous NML object.
/// and a hex dump of the raw news object
/// @param oid    NML object id
/// @param title  title error message
/// @return  pointer to error object (delete after use!)
std::shared_ptr<NML> NMLFactory::CreateError(NML::NewsObjectId_t oid, const char * title)
{
  log_err << "CreateError" << endmsg;

  std::shared_ptr<NML> n = std::make_shared<NML>();
  n->_valid = false;
  n->_pNews->object_id = oid;
  n->_pNews->title = title;
  n->_pNews->object_type = NML::TITLE;
  n->_pNews->static_flag = false;
  n->_pNews->revision_index = 0x00;
  return n;
}


/// dump raw NML object
/// @param rno  raw NML news object
/// @return  dump of NML object as string
std::string DumpRaw(const NML::RawNewsObject_t & rno)
{
  return HexDump(rno.nml, static_cast<int>(rno.nml_len), 8);
}


/// dump news object to stream
/// @param os  output stream to dump to
/// @return  output stream (for chained stream io)
std::ostream & NML::operator<<(std::ostream & os) const
{
  return os << Dump();
}


/// dump the news object as string
/// @return  dump as string
std::string NML::Dump(void) const
{
  char buf[4096];
  std::string items = "";
  unsigned int nr_of_items = GetNrOfItems();
  for (unsigned int i = 0; i < nr_of_items; i++)
  {
    if (isMenu())
    {
      sprintf(buf,
              "Link #%d [%04x] %s\n",
              i,
              GetLinkId(i),
              (isLinkIdAvailable(i) ? "is" : "would be"));
    }
    else
    {
      sprintf(buf, "Item #%d is\n", i);
    }
    items += buf;
    items += HexDump(GetItemText(i).c_str(),
                     GetItemText(i).size());
  }

  sprintf(buf,
          "object_id=%04x, object_type: %s(%d), static_flag=%1x,\n"
          "revision=%d,extended_header_len=%d,nr_of_items=%d\n",
          GetObjectId(),
          GetObjectTypeString(),
          GetObjectType(),
          isStatic(),
          GetRevisionIndex(),
          (int)_pNews->extended_header.size(),
          nr_of_items);
  std::string s = buf;
  if (_pNews->extended_header.size())
  {
    s += "\nextended header: ";
    s += HexDump(_pNews->extended_header.c_str(),
                 _pNews->extended_header.size());
  }
  s += "\ntitle=\n";
  s += HexDump(_pNews->title.c_str(),
               _pNews->title.size());
  s += "\nitems:\n";
  s += items;
  return s;
}

//ignore datasections and return a new char pointer pointing to
//a buffer without datasections and will manipulate p and plen accordingly
//reslen is an out parameter returning the size of the buffer
//returns a newly created buffer containing pure nml data on success
//returns 0 on error
unsigned char * NMLFactory::getNextSection(std::vector<NML::SLinkData> & oLinkData, const unsigned char *& p, unsigned short & plen, unsigned short & reslen) const
{
  int i = 0;
  int j = 0;
  int dslen;
  unsigned char * res = (unsigned char *)malloc(plen + 1);

  // we will loop through the entire data until a break case is met
  while (i < plen)
  {
    if (p[i] == 0x1a // datasection begin
        || p[i] == 0x1b // datasection continue
    )
    {
      // we take the length of the datasection and add it
      // to the current position in order to ignore it
      dslen = p[i + 1] + 1;

      if (dslen > 4 && p[i] == 0x1a && p[i+2] == 0x03 && p[i+3] == 0x02) // is it a valid URL link target header?
      {
        extract_link_data(oLinkData, p+i);
      }
      else if(dslen > 4 && p[i] == 0x1a && p[i+2] == 0xc0) // Picture
      {
        NML::SLinkData linkData;

        linkData.urlStr = std::string(reinterpret_cast<const char *>(p+6), dslen-5);
        linkData.textStr = "Bild";
        oLinkData.emplace_back(std::move(linkData));
      }

      // make a safety check for that the length is still in bounds
      if (dslen > (plen - i))
      {
        // we got an invalid datasection
        // abort!
        reslen = 0;
        free(res);
        return 0;
      }
      // we can safely skip the datasection now
      i += 2 + dslen;
      continue;
    }

    if (!(p[i] & 0xF0)) // any jml code
    {
      // we can quit the loop here since we reached
      // another jml code section
      break;
    }

    // well, here we can copy the contents right over
    res[j++] = p[i++];
  }

  // adjust the pointer and its length
  p += i;
  plen -= i;

  // make a safety null termination
  res[j] = '\0';

  // finally return the result
  reslen = j;
  return res;
}

void NMLFactory::extract_link_data(std::vector<NML::SLinkData> & oLinkData, const unsigned char * const ip) const
{
  std::string ds;
  int segLen = ip[1] + 1;
  const unsigned char * pData = ip + 2; // real data start of (first) segment (link target marker is included)

  do
  {
    ds += std::string(reinterpret_cast<const char *>(pData), segLen);
    if (segLen != 256 || pData[segLen] != 0x1B)
    {
      break; // no further segment existing
    }
    pData += segLen + 2;    // new data start pointer of next segment
    segLen = pData[-1] + 1; // new len of next segment
  }
  while (true);

  if (ds.size() < 6) //  avoid too short unplausible length
  {
    return;
  }

  NML::SLinkData linkData;

  // with missing link text, also the zero marker is not existing, see 5.3.2.3
  const size_t zeroPos = ds.find('\0');

  if (zeroPos != std::string::npos)
  {
    linkData.urlStr = ds.substr(2, zeroPos - 2); // skip link target marker
    linkData.textStr = ds.substr(zeroPos + 1, ds.size() - zeroPos - 1);
  }
  else
  {
    linkData.urlStr = ds.substr(2, ds.size() - 2); // skip link target marker
    linkData.textStr.clear();
  }

  oLinkData.emplace_back(std::move(linkData));
}

/// @brief create an NML object from raw NML
/// An NML object will be created from the specified raw news object,
/// even if the raw news object is invalid. In this case, an error
/// NML object is created containing a hexadecimal dump of the erroneous
/// input.
/// A handler for NML escape code sequences, e.g.
/// RemoveNMLEscapeSequences or NMLEscapeSequences2HTML  must be specified,
/// so that escape code sequences can be handled in an application specific way.
/// @param rno                 raw NML object
/// @param EscapeCodeHandler   handler for NML escape codes
/// @return  pointer to created NML news object (delete after use!)
std::shared_ptr<NML> NMLFactory::CreateNML(const NML::RawNewsObject_t & rno, const NMLEscapeCodeHandler * EscapeCodeHandler)
{
  std::shared_ptr<NML> n = std::make_shared<NML>();
  char buf[256];
  char * error = &buf[0];
  std::string tmp_string;
  unsigned char * pure_text;

  // at least header needs to be present
  if (rno.nml_len < 4)
  {
    sprintf(error, "Error: NML has only %u bytes", rno.nml_len);
    n->SetErrorDump(0x0815, rno, error);
    return n;
  }

  // extract object id
  n->_pNews->object_id = static_cast<unsigned short>((rno.nml[0] << 8) + rno.nml[1]);

  // check for reserved object types
  if (((rno.nml[2] >> 5) == 0) || ((rno.nml[2] >> 5) > NML::LIST))
  {
    sprintf(error, "Error: NML has illegal object type 0x%02x", rno.nml[2] >> 5);
    n->SetErrorDump(n->_pNews->object_id, rno, error);
    return n;
  }

  // extract further header data: object_id, static, compressed, revision
  n->_pNews->object_type = static_cast<NML::object_type_t>(rno.nml[2] >> 5);
  n->_pNews->static_flag = (rno.nml[2] & 0x10) != 0;
  bool compressed_flag = (rno.nml[2] & 0x08) != 0;
  n->_pNews->revision_index = static_cast<unsigned char>(rno.nml[2] & 0x07);
  unsigned short len = rno.nml_len - NML::NML_NR_OF_HEADER_BYTES;

  // extended header checks
  if (rno.extended_header_len > len)
  {
    sprintf(error, "Error: NML extended header too big (%u>%u)", rno.extended_header_len, len);
    n->SetErrorDump(n->_pNews->object_id, rno, error);
    return n;
  }
  len -= rno.extended_header_len;
  const unsigned char * pStart = rno.nml + NML::NML_NR_OF_HEADER_BYTES + rno.extended_header_len;
  const unsigned char * p = pStart;

  NML::RawNewsObject_t uncompressed;

  // if compressed then decompress body
  if (compressed_flag)
  {
    // check GZIP magic bytes
    if (*p != 0x08)
    {
      // unknown (not libz) compression
      sprintf(error, "Error: NML with unknown compression 0x%02x", *p);
      log_err << error << endmsg;
      n->SetErrorDump(n->_pNews->object_id, rno, error);
      return n;
    }
    p++;
    len--;

    // copy header
    memcpy(uncompressed.nml, rno.nml, NML::NML_NR_OF_HEADER_BYTES + rno.extended_header_len);
    unsigned long ulen = 4092;
    int rv = Inflate(uncompressed.nml + NML::NML_NR_OF_HEADER_BYTES + rno.extended_header_len, &ulen, p, len);

    if (!rv)
    {
      sprintf(error, "Error: could not uncompress NML body (%d)", rv);
      n->SetErrorDump(n->_pNews->object_id, rno, error);
      return n;
    }

    /*
    log_info
        << "decompressed NML, "
        << len << " -> " << ulen
        << endmsg;
    */

    if (ulen + NML::NML_NR_OF_HEADER_BYTES + rno.extended_header_len > NML::NML_MAX_LEN)
    {
      sprintf(error, "Error: uncompressed NML body too big (%lu bytes)",
              ulen + NML::NML_NR_OF_HEADER_BYTES + rno.extended_header_len);
      n->SetErrorDump(n->_pNews->object_id, rno, error);
      return n;
    }

    // => ulen < NML::NML_MAX_LEN
    uncompressed.nml_len = static_cast<unsigned short>(ulen);
    p = uncompressed.nml + NML::NML_NR_OF_HEADER_BYTES + rno.extended_header_len;
    len = uncompressed.nml_len;

    /*
    log_info
        << "compressed NML len=" << rno.nml_len
        << ", uncompressed NML len= " << uncompressed.nml_len
        << endmsg;
    */
  }
  // wasn't compressed, so just copy content
  else
  {
    uncompressed.nml_len = rno.nml_len;
    memcpy(uncompressed.nml, rno.nml, rno.nml_len);
  }

  // we take the length of the datasection and add it
  // to the current position in order to ignore it

  while (*p == 0x1a || *p == 0x1b)
  {
    const int dsLen = p[1] + 1;
    const int remLen = len - dsLen - 2;

    if (remLen < 2)
    {
      sprintf(error, "Error: Data section too long");
      n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
      return n;
    }

    if (dsLen > 4 && p[0] == 0x1a && p[2] == 0x03 && p[3] == 0x02) // is it a valid URL link target header?
    {
      extract_link_data(n->_pNews->linkVec, p);
    }

    p += dsLen + 2;
    len = remLen;
  }


  // check for title section
  if (*p != 0x01)
  {
    sprintf(error, "Error: expected NML Code 0x01, got 0x%02x", *p);
    n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
    return n;
  }
  len--;
  p++;

  unsigned short pure_txt_len = 0;

  // extract title
  pure_text = getNextSection(n->_pNews->linkVec, p, len, pure_txt_len);
  if (!pure_text)
  {
    sprintf(error, "Error: ignoreDataSections() returned illegal data section");
    n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
    return n;
  }
  tmp_string.assign(reinterpret_cast<const char *>(pure_text), pure_txt_len);
  free(pure_text);
  // report title to codehandler
  EscapeCodeHandler->Convert(n->_pNews->title, tmp_string);

  n->_pNews->item.clear();

  // title object only needs title..
  if (n->_pNews->object_type == NML::TITLE)
    return n;

  // plain text message will need a text message as well
  if (n->_pNews->object_type == NML::PLAIN)
  {
    if (*p != 0x03)
    {
      sprintf(error, "Error: expected NML Code 0x03, got 0x%02x", *p);
      n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
      return n;
    }
    len--;
    p++;
    pure_text = getNextSection(n->_pNews->linkVec, p, len, pure_txt_len);
    if (!pure_text)
    {
      sprintf(error, "Error: ignoreDataSections() returned illegal data section");
      n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
      return n;
    }
    tmp_string.assign((const char *)(pure_text), pure_txt_len);
    free(pure_text);
    NML::Item_t item;
    EscapeCodeHandler->Convert(item.text, tmp_string);
    n->_pNews->item.push_back(item);
    return n;
  }
  // menu object will need link items
  else if (n->_pNews->object_type == NML::MENU)
  {
    while (len > NML::NML_MIN_NR_OF_ITEM_BYTES)
    {
      if (*p != 0x02)
      {
        sprintf(error, "Error: expected NML Code 0x02, got 0x%02x", *p);
        n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
        return n;
      }
      len--;
      p++;

      NML::Item_t item;
      item.link_id = static_cast<unsigned short>((p[0] << 8) + p[1]);
      item.link_id_available = false;
      len -= 2;
      p += 2;

      pure_text = getNextSection(n->_pNews->linkVec, p, len, pure_txt_len);
      if (!pure_text)
      {
        sprintf(error, "Error: ignoreDataSections() returned illegal data section");
        n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
        return n;
      }
      tmp_string.assign(reinterpret_cast<const char *>(pure_text), pure_txt_len);
      free(pure_text);
      EscapeCodeHandler->Convert(item.text, tmp_string);

      n->_pNews->item.push_back(item);
    }
  }
  // list item will need list items
  else if (n->_pNews->object_type == NML::LIST)
  {
    NML::Item_t item;
    while (len > NML::NML_MIN_NR_OF_ITEM_BYTES)
    {
      if (*p != 0x04 && *p != 0x05)
      {
        sprintf(error, "Error: expected NML Code 0x04 or 0x05, got 0x%02x", *p);
        n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
        return n;
      }
//          bool newRow = (*p == 0x04);
      len--;
      p++;

      pure_text = getNextSection(n->_pNews->linkVec, p, len, pure_txt_len);
      if (!pure_text)
      {
        sprintf(error, "Error: ignoreDataSections() returned illegal data section");
        n->SetErrorDump(n->_pNews->object_id, uncompressed, error);
        return n;
      }
      tmp_string.assign(reinterpret_cast<const char *>(pure_text), pure_txt_len);
      free(pure_text);
      EscapeCodeHandler->Convert(item.text, tmp_string);

      n->_pNews->item.push_back(item);
    }
  }

  // all went well, mark as valid
  n->_valid = true;
  return n;
}


/// default NML constructor
NML::NML()
{
  _pNews = std::make_shared<NML::News_t>();
  _valid = false;
  _pNews->object_type = NML::TITLE;
  _pNews->object_id = 0x0815;
  _pNews->title = "uninit. NML";
}

/// NML comparison operator
/// @param prototype  NML object to compare this with
/// @retval true  NML objects are equal
bool NML::operator==(const NML & prototype) const
{
  if (_pNews->item.size() != prototype._pNews->item.size()) return false;
  for (unsigned int i = 0; i < _pNews->item.size(); i++)
  {
    if (!(_pNews->item[i] == prototype._pNews->item[i])) return false;
  }
  return (_valid != prototype._valid)
         && (_pNews->object_id != prototype._pNews->object_id)
         && (_pNews->static_flag == prototype._pNews->static_flag)
         && (_pNews->revision_index == prototype._pNews->revision_index)
         && (_pNews->extended_header == prototype._pNews->extended_header)
         && (_pNews->title == prototype._pNews->title);
}


/// comparison operator for NML menu/list items
/// @param lhs  left hand side of NML object comparison
/// @param rhs  right hand side of NML object comparison
/// @retval true  NML objects are equal
bool operator==(const NML::Item_t & lhs, const NML::Item_t & rhs)
{
  return (lhs.text == rhs.text);
}


/// set NML to an error object
/// generates a fake NML object containing an error message.
/// The title of the error message will be error_msg, and the
/// body will consist of a hex dump of the erroneous raw NML object.
/// and a hex dump of the raw news object
/// @param oid        NML object id
/// @param rno        raw NML object
/// @param error      error message
void NML::SetErrorDump(NewsObjectId_t oid,
                       const RawNewsObject_t & rno,
                       const char * error)
{
#ifndef NDEBUG
  log_err << "NML::SetErrorDump: " << error << endmsg;
#endif

  _pNews->object_type = NML::PLAIN;
  _pNews->object_id = oid;
  _pNews->static_flag = false;
  _pNews->revision_index = 0;
  _pNews->title = error;

  Splitter s;
  std::vector<std::string> lines;
  std::vector<std::string>::const_iterator lines_i;
  s.Split(lines, DumpRaw(rno));

  _pNews->item.clear();
  for (lines_i = lines.begin(); lines_i != lines.end(); lines_i++)
  {
    NML::Item_t item;
    item.text = *lines_i;
    _pNews->item.push_back(item);
  }
}


/// hexadecimal dump of field of unsigned chars
/// @param p               field
/// @param len             length of field
/// @param bytes_per_line  how many hexadecimal bytes should be
///                        displayed on one line of the hexdump
/// @return  hexdump as string
std::string HexDump(const char * p,
                    unsigned int len,
                    unsigned int bytes_per_line)
{
  return HexDump(reinterpret_cast<const unsigned char *>(p),
                 len,
                 bytes_per_line);
}


/// hexadecimal dump of field of unsigned chars
/// @param p               field
/// @param len             length of field
/// @param bytes_per_line  how many hexadecimal bytes should be
///                        displayed on one line of the hexdump
/// @return  hexdump as string
std::string HexDump(const unsigned char * p,
                    unsigned int len,
                    unsigned int bytes_per_line)
{
  std::string dump, ascii;
  unsigned int i;

  for (i = 0; i < len; i++)
  {
    char buf[4];
    sprintf(buf, "%02x ", p[i]);
    ascii += (isprint(p[i])) ? p[i] : '.';
    dump += buf;
    if (i % bytes_per_line == bytes_per_line - 1)
    {
      dump += " " + ascii + "\n";
      ascii = "";
    }
  }
  if (i % bytes_per_line)
  {
    for (; i % bytes_per_line; i++)
    {
      dump += "   ";
    }
    dump += " " + ascii + "\n";
  }

  return dump;
}


/// for decompressing deflated (RFC1951) data
/// @param dest       where to store decompressed data
/// @param destLen    maximum number of bytes of decompressed data
/// @param source     compressed (deflated) data
/// @param sourceLen  length of compressed data
/// @retval  Z_OK     iff source was inflated successfully
int Inflate(unsigned char * dest,
            unsigned long * destLen,
            const unsigned char * source,
            unsigned long sourceLen)
{
  z_stream stream;
  int err;
  stream.next_in = const_cast<Bytef *>(source);
  stream.avail_in = static_cast<uInt>(sourceLen);
  if (static_cast<uLong>(stream.avail_in) != sourceLen)
  {
    log_err << "avail_in=" << static_cast<uLong>(stream.avail_in)
                << "!= sourceLen=" << sourceLen
                << endmsg;
    return 0;
  }

  stream.next_out = dest;
  stream.avail_out = static_cast<uInt>(*destLen);
  if (static_cast<uLong>(stream.avail_out) != *destLen)
  {
    log_err << "avail_out=" << static_cast<uLong>(stream.avail_out)
                << "!= *destLen=" << *destLen
                << endmsg;
    return 0;
  }

  stream.zalloc = 0;
  stream.zfree = 0;

  err = inflateInit2(&stream, -15);
  if (err != Z_OK)
  {
    log_err << "inflateInit2 failed with " << err
                << endmsg;
    return 0;
  }

  err = inflate(&stream, Z_FINISH);
  if (err != Z_STREAM_END)
  {
    log_err << "inflate failed with " << err
                << endmsg;
    inflateEnd(&stream);
    return 0;
  }
  *destLen = stream.total_out;

  err = inflateEnd(&stream);
  if (err != Z_OK)
  {
    log_err << "inflateEnd failed with " << err
                << endmsg;
    return 0;
  }
  return 1;
}


RemoveNMLEscapeSequences::RemoveNMLEscapeSequences() {}


RemoveNMLEscapeSequences::~RemoveNMLEscapeSequences() {}


/// remove NML escape sequences
/// substitutes some NML sequences but removes most.
/// the result can be displayed on a standard UTF-8 capable
/// terminal.
/// @attention NML data sections and extended NML codes are silently ignored
/// @param src   source string (including NML escape sequences)
/// @param dest  UTF-8 string
/// @return  always true
bool RemoveNMLEscapeSequences::Convert(std::string & dest,
                                       const std::string & src) const
{
  dest.clear();
  dest.reserve(src.length());

  for (unsigned int i = 0; i < src.length(); i++)
  {
    switch (src[i])
    {
    case 0x10: // preferred line break
      dest += "<br>";
      break;
    case 0x11: // preferred word break
      dest += "&shy;";
      break;
    case 0x12:// highlight begin
      dest += "<b>";
      break;
    case 0x13: // highlight end
      dest += "</b>";
      break;
    case 0x14: // end of introductory section
      break;
    case 0x1A: // data section begin -> read over next bytes
    case 0x1B: // data section continuation -> read over next bytes
      ++i;
      i += src[i] + 1;
      break;
    case 0x1C: // extended code begin -> ignore next byte
    case 0x1D: // extended code end -> ignore next byte
      ++i;
      break;
    default:
      dest += src[i];
      break;
    }
  }
  return true;
}

NMLEscapeSequences2HTML::NMLEscapeSequences2HTML() {}


NMLEscapeSequences2HTML::~NMLEscapeSequences2HTML() {}


/// replace NML escape sequences with HTML tags
/// replaces most NML escape sequences with the corresponding
/// HTML tags.
/// The result can be displayed on a standard UTF-8 capable
/// HTML Browser.
/// @attention NML data sections and extended NML codes are silently ignored
/// @param  src source string (including NML escape sequences)
/// @param  dest UTF-8 string
/// @return  always true
bool NMLEscapeSequences2HTML::Convert(std::string & dest,
                                      const std::string & src) const
{
  dest = "";

  for (unsigned int i = 0; i < src.length(); i++)
  {
    switch (src[i])
    {
    case 0x10: // replace preferred line break with <br> tag
      dest += "<br/>\n";
      break;
    case 0x12: // highlight begin -> <em>
      dest += "<em>";
      break;
    case 0x13: // highlight end -> </em>
      dest += "</em>";
      break;
    case 0x1A: // data section begin -> read over next bytes
    case 0x1B: // data section continuation -> read over next bytes
      ++i;
      i += static_cast<unsigned char>(src[i]) + 1;
      break;
    case 0x1C: // extended code begin -> ignore next byte
    case 0x1D: // extended code end -> ignore next byte
      i++;
      break;
    default:
      dest += src[i];
      break;
    }
  }
  return true;
}

#include "fib-decoder.h"
#include "dab-tables.h"

i32 FibDecoder::get_scan_width() const
{
  QString s1 = get_audio_header();
  QString s2 = get_packet_header();
  QStringList l1 = s1.split(";");
  QStringList l2 = s2.split(";");
  return l1.size() >= l2.size() ? l1.size() - 1 : l2.size() - 1;
}

QStringList FibDecoder::basic_print() const
{
  QStringList out;
  bool hasContents = false;
  for (i32 i = 0; i < 64; i++)
  {
    if (currentConfig->serviceComps[i].inUse)
    {
      if (currentConfig->serviceComps[i].TMid != ETMId::StreamModeAudio)
      { // audio
        continue;
      }
      if (!hasContents)
      {
        out << get_audio_header();
      }
      hasContents = true;
      out << get_audio_data(i);
    }
  }
  hasContents = false;
  for (i32 i = 0; i < 64; i++)
  {
    if (currentConfig->serviceComps[i].inUse)
    {
      if (currentConfig->serviceComps[i].TMid != ETMId::PacketModeData)
      { // packet
        continue;
      }
      if (get_sub_channel_of(i) == "")
      {
        continue;
      }
      if (!hasContents)
      {
        out << "\n";
        out << get_packet_header();
      }
      hasContents = true;
      out << get_packet_data(i);
    }
  }
  return out;
}

//
//
QString FibDecoder::get_service_name(i32 index) const
{
  const i32 sid = currentConfig->serviceComps[index].SId;
  const auto it = ensemble->servicesMap.find(sid);

  if (it != ensemble->servicesMap.end())
  {
    return it->second.serviceLabel;
  }
  return "";
}

QString FibDecoder::get_service_id_of(i32 index) const
{
  return QString::number(currentConfig->serviceComps[index].SId, 16);
}

QString FibDecoder::get_sub_channel_of(i32 index) const
{
  const i32 subChannel = currentConfig->serviceComps[index].subChannelId;
  if (subChannel < 0)
  {
    return "";
  }
  return QString::number(subChannel);
}

QString FibDecoder::get_start_address_of(i32 index) const
{
  const i32 subChannel = currentConfig->serviceComps[index].subChannelId;
  const auto it = currentConfig->subChDescMapFig0_1.find(subChannel);
  i32 startAddr = it != currentConfig->subChDescMapFig0_1.end() ? it->second.startAddr : 0;
  return QString::number(startAddr);
}

QString FibDecoder::get_length_of(i32 index) const
{
  const i32 subChannel = currentConfig->serviceComps[index].subChannelId;
  const auto it = currentConfig->subChDescMapFig0_1.find(subChannel);
  i32 Length = it != currentConfig->subChDescMapFig0_1.end() ? it->second.Length : 0;
  return QString::number(Length);
}

QString FibDecoder::get_prot_level_of(i32 index) const
{
  const i32 subChannel = currentConfig->serviceComps[index].subChannelId;
  const auto it = currentConfig->subChDescMapFig0_1.find(subChannel);
  if (it != currentConfig->subChDescMapFig0_1.end())
  {
    return getProtectionLevel(it->second.shortForm, it->second.protLevel);
  }
  else
  {
    return getProtectionLevel(false, 0);
  }
}

QString FibDecoder::get_code_rate_of(i32 index) const
{
  const i32 subChannel = currentConfig->serviceComps[index].subChannelId;
  const auto it = currentConfig->subChDescMapFig0_1.find(subChannel);
  if (it != currentConfig->subChDescMapFig0_1.end())
  {
    return getCodeRate(it->second.shortForm, it->second.protLevel);
  }
  else
  {
    return getCodeRate(false, 0);
  }
}

QString FibDecoder::get_bit_rate_of(i32 index) const
{
  const i32 subChannel = currentConfig->serviceComps[index].subChannelId;
  const auto it = currentConfig->subChDescMapFig0_1.find(subChannel);
  i32 bitRate = it != currentConfig->subChDescMapFig0_1.end() ? it->second.bitRate : 0;
  return QString::number(bitRate);
}

QString FibDecoder::get_dab_type(i32 index) const
{
  const i32 dabType = currentConfig->serviceComps[index].ASCTy;
  return dabType == 077 ? "DAB+" : "DAB";
}

QString FibDecoder::get_language_of(i32 index) const
{
  const i32 subChannel = currentConfig->serviceComps[index].subChannelId;
  const auto it = currentConfig->subChDescMapFig0_5.find(subChannel);
  const i32 language = it != currentConfig->subChDescMapFig0_5.end() ? it->second.language : 0;
  return getLanguage(language);
}

QString FibDecoder::get_program_type_of(i32 index) const
{
  const i32 sid = currentConfig->serviceComps[index].SId;
  const auto it = ensemble->servicesMap.find(sid);

  if (it != ensemble->servicesMap.end())
  {
    const i32 programType = it->second.programType;
    return getProgramType(programType);
  }
  return "";
}

QString FibDecoder::get_fm_freq_of(i32 index) const
{
  const i32 sid = currentConfig->serviceComps[index].SId;
  const auto it = ensemble->servicesMap.find(sid);

  if (it != ensemble->servicesMap.end())
  {
    return it->second.fmFrequency != -1 ? QString::number(it->second.fmFrequency) : "    ";
  }
  return "";
}

QString FibDecoder::get_app_type_of(i32 index) const
{
  const i32 appType = currentConfig->serviceComps[index].appType;
  return QString::number(appType);
}

QString FibDecoder::get_FEC_scheme(i32 index) const
{
  const i32 subChannel = currentConfig->serviceComps[index].subChannelId;
  const auto it = currentConfig->subChDescMapFig0_14.find(subChannel);
  const i32 FEC_scheme = it != currentConfig->subChDescMapFig0_14.end() ? it->second.FEC_scheme : 0;
  return QString::number(FEC_scheme);
}

QString FibDecoder::get_packet_address(i32 index) const
{
  const i32 packetAddr = currentConfig->serviceComps[index].packetAddress;
  return QString::number(packetAddr);
}

QString FibDecoder::get_DSCTy(i32 index) const
{
  const i32 DSCTy = currentConfig->serviceComps[index].DSCTy;
  switch (DSCTy)
  {
  case 60 : return "mot data";
  case 59: return "ip data";
  case 44 : return "journaline data";
  case 5 : return "tdc data";
  default: return "unknow data";
  }
}

//
QString FibDecoder::get_audio_header() const
{
  return QString("serviceName") + ";" + "serviceId" + ";" + "subChannel" + ";" + "start address (CU's)"+ ";"
               + "length (CU's)" + ";" + "protection" + ";" + "code rate" + ";" + "bitrate" + ";"
               + "dab type" + ";" + "language" + ";" + "program type" + ";" + "fm freq" + ";";
}

QString FibDecoder::get_audio_data(i32 index) const
{
  return get_service_name(index) + ";" + get_service_id_of(index) + ";" + get_sub_channel_of(index) + ";" + get_start_address_of(index) + ";"
       + get_length_of(index) + ";" + get_prot_level_of(index) + ";" + get_code_rate_of(index) + ";" + get_bit_rate_of(index) + ";"
       + get_dab_type(index) + ";" + get_language_of(index) + ";" + get_program_type_of(index) + ";" + get_fm_freq_of(index) + ";";
}

//
QString FibDecoder::get_packet_header() const
{
  return QString("serviceName") + ";" + "serviceId" + ";" + "subChannel" + ";" + "start address" + ";"
               + "length" + ";" + "protection" + ";" + "code rate" + ";" + "appType" + ";"
               + "FEC_scheme" + ";" + "packetAddress" + ";" + "DSCTy" + ";";
}

QString FibDecoder::get_packet_data(i32 index) const
{
  return get_service_name(index) + ";" + get_service_id_of(index) + ";" + get_sub_channel_of(index) + ";" + get_start_address_of(index) + ";"
       + get_length_of(index) + ";" + get_prot_level_of(index) + ";" + get_code_rate_of(index) + ";" + get_app_type_of(index) + ";"
       + get_FEC_scheme(index) + ";" + get_packet_address(index) + ";" + get_DSCTy(index) + ";";
}

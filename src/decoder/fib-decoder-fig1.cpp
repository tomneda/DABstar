//
// Created by tomneda on 2025-08-23.
//
#include "fib-decoder.h"
#include "bit-extractors.h"
#include <QDebug>

//	FIG 1 - Cover the different possible labels, section 5.2
void FibDecoder::process_FIG1(const u8 * const d)
{
  const u8 extension = getBits_3(d, 8 + 5);

  switch (extension)
  {
  case 0: FIG1Extension0(d); break;  // ensemble name
  case 1: FIG1Extension1(d); break;  // service name
  case 4: FIG1Extension4(d); break;  // Service Component Label
  case 5: FIG1Extension5(d); break;  // Data service label
  case 6: FIG1Extension6(d); break;  // XPAD label - 8.1.14.4
  default:;
  }
}

//	Name of the ensemble
//
void FibDecoder::FIG1Extension0(const u8 * const d)
{

  // from byte 1 we deduce:
  const u8 extension = getBits_3(d, 8 + 5);
  (void)extension;
  const u32 EId = getBits(d, 16, 16);

  QString name;
  if (!extract_character_set_label(name, d, 32))
  {
    return;
  }

  if (!ensemble->namePresent)
  {
    ensemble->ensembleName = name;
    ensemble->ensembleId = EId;
    ensemble->namePresent = true;

    emit signal_name_of_ensemble(EId, name);
  }

  ensemble->isSynced = true;
}

//
//	Name of service
void FibDecoder::FIG1Extension1(const u8 * const d)
{
  const i32 SId = getBits(d, 16, 16);
  const i16 offset = 32;
  const u8 extension = getBits_3(d, 8 + 5);
  (void)extension;

  QString dataName;
  if (!extract_character_set_label(dataName, d, offset))
  {
    return;
  }

  const auto it = find_service_from_service_name(dataName);

  if (it == ensemble->servicesMap.end())
  {
    create_service(dataName, SId, 0);
  }
  else
  {
    it->second.SCIdS = 0;
    it->second.hasName = true;
  }
}

// service component label 8.1.14.3
void FibDecoder::FIG1Extension4(const u8 * const d)
{
  const u8 PD_bit = getBits_1(d, 16);
  // const u8 Rfa = getBits_3(d, 17);
  const u8 SCIdS = getBits_4(d, 20);

  const u32 SId = PD_bit ? getLBits(d, 24, 32) : getLBits(d, 24, 16);
  const i16 offset = PD_bit ? 56 : 40;

  const auto pScd = find_service_component(currentConfig.get(), SId, SCIdS);  // TODO: why only current config

  if (pScd != nullptr) // TODO: bug? was > 0?
  {
    QString dataName;
    if (!extract_character_set_label(dataName, d, offset))
    {
      return;
    }

    if (find_service_from_service_name(dataName) == ensemble->servicesMap.end())
    {
      if (pScd->TMid == ETMId::StreamModeAudio)
      {
        create_service(dataName, SId, SCIdS);
        emit signal_add_to_ensemble(dataName, SId);
      }
    }
  }
}

//	Data service label - 32 bits 8.1.14.2
void FibDecoder::FIG1Extension5(const u8 * const d)
{
  constexpr i16 offset = 48;
  const u8 extension = getBits_3(d, 8 + 5);
  const u32 SId = getLBits(d, 16, 32);
  (void)extension;

  const auto it = ensemble->servicesMap.find(SId);

  if (it != ensemble->servicesMap.end())
  {
    return;  // entry already existing
  }

  QString serviceName;
  if (!extract_character_set_label(serviceName, d, offset))
  {
    return;
  }

  create_service(serviceName, SId, 0);
}

//	XPAD label - 8.1.14.4
void FibDecoder::FIG1Extension6(const u8 * const d)
{
  const u8 PD_bit = getBits_1(d, 16);
  const u8 Rfu = getBits_3(d, 17);
  (void)Rfu;
  const u8 SCIdS = getBits_4(d, 20);

  u32 SId = 0;
  i16 offset = 0;
  u8 XPAD_apptype;

  if (PD_bit)
  {  // 32 bits identifier for XPAD label
    SId = getLBits(d, 24, 32);
    XPAD_apptype = getBits_5(d, 59);
    offset = 64;
  }
  else
  {  // 16 bit identifier for XPAD label
    SId = getLBits(d, 24, 16);
    XPAD_apptype = getBits_5(d, 43);
    offset = 48;
  }

  (void)SId;
  (void)SCIdS;
  (void)XPAD_apptype;
  (void)offset;
}

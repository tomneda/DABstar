#include "spi_decoder.h"

#include <QTimeZone>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
bool check(const char* const iName, const bool iCondition)
{
  if (!iCondition)
  {
    std::fprintf(stderr, "FAILED: %s\n", iName);
  }
  return iCondition;
}

QDateTime utc(const i32 iYear, const i32 iMonth, const i32 iDay, const i32 iHour, const i32 iMinute, const i32 iSecond = 0)
{
  return QDateTime(QDate(iYear, iMonth, iDay), QTime(iHour, iMinute, iSecond), QTimeZone::UTC);
}

// appends a tag-length-value entry, uses the extended length forms if needed
void add_tlv(std::vector<u8>& ioData, const u8 iTag, const std::vector<u8>& iValue)
{
  ioData.push_back(iTag);
  const usize len = iValue.size();
  if (len < 0xFE)
  {
    ioData.push_back((u8)len);
  }
  else if (len <= 0xFFFF)
  {
    ioData.push_back(0xFE);
    ioData.push_back((u8)(len >> 8));
    ioData.push_back((u8)len);
  }
  else
  {
    ioData.push_back(0xFF);
    ioData.push_back((u8)(len >> 16));
    ioData.push_back((u8)(len >> 8));
    ioData.push_back((u8)len);
  }
  ioData.insert(ioData.end(), iValue.begin(), iValue.end());
}

std::vector<u8> tlv(const u8 iTag, const std::vector<u8>& iValue)
{
  std::vector<u8> v;
  add_tlv(v, iTag, iValue);
  return v;
}

std::vector<u8> cat(const std::vector<std::vector<u8>>& iParts)
{
  std::vector<u8> v;
  for (const auto& p : iParts)
  {
    v.insert(v.end(), p.begin(), p.end());
  }
  return v;
}

std::vector<u8> str(const char* const iText)
{
  std::vector<u8> v;
  for (const char* p = iText; *p != 0; ++p)
  {
    v.push_back((u8)*p);
  }
  return v;
}

// ETSI TS 102 371 V3.2.1, annex C: binary encoding example
const std::vector<u8> cAnnexC =
{
  0x02, 0x52,
    0x21, 0x50,
      0x24, 0x16,
        0x80, 0x04, 0x33, 0xBF, 0xC4, 0x40,
        0x81, 0x04, 0x33, 0xBF, 0xC4, 0x80,
        0x25, 0x08,
          0x80, 0x06, 0x40, 0xE1, 0xCE, 0x15, 0xC2, 0x24,
      0x1C, 0x36,
        0x81, 0x03, 0xFA, 0xE4, 0x51,
        0x80, 0x1B, 'c', 'r', 'i', 'd', ':', '/', '/', 'b', 'b', 'c', '.', 'c', 'o', '.', 'u', 'k', '/', '4', '9', '6', '9', '7', '5', '8', '9', '8', '8',
        0x11, 0x04,
          0x01, 0x02, 'P', 'M',
        0x19, 0x0C,
          0x2C, 0x0A,
            0x80, 0x04, 0x33, 0xBF, 0xC4, 0x40,
            0x81, 0x02, 0x0E, 0x10
};

bool test_annex_c()
{
  bool ok = true;
  SpiDecoder decoder;
  Spi::SResult result;

  ok &= check("annex C decode", decoder.decode(cAnnexC, result));
  ok &= check("annex C type", result.type == Spi::EObjectType::Epg);
  if (!check("annex C one schedule", result.schedules.size() == 1))
  {
    return false;
  }

  const Spi::SSchedule& s = result.schedules[0];
  ok &= check("annex C scope start", s.scopeStart == utc(2003, 12, 18, 17, 0));
  ok &= check("annex C scope stop", s.scopeStop == utc(2003, 12, 18, 18, 0));
  if (check("annex C one service scope", s.serviceScopes.size() == 1))
  {
    const Spi::SBearer& b = s.serviceScopes[0];
    ok &= check("annex C scope bearer", b.hasEnsemble && !b.isXPad && b.ECC == 0xE1 && b.EId == 0xCE15 && b.SId == 0xC224 && b.SCIdS == 0);
  }
  else
  {
    ok = false;
  }

  if (!check("annex C one programme", s.programmes.size() == 1))
  {
    return false;
  }

  const Spi::SProgramme& p = s.programmes[0];
  ok &= check("annex C shortId", p.shortId == 16442449);
  ok &= check("annex C crid", p.crid == "crid://bbc.co.uk/4969758988");
  ok &= check("annex C mediumName", p.names.mediumName == "PM" && p.names.best() == "PM");
  ok &= check("annex C start", p.startTime == utc(2003, 12, 18, 17, 0));
  ok &= check("annex C duration", p.durationSec == 3600);
  ok &= check("annex C stop", p.stop_time() == utc(2003, 12, 18, 18, 0));
  ok &= check("annex C defaults", p.isOnAir && !p.isRecommended && p.ltoMinutes == 0);
  return ok;
}

bool test_truncated_and_garbage()
{
  bool ok = true;
  SpiDecoder decoder;
  Spi::SResult result;

  for (usize len = 0; len < cAnnexC.size(); ++len)
  {
    const std::span<const u8> part(cAnnexC.data(), len);
    if (decoder.decode(part, result))
    {
      std::fprintf(stderr, "truncated object with %zu of %zu bytes was accepted\n", len, cAnnexC.size());
      ok = false;
    }
  }

  // random data (with a valid top-level tag) must not crash
  std::mt19937 rng(4711);
  for (i32 run = 0; run < 2000; ++run)
  {
    std::vector<u8> data(rng() % 300);
    for (auto& b : data)
    {
      b = (u8)rng();
    }
    if (!data.empty())
    {
      data[0] = (run & 1) != 0 ? 0x02 : 0x03;
    }
    (void)decoder.decode(data, result);
  }

  // also random mutations of the valid object
  for (i32 run = 0; run < 2000; ++run)
  {
    std::vector<u8> data = cAnnexC;
    data[rng() % data.size()] = (u8)rng();
    (void)decoder.decode(data, result);
  }

  ok &= check("unknown top-level tag", !decoder.decode(std::vector<u8>{ 0x05, 0x00 }, result));
  return ok;
}

bool test_time_point()
{
  bool ok = true;
  QDateTime t;
  i32 lto = 0;

  // long form with LTO: MJD 60000 (2023-02-25) 04:30:15 UTC, LTO -1.5 h
  const u32 mjd = 60000;
  const u32 word = (mjd << 14) | (1u << 12) | (1u << 11) | (4u << 6) | 30u;
  const std::vector<u8> data = { (u8)(word >> 24), (u8)(word >> 16), (u8)(word >> 8), (u8)word, (u8)(15 << 2), 0x00, (u8)(0x20 | 3) };
  ok &= check("time long form decode", SpiDecoder::decode_time_point(data, t, lto));
  ok &= check("time long form value", t == utc(2023, 2, 25, 4, 30, 15));
  ok &= check("time long form lto", lto == -90);

  // short form with positive LTO
  const u32 word2 = (mjd << 14) | (1u << 12) | (23u << 6) | 59u;
  const std::vector<u8> data2 = { (u8)(word2 >> 24), (u8)(word2 >> 16), (u8)(word2 >> 8), (u8)word2, 0x02 };
  ok &= check("time short form decode", SpiDecoder::decode_time_point(data2, t, lto));
  ok &= check("time short form value", t == utc(2023, 2, 25, 23, 59));
  ok &= check("time short form lto", lto == 60);

  // missing bytes
  ok &= check("time missing LTO byte", !SpiDecoder::decode_time_point(std::span(data2.data(), 4), t, lto));
  ok &= check("time missing seconds", !SpiDecoder::decode_time_point(std::span(data.data(), 5), t, lto));
  ok &= check("time too short", !SpiDecoder::decode_time_point(std::span(data.data(), 3), t, lto));

  // invalid hours
  const u32 word3 = (mjd << 14) | (25u << 6);
  const std::vector<u8> data3 = { (u8)(word3 >> 24), (u8)(word3 >> 16), (u8)(word3 >> 8), (u8)word3 };
  ok &= check("time invalid hours", !SpiDecoder::decode_time_point(data3, t, lto));
  return ok;
}

bool test_bearer_and_genre()
{
  bool ok = true;
  Spi::SBearer b;

  // 32 bit SId with ensemble and SCIdS 3
  ok &= check("bearer 32 bit", SpiDecoder::decode_bearer(std::vector<u8>{ 0x53, 0xE0, 0x10, 0x20, 0xE0, 0x12, 0x34, 0x56 }, b));
  ok &= check("bearer 32 bit values", b.hasEnsemble && b.ECC == 0xE0 && b.EId == 0x1020 && b.SId == 0xE0123456 && b.SCIdS == 3);
  // 16 bit SId without ensemble (older encoders)
  ok &= check("bearer no ensemble", SpiDecoder::decode_bearer(std::vector<u8>{ 0x00, 0xD3, 0x01 }, b) && !b.hasEnsemble && b.SId == 0xD301);
  ok &= check("bearer too short", !SpiDecoder::decode_bearer(std::vector<u8>{ 0x40, 0xE1, 0xCE, 0x15, 0xC2 }, b));

  ok &= check("genre href", SpiDecoder::decode_genre_href(std::vector<u8>{ 0x03, 0x06, 0x01 }) == "urn:tva:metadata:cs:ContentCS:2005:3.6.1");
  ok &= check("genre undefined CS", SpiDecoder::decode_genre_href(std::vector<u8>{ 0x09, 0x01 }).isEmpty());
  return ok;
}

bool test_service_information()
{
  bool ok = true;

  // token table: 0x01 -> "Radio ", 0x13 -> " Info"
  const auto tokenTable = tlv(0x04, cat({ { 0x01, 6 }, str("Radio "), { 0x13, 5 }, str(" Info") }));
  const auto defaultLang = tlv(0x06, str("de"));

  std::vector<u8> longDescr(300, 'x'); // forces the extended length form 0xFE

  const auto service = tlv(0x28, cat({
    tlv(0x80, { 0x00, 0x05 }),                                           // version
    tlv(0x10, tlv(0x01, cat({ { 0x01 }, str("One") }))),                 // shortName with token
    tlv(0x12, cat({ tlv(0x80, str("en")), tlv(0x01, cat({ { 0x01 }, str("One"), { 0x13 } })) })), // longName with xml:lang
    tlv(0x7E, { 1, 2, 3 }),                                              // unknown element, must be skipped
    tlv(0x13, cat({                                                       // mediaDescription
      tlv(0x1A, tlv(0x01, str("Short"))),
      tlv(0x1B, tlv(0x01, longDescr)),
      tlv(0x2B, cat({ tlv(0x80, str("image/png")), tlv(0x82, str("logo32.png")), tlv(0x83, { 0x04 }), tlv(0x84, { 0x00, 32 }), tlv(0x85, { 0x00, 32 }) }))
    })),
    tlv(0x14, tlv(0x80, { 0x03, 0x01 })),                                // genre
    tlv(0x29, tlv(0x80, { 0x40, 0xE0, 0x10, 0x20, 0xD3, 0x01 }))         // bearer
  }));

  const auto ensemble = tlv(0x26, cat({ tlv(0x80, { 0xE0, 0x10, 0x20 }), tlv(0x10, tlv(0x01, str("Ens"))), service }));
  const auto si = tlv(0x03, cat({ tlv(0x80, { 0x00, 0x07 }), tlv(0x82, str("orig")), tokenTable, defaultLang, ensemble }));

  SpiDecoder decoder;
  Spi::SResult result;
  ok &= check("SI decode", decoder.decode(si, result));
  ok &= check("SI type", result.type == Spi::EObjectType::ServiceInformation);
  ok &= check("SI default language", result.defaultLanguage == "de");

  const Spi::SServiceInformation& s = result.serviceInformation;
  ok &= check("SI attributes", s.version == 7 && s.originator == "orig");
  ok &= check("SI ensemble", s.hasEnsemble && s.ECC == 0xE0 && s.EId == 0x1020 && s.ensembleNames.shortName == "Ens");

  if (!check("SI one service", s.services.size() == 1))
  {
    return false;
  }

  const Spi::SService& sv = s.services[0];
  ok &= check("SI service version", sv.version == 5);
  ok &= check("SI token short name", sv.names.shortName == "Radio One");
  ok &= check("SI token long name", sv.names.longName == "Radio One Info" && sv.names.best() == "Radio One Info");
  ok &= check("SI short description", sv.shortDescription == "Short");
  ok &= check("SI long description", sv.longDescription == QString(300, 'x'));
  ok &= check("SI multimedia", sv.multimedia.size() == 1 && sv.multimedia[0].url == "logo32.png" && sv.multimedia[0].mimeValue == "image/png"
                                && sv.multimedia[0].type == 4 && sv.multimedia[0].width == 32 && sv.multimedia[0].height == 32);
  ok &= check("SI genre", sv.genres.size() == 1 && sv.genres[0] == "urn:tva:metadata:cs:ContentCS:2005:3.1");
  ok &= check("SI bearer", sv.bearers.size() == 1 && sv.bearers[0].SId == 0xD301 && sv.bearers[0].EId == 0x1020);

  // a token table with a wrong length is an error
  const auto badSi = tlv(0x03, tlv(0x04, { 0x01, 10, 'a' }));
  ok &= check("SI bad token table", !decoder.decode(badSi, result));
  return ok;
}

bool test_programme_details()
{
  bool ok = true;
  const u32 mjd = 60000;
  const u32 w = (mjd << 14) | (1u << 12) | (6u << 6) | 0u; // 06:00 UTC, LTO +1 h
  const std::vector<u8> time = { (u8)(w >> 24), (u8)(w >> 16), (u8)(w >> 8), (u8)w, 0x02 };
  const u32 w2 = (mjd << 14) | (8u << 6) | 0u; // 08:00 UTC, second time element (must be ignored)
  const std::vector<u8> time2 = { (u8)(w2 >> 24), (u8)(w2 >> 16), (u8)(w2 >> 8), (u8)w2 };

  const auto programme = tlv(0x1C, cat({
    tlv(0x81, { 0x00, 0x00, 0x2A }),
    tlv(0x83, { 0x02 }),                       // recommendation = yes
    tlv(0x84, { 0x02 }),                       // broadcast = off-air
    tlv(0x10, tlv(0x01, str("News"))),
    tlv(0x2E, tlv(0x10, tlv(0x01, str("Event")))), // programmeEvent, ignored
    tlv(0x19, cat({
      tlv(0x2C, cat({ tlv(0x80, time), tlv(0x81, { 0x07, 0x08 }), tlv(0x82, time2), tlv(0x83, { 0x00, 0x3C }) })),
      tlv(0x2D, tlv(0x80, { 0x40, 0xE0, 0x10, 0x20, 0xD3, 0x02 })),
      tlv(0x2C, cat({ tlv(0x80, time2), tlv(0x81, { 0x00, 0x01 }) }))
    })),
    tlv(0x13, tlv(0x1B, tlv(0x01, str("Long text"))))
  }));
  const auto epg = tlv(0x02, tlv(0x21, cat({ tlv(0x80, { 0x00, 0x02 }), programme })));

  SpiDecoder decoder;
  Spi::SResult result;
  ok &= check("programme decode", decoder.decode(epg, result));
  if (!check("programme found", result.schedules.size() == 1 && result.schedules[0].programmes.size() == 1))
  {
    return false;
  }

  const Spi::SProgramme& p = result.schedules[0].programmes[0];
  ok &= check("programme schedule version", result.schedules[0].version == 2);
  ok &= check("programme shortId", p.shortId == 42);
  ok &= check("programme flags", p.isRecommended && !p.isOnAir);
  ok &= check("programme name", p.names.shortName == "News");
  ok &= check("programme first time", p.startTime == utc(2023, 2, 25, 6, 0) && p.ltoMinutes == 60);
  ok &= check("programme duration", p.durationSec == 0x0708);
  ok &= check("programme actual time", p.actualTime == utc(2023, 2, 25, 8, 0) && p.actualDurationSec == 60);
  ok &= check("programme bearer", p.bearers.size() == 1 && p.bearers[0].SId == 0xD302);
  ok &= check("programme long description", p.longDescription == "Long text" && p.shortDescription.isEmpty());
  return ok;
}
}

int main()
{
  bool ok = true;
  ok &= test_annex_c();
  ok &= test_truncated_and_garbage();
  ok &= test_time_point();
  ok &= test_bearer_and_genre();
  ok &= test_service_information();
  ok &= test_programme_details();

  if (!ok)
  {
    std::fprintf(stderr, "spi_decoder_test FAILED\n");
    return 1;
  }
  std::printf("spi_decoder_test passed\n");
  return 0;
}

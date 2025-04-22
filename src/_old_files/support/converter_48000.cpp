/*
* This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2011 .. 2023
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
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

#include	"converter_48000.h"
#include	"radio.h"
#include	<cstdio>

converter_48000::converter_48000()
{
}

int converter_48000::convert(const cmplx16 * V, int32_t amount, int32_t rate, std::vector<float> & outB)
{
  switch (rate)
  {
  case 16000: return convert_16000(V, amount, outB);
  case 24000: return convert_24000(V, amount, outB);
  case 32000: return convert_32000(V, amount, outB);
  case 48000: return convert_48000(V, amount, outB);
  default: qWarning("unsupported rate %d", rate);
  }
}

//	scale up from 16 -> 48
//	amount gives number of pairs
int converter_48000::convert_16000(const cmplx16 * V, int amount, std::vector<float> & out)
{
  auto * const buffer = make_vla(cmplx, mapper_16.getMaxOutputsize());
  int teller = 0;
  out.resize(mapper_16.getMaxOutputsize());

  for (int i = 0; i < amount; i++)
  {
    int result;
    if (mapper_16.convert(cmplx(real(V[i]) / 32767.0f, imag(V[i]) / 32767.0f), buffer, &result))
    {
      out.resize(out.size() + 2 * result);

      for (int j = 0; j < result; j++)
      {
        out[teller++] = real(buffer[j]);
        out[teller++] = imag(buffer[j]);
      }
    }
  }

  assert(teller <= (int)out.size());
  out.resize(teller); // this does not move the memory allocation
  return teller;
}

//	scale up from 24000 -> 48000
//	amount gives number of pairs
int converter_48000::convert_24000(const cmplx16 * V, int amount, std::vector<float> & out)
{
  auto * const buffer = make_vla(cmplx, mapper_24.getMaxOutputsize());
  int teller = 0;

  out.resize(2 * mapper_24.getMaxOutputsize());

  for (int i = 0; i < amount; i++)
  {
    int amountResult;
    if (mapper_24.convert(cmplx(real(V[i]) / 32767.0f, imag(V[i]) / 32767.0f), buffer, &amountResult))
    {
      for (int j = 0; j < amountResult; j++)
      {
        out[teller++] = real(buffer[j]);
        out[teller++] = imag(buffer[j]);
      }
    }
  }

  assert(teller <= (int)out.size());
  out.resize(teller); // this does not move the memory allocation
  return teller;
}

//	scale up from 32000 -> 48000
//	amount is number of pairs
int converter_48000::convert_32000(const cmplx16 * V, int amount, std::vector<float> & out)
{
  auto * const buffer = make_vla(cmplx, mapper_32.getMaxOutputsize());
  int teller = 0;

  out.resize(2 * mapper_32.getMaxOutputsize());

  for (int i = 0; i < amount; i++)
  {
    int amountResult;
    if (mapper_32.convert(cmplx(real(V[i]) / 32767.0f, imag(V[i]) / 32767.0f), buffer, &amountResult))
    {
      for (int j = 0; j < amountResult; j++)
      {
        out[teller++] = real(buffer[j]);
        out[teller++] = imag(buffer[j]);
      }
    }
  }

  assert(teller <= (int)out.size());
  out.resize(teller); // this does not move the memory allocation
  return teller;
}

int converter_48000::convert_48000(const cmplx16 * V, int amount, std::vector<float> & out)
{
  auto * const buffer = make_vla(cmplx, amount);
  out.resize(2 * amount);

  for (int i = 0; i < amount; i++)
  {
    buffer[i] = cmplx(real(V[i]) / 32768.0f, imag(V[i]) / 32768.0f);
    out[2 * i + 0] = std::real(buffer[i]);
    out[2 * i + 1] = std::imag(buffer[i]);
  }

  return 2 * amount;
}

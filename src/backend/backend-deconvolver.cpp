/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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
#include  "backend-deconvolver.h"
#include  "eep-protection.h"
#include  "uep-protection.h"


BackendDeconvolver::BackendDeconvolver(DescriptorType * d)
{
  if (d->shortForm)
  {
    protectionHandler = new UepProtection(d->bitRate, d->protLevel);
  }
  else
  {
    protectionHandler = new EepProtection(d->bitRate, d->protLevel);
  }
}

BackendDeconvolver::~BackendDeconvolver()
{
  delete protectionHandler;
}

void BackendDeconvolver::deconvolve(const int16_t * rawBits_in, int32_t length, uint8_t * outData)
{
  protectionHandler->deconvolve(rawBits_in, length, outData);
}


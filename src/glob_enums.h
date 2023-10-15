/*
 * Copyright (c) 2023 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef GLOB_ENUMS_H
#define GLOB_ENUMS_H

enum class EIqPlotType
{
  PHASE_CORR_CARR_NORMED,
  PHASE_CORR_MEAN_NORMED,
  RAW_MEAN_NORMED,

  DEFAULT = PHASE_CORR_CARR_NORMED  // use the first element for startup constellation
};

enum class ECarrierPlotType
{
  MOD_QUAL,
  STD_DEV,
  PHASE_ERROR,
  FOUR_QUAD_PHASE,
  REL_POWER,
  SNR,
  NULL_TII,

  DEFAULT = MOD_QUAL  // use the first element for startup constellation
};

enum class ESoftBitType
{
  AVER,
  FAST,
  QTDAB,
  QTDAB_MOD,
  FIX_HIGH,
  FIX_MED,
  FIX_LOW,

  DEFAULT = AVER  // use the first element for startup constellation
};

#endif // GLOB_ENUMS_H

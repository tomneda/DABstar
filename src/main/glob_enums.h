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
  DC_OFFSET_FFT_10,
  DC_OFFSET_FFT_100,
  DC_OFFSET_ADC_10,
  DC_OFFSET_ADC_100,

  DEFAULT = PHASE_CORR_CARR_NORMED  // use the first element for startup constellation
};

enum class ECarrierPlotType
{
  SB_WEIGHT, // soft bit weight
  EVM_PER,
  EVM_DB,
  STD_DEV,
  PHASE_ERROR,
  FOUR_QUAD_PHASE,
  REL_POWER,
  SNR,
  NULL_TII_LIN,
  NULL_TII_LOG,
  NULL_NO_TII,
  NULL_OVR_POW,

  DEFAULT = SB_WEIGHT  // use the first element for startup constellation
};

enum class ESoftBitType  // adapt RadioInterface::get_soft_bit_gen_names() too if changes made here
{
  SOFTDEC1,
  SOFTDEC2,
  SOFTDEC3,

  DEFAULT = SOFTDEC1  // use the first element for startup constellation
};

#endif // GLOB_ENUMS_H

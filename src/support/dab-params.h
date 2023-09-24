/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013, 2014, 2015, 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB (formerly SDR-J, JSDR).
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
#ifndef  DAB_PARAMS_H
#define  DAB_PARAMS_H

#include  <cstdint>
#include  <array>

class DabParams
{
public:
  explicit DabParams(uint8_t iDabMode);
  ~DabParams() = default;

  struct SDabPar
  {                   //  Mode 1
    int16_t L;        //      76 blocks per frame
    int16_t K;        //    1536 number carriers
    int16_t T_n;      //    2656 null length
    int32_t T_F;      //  196608 samples per frame (T_n + L * T_s)
    int16_t T_s;      //    2552 block length
    int16_t T_u;      //    2048 useful part, FFT length
    int16_t T_g;      //     504 guard length (T_s - T_u)
    int16_t CarrDiff; //    1000
    int16_t CIFs;     //       4
  };

  [[nodiscard]] const SDabPar & get_dab_par() const { return msDabPar[mDabMode]; }

  [[nodiscard]] int16_t get_dabMode()     const { return mDabMode; }
  [[nodiscard]] int16_t get_L()           const { return get_dab_par().L; }
  [[nodiscard]] int16_t get_K()           const { return get_dab_par().K; }
  [[nodiscard]] int16_t get_T_null()      const { return get_dab_par().T_n; }
  [[nodiscard]] int16_t get_T_s()         const { return get_dab_par().T_s; }
  [[nodiscard]] int16_t get_T_u()         const { return get_dab_par().T_u; }
  [[nodiscard]] int16_t get_T_g()         const { return get_dab_par().T_g; }
  [[nodiscard]] int32_t get_T_F()         const { return get_dab_par().T_F; }
  [[nodiscard]] int32_t get_carrierDiff() const { return get_dab_par().CarrDiff; }
  [[nodiscard]] int16_t get_CIFs()        const { return get_dab_par().CIFs; }

private:
  const uint8_t mDabMode{ 0 };
  using TArrDabPar = std::array<SDabPar, 5>;
  static const TArrDabPar msDabPar;
};

#endif

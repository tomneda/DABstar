/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
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

#ifndef DABSTAR_CONFIGURATION_H
#define DABSTAR_CONFIGURATION_H

#include "ui_configuration.h"
#include <QWidget>

class DabRadio;

// allow direct access to components -> makes a live simpler but more dangerous!
class Configuration : public QWidget, public Ui_configWidget
{
  Q_OBJECT

public:
  Configuration(DabRadio * ipRI);
  ~Configuration() override = default;

  void save_position_and_config();

private:
  DabRadio * const mpRadioInterface;
};


#endif 

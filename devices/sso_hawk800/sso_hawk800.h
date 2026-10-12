/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 sso_hawk800
  vendor:             Smooth Synth Operator
  version:            0.1.0
  name:               Korg Poly-800 with HAWK-800 upgrade
  description:        Device definition (parameters, SysEx), factory bank import and generator for the HAWK-800.
  license:            AGPLv3
  minimumCppStandard: 20

  dependencies:       sso_core

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define SSO_HAWK800_H_INCLUDED

#include <sso_core/sso_core.h>

#include "Hawk800Device.h"
#include "Hawk800Generator.h"

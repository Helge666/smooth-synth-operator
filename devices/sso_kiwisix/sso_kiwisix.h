/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 sso_kiwisix
  vendor:             Smooth Synth Operator
  version:            0.1.0
  name:               Korg PolySix with KiwiTechnics KiwiSix upgrade
  description:        Device definition (parameters, MIDI CC map, SysEx) and generator for the KiwiSix.
  license:            AGPLv3
  minimumCppStandard: 20

  dependencies:       sso_core sso_kiwitechnics

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define SSO_KIWISIX_H_INCLUDED

#include <sso_core/sso_core.h>
#include <sso_kiwitechnics/sso_kiwitechnics.h>

#include "KiwiSixDevice.h"
#include "KiwiSixGenerator.h"

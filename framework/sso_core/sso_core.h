/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 sso_core
  vendor:             Smooth Synth Operator
  version:            0.1.0
  name:               Smooth Synth Operator core
  description:        Device-independent building blocks for hardware synth programmers.
  license:            AGPLv3
  minimumCppStandard: 20

  dependencies:       juce_audio_utils

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define SSO_CORE_H_INCLUDED

#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

#include "model/sso_ParamSpec.h"
#include "model/sso_Patch.h"
#include "model/sso_DeviceDefinition.h"
#include "midi/sso_MidiConnection.h"
#include "midi/sso_ParameterSender.h"
#include "state/sso_AppSettings.h"
#include "state/sso_SynthController.h"
#include "gui/sso_GenericParamPanel.h"
#include "gui/sso_ConnectionBar.h"

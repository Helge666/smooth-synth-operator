#pragma once

#include "PluginProcessor.h"
#include "KiwiSixPanel.h"

/** Browser + console (header and the PolySix panel), scaled to the window size. */
class KiwiSixEditor final : public sso::ProgrammerEditor
{
public:
    explicit KiwiSixEditor (KiwiSixProcessor&);

    static constexpr int consoleWidth = 1440;
    static constexpr int designHeight = 800;
};

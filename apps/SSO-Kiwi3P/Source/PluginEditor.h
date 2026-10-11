#pragma once

#include "PluginProcessor.h"
#include "Kiwi3PPanel.h"

/** Browser + console (header and the Kiwi-3P panel), scaled to the window size. */
class Kiwi3PEditor final : public sso::ProgrammerEditor
{
public:
    explicit Kiwi3PEditor (Kiwi3PProcessor&);

    static constexpr int consoleWidth = 1520;
    static constexpr int designHeight = 930;
};

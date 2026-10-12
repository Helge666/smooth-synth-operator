#pragma once

#include "PluginProcessor.h"
#include "Hawk800Panel.h"

/** Browser + console (header and the Poly-800 panel), scaled to the window size. */
class Hawk800Editor final : public sso::ProgrammerEditor
{
public:
    explicit Hawk800Editor (Hawk800Processor&);

    static constexpr int consoleWidth = 1500;
    static constexpr int designHeight = 890;
};

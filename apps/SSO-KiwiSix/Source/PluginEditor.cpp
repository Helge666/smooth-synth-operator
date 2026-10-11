#include "PluginEditor.h"

KiwiSixEditor::KiwiSixEditor (KiwiSixProcessor& p)
    : ProgrammerEditor (p, std::make_unique<KiwiSixPanel> (p.controller, p.parameters, *p.generator),
                        { "KiwiSix Programmer", consoleWidth, designHeight })
{
}

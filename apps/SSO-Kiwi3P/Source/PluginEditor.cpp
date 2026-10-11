#include "PluginEditor.h"

Kiwi3PEditor::Kiwi3PEditor (Kiwi3PProcessor& p)
    : ProgrammerEditor (p, std::make_unique<Kiwi3PPanel> (p.controller, p.parameters, *p.generator),
                        { "Kiwi-3P Programmer", consoleWidth, designHeight })
{
}

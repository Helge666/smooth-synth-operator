#include "PluginEditor.h"

Hawk800Editor::Hawk800Editor (Hawk800Processor& p)
    : ProgrammerEditor (p, std::make_unique<Hawk800Panel> (p.controller, p.parameters, *p.generator),
                        { "HAWK Programmer", consoleWidth, designHeight })
{
}

#include "PluginEditor.h"
#include "SSO_Hawk800_Version.h"

Hawk800Editor::Hawk800Editor (Hawk800Processor& p)
    : ProgrammerEditor (p, std::make_unique<Hawk800Panel> (p.controller, p.parameters, *p.generator),
                        { "HAWK Programmer", SSO_APP_VERSION, consoleWidth, designHeight })
{
}

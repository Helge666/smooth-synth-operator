#include "PluginEditor.h"
#include "SSO_KiwiSix_Version.h"

KiwiSixEditor::KiwiSixEditor (KiwiSixProcessor& p)
    : ProgrammerEditor (p, std::make_unique<KiwiSixPanel> (p.controller, p.parameters, *p.generator),
                        { "KiwiSix Programmer", SSO_APP_VERSION, consoleWidth, designHeight })
{
}

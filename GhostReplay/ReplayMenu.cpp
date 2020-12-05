#include "ScriptMenu.hpp"
#include "Script.hpp"
#include "ReplayScript.hpp"
#include "Constants.hpp"

#include "ScriptMenuUtils.h"

#include "Util/UI.hpp"

#include <fmt/format.h>

std::vector<CScriptMenu<CReplayScript>::CSubmenu> GhostReplay::BuildMenu() {
    std::vector<CScriptMenu<CReplayScript>::CSubmenu> submenus;
    /* mainmenu */
    submenus.emplace_back("mainmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Ghost Car Replay");
        mbCtx.Subtitle(std::string("~b~") + Constants::DisplayVersion);

        CReplayData* activeConfig = context.ActiveReplay();
    });

    return submenus;
}

#pragma once

namespace devilution {

/**
 * @brief Whether DevilTouch's optional iOS touch overlay is live.
 *
 * It is live unless the player turned it off, a directly touchable side panel
 * is open, or "Auto-hide Touch Controls with Controller" is on while the player
 * is using a connected controller. Direct taps, the main panel, inventory and
 * stores are essential game UI and never depend on this.
 */
inline bool TouchOverlayLive(bool showTouchControls, bool autoHideWithController,
    bool controllerConnected, bool lastInputFromController, bool sidePanelOpen)
{
	return showTouchControls && !sidePanelOpen
	    && !(autoHideWithController && controllerConnected && lastInputFromController);
}

} // namespace devilution

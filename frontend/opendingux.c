#include <stdlib.h>
#include <SDL_keysym.h>

#include "main.h"
#include "plugin_lib.h"
#include "libpicofe/input.h"
#include "libpicofe/in_sdl.h"

const struct in_default_bind in_sdl_defbinds[] = {
	{ SDLK_UP,		IN_BINDTYPE_PLAYER12, DKEY_UP },
	{ SDLK_DOWN,	IN_BINDTYPE_PLAYER12, DKEY_DOWN },
	{ SDLK_LEFT,	IN_BINDTYPE_PLAYER12, DKEY_LEFT },
	{ SDLK_RIGHT,	IN_BINDTYPE_PLAYER12, DKEY_RIGHT },
	{ SDLK_LSHIFT,	IN_BINDTYPE_PLAYER12, DKEY_SQUARE },
	{ SDLK_LALT,	IN_BINDTYPE_PLAYER12, DKEY_CROSS },
	{ SDLK_LCTRL,	IN_BINDTYPE_PLAYER12, DKEY_CIRCLE },
	{ SDLK_SPACE,	IN_BINDTYPE_PLAYER12, DKEY_TRIANGLE },
	{ SDLK_RETURN,	IN_BINDTYPE_PLAYER12, DKEY_START },
	{ SDLK_ESCAPE,	IN_BINDTYPE_EMU, DKEY_SELECT },
	{ SDLK_POWER,	IN_BINDTYPE_EMU, SACTION_ENTER_MENU },
	{ SDLK_TAB,		IN_BINDTYPE_PLAYER12, DKEY_L1 },
	{ SDLK_BACKSPACE,	IN_BINDTYPE_PLAYER12, DKEY_R1 },
	{ 0, 0, 0 }
};

const struct menu_keymap in_sdl_key_map[] =
{
	{ SDLK_UP,	PBTN_UP },
	{ SDLK_DOWN,	PBTN_DOWN },
	{ SDLK_LEFT,	PBTN_LEFT },
	{ SDLK_RIGHT,	PBTN_RIGHT },
	{ SDLK_LCTRL,	PBTN_MOK },
	{ SDLK_LALT,	PBTN_MBACK },
	{ SDLK_SPACE,	PBTN_MA2 },
	{ SDLK_LSHIFT,	PBTN_MA3 },
	{ SDLK_TAB,	PBTN_L },
	{ SDLK_BACKSPACE,	PBTN_R },
};

const char * const in_sdl_key_names[SDLK_LAST] = {
	[SDLK_UP] = "UP",
	[SDLK_DOWN] = "DOWN",
	[SDLK_LEFT] = "LEFT",
	[SDLK_RIGHT] = "RIGHT",
	[SDLK_LCTRL] = "A",
	[SDLK_LALT] = "B",
	[SDLK_LSHIFT] = "X",
	[SDLK_SPACE] = "Y",
	[SDLK_TAB] = "L",
	[SDLK_BACKSPACE] = "R",
	[SDLK_RETURN] = "START",
	[SDLK_ESCAPE] = "SELECT",
	[SDLK_POWER] = "POWER",
	[SDLK_PAUSE] = "LOCK",
};

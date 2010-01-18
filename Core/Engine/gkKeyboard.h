/*
-------------------------------------------------------------------------------
	This file is part of the Ogre GameKit port.
	http://gamekit.googlecode.com/

	Copyright (c) 2009 Charlie C.
-------------------------------------------------------------------------------
 This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgment in the product documentation would be
	 appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
-------------------------------------------------------------------------------
*/
#ifndef _gkKeyboard_h_
#define _gkKeyboard_h_

#include "gkCommon.h"
#include "gkMathUtils.h"
#include "gkKeyCodes.h"





// ---------------------------------------------------------------------------
class gkKeyboardDevice
{
public:
	enum State
	{
		STATE_NULL= 0,
		STATE_PRESSED,
		STATE_RELEASED,
	};

	typedef int KeyState[KC_MAX];
public:
	gkKeyboardDevice();

	bool isKeyDown(gkScanCode key) const;

	bool was_pressed;
	bool was_released;
	int key_count;
	KeyState keys;
};

// ---------------------------------------------------------------------------
GK_INLINE gkKeyboardDevice::gkKeyboardDevice():
		was_pressed(false),
		was_released(false),
		key_count(0)
{
	memset(keys, 0, sizeof(KeyState));
}

// ---------------------------------------------------------------------------
GK_INLINE bool gkKeyboardDevice::isKeyDown(gkScanCode key) const
{
	if (key > KC_NONE && key < KC_MAX)
		return (keys[(int)key] & STATE_PRESSED) != 0;
	return false;
}





#endif//_gkKeyboard_h_
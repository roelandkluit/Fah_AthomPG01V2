/*
Copyright (C) 2021 Roeland Kluit - v1.0 January 2021

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

The Software is provided to you by the Licensor under the License,
as defined, subject to the following condition.

Without limiting other conditions in the License, the grant of rights
under the License will not include, and the License does not grant to
you, the right to Sell the Software.

For purposes of the foregoing, "Sell" means practicing any or all of
the rights granted to you under the License to provide to third
parties, for a fee or other consideration (including without
limitation fees for hosting or consulting/ support services related
to the Software), a product or service whose value derives, entirely
or substantially, from the functionality of the Software.
Any license notice or attribution required by the License must also
include this Commons Clause License Condition notice.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#ifndef ButtonManager_h
#define ButtonManager_h

#include "Arduino.h"

class ButtonManager
{
private:
	void(*__SD_CALLBACK_BUTTONPRESS)(bool LongButtonPress) = NULL;
	uint8_t buttonID = 0;
	unsigned long previousMillis = 0;
	void NotifyPress(bool LongButtonPress);
	bool bLongPressHasBeenNotified = false;
public:
	ButtonManager(uint8_t button, bool PullUp);
	bool CheckIsButtonPressed();
	bool defaultStateIsHigh = false; //default value (not pressed is high, pressed is low 
	void process();
	void OnButtonPressEvent(void(*callback)(bool LongButtonPress)) { __SD_CALLBACK_BUTTONPRESS = callback; }
};

#endif

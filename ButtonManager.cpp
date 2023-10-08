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

#include "ButtonManager.h"

#define LONG_PRESS 2000
#define SHORT_PRESS 1

void ButtonManager::NotifyPress(bool LongButtonPress)
{
	if (__SD_CALLBACK_BUTTONPRESS != NULL)
		__SD_CALLBACK_BUTTONPRESS(LongButtonPress);
}

ButtonManager::ButtonManager(uint8_t button, bool InternalPullUp)
{
	buttonID = button;
	if (InternalPullUp)
	{
		pinMode(buttonID, INPUT_PULLUP);
		defaultStateIsHigh = true;
	}
	else
		pinMode(buttonID, INPUT);
}

bool ButtonManager::CheckIsButtonPressed()
{
	if (defaultStateIsHigh)
		return (digitalRead(buttonID) != 1);
	else
		return (digitalRead(buttonID) == 1);
}


void ButtonManager::process()
{
	if (CheckIsButtonPressed() == true)
	{
		if (previousMillis == 0)
		{
			previousMillis = millis();
		}
		else if (!bLongPressHasBeenNotified && (millis() - previousMillis >= (LONG_PRESS)))
		{
			bLongPressHasBeenNotified = true;
			NotifyPress(true);
		}
	}
	else
	{
		if (previousMillis != 0)
		{
			if (bLongPressHasBeenNotified)
			{
				//Ignore, previously notified
			}
			else if (millis() - previousMillis >= (LONG_PRESS))
			{
				NotifyPress(true);
			}
			else
			{
				NotifyPress(false);
			}
			previousMillis = 0;
			bLongPressHasBeenNotified = false;
		}
	}
}

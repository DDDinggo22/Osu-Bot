#pragma once

#include "stdafx.h"

#include "GlobalVariables.h"
#include "timeAddress.h"
#include "SendInput.h"
#include "CursorMovement.h"
#include "SongsReading.h"


using namespace std;


LPVOID GetTimeAddress() {
	int pLevel = 5;
	
	LPVOID osuProcessID = GetProcessID(L"osu!.exe");
	osuProcessHandle = GetHandle(osuProcessID);

	LPVOID thread = GetThreadList(osuProcessID)[0];
	
	ULONG threadStack = GetThreadStack(osuProcessHandle, thread);

	LPVOID threadAddress = UlongToPtr(threadStack + (ULONG)threadOffset);

	return GetAddress(osuProcessHandle, threadAddress, pLevel);
}


void TimeThread() {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	while (true) {
		ReadProcessMemory(osuProcessHandle, timeAddress, &songTime, sizeof(INT), NULL);
		this_thread::sleep_for(chrono::milliseconds(1));
	}
}


void AutoPlay(wstring nowPlaying) {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	wstring beatmap(nowPlaying.begin() + 8, nowPlaying.end());

	if (statusText != L"No New Beatmap Selected! Using The Last Selected Beatmap.")
		statusText = L"Now Playing: " + beatmap;
	DrawTextToWindow(hWnd, statusText, rectStatus);

	prevInputTime = songTime;
	SendMessage(hwndProgressBar, PBM_SETPOS, WPARAM(0), NULL);

	if (hardrockFlip)
		for (unsigned int i = 0; i < hitObjects.size(); i++) {
			hitObjects.at(i).startPosition.y = 384.f - hitObjects.at(i).startPosition.y;
		}

	for (unsigned int nObject = 0; nObject < hitObjects.size(); nObject++) {
		HitObject hit = hitObjects.at(nObject);
		if (songStarted) {

			// Movement calls:
			switch (modeMoveTo) {
			case MODE_NONE:
				break;
			
			case MODE_STANDARD:
				MoveToStandard(&hit);
				break;

			case MODE_FLOWING:
				MoveToCircle(&hitObjects, nObject, FlowVectorPoint);
				break;
			
			case MODE_PREDICTING:
				MoveToCircle(&hitObjects, nObject, PredictionVectorPoint);
				break;
			}

			if ((hit.getHitType() == HIT_CIRCLE || modeSlider == MODE_NONE) && modeMoveTo != MODE_NONE && songStarted) {
				SendKeyPress(&hit);

				int keyPressTime = MAX(static_cast<int>(hit.getBPM() / 4.f), 5);
				this_thread::sleep_for(chrono::milliseconds(keyPressTime));

				SendKeyRelease(&hit);
			}
			else if (hit.getHitType() == HIT_SLIDER) {
				switch (modeSlider) {
				case MODE_NONE:
					break;
				
				case MODE_STANDARD:
					SliderStandard(&hit);
					break;

				case MODE_FLOWING:
					SliderFlowing(&hitObjects, nObject);
					break;

				case MODE_PREDICTING:
					SliderFlowing(&hitObjects, nObject);
					break;
				}
			}
			else if (hit.getHitType() == HIT_SPINNER) {
				switch (modeSpinner) {
				case MODE_NONE:
					break;
				
				case MODE_STANDARD:
					SpinnerStandard(&hit);
					break;

				case MODE_FLOWING:
					SpinnerStandard(&hit);
					break;

				case MODE_PREDICTING:
					SpinnerStandard(&hit);
					break;
				}
			}
			// Movement calls END;

			SendMessage(hwndProgressBar, PBM_SETPOS, WPARAM(nObject), NULL);
		}
		else {
			while (!songStarted && !firstStart) {
				this_thread::sleep_for(chrono::milliseconds(5));
			}
			nObject -= (UINT)2;
		}
	}

	statusText = L"Waiting for user...";
	DrawTextToWindow(hWnd, statusText, rectStatus);

	songStarted = FALSE;
}


void GameActiveChecker() {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	while (true) {
		RECT rect;
		POINT w = { 0, 6 };
		GetClientRect(osuWindow, &rect);
		ClientToScreen(osuWindow, &w);

		int x = min(rect.right, GetSystemMetrics(SM_CXSCREEN)), swidth = x,
			y = min(rect.bottom, GetSystemMetrics(SM_CXSCREEN)), sheight = y;

		if (swidth * 3 > sheight * 4) swidth = sheight * 4 / 3;
		else sheight = swidth * 3 / 4;
		multiplierX = swidth / 640.f;
		multiplierY = sheight / 480.f;

		int xOffset = static_cast<int>(x - 512.f * multiplierX) / 2,
			yOffset = static_cast<int>(y - 384.f * multiplierY) / 2;

		osuWindowX = w.x + xOffset;
		osuWindowY = w.y + yOffset;

		TCHAR titleC[MAXCHAR];
		GetWindowText(osuWindow, (LPTSTR)titleC, MAXCHAR);
		wstring title(titleC);

		if (title != L"osu!" && title != L"" && pathSet) {
			if (firstStart) {
				songStarted = TRUE;

				if (autoOpenSong) {
					SongFileCheck(OpenSongAuto(title), autoSelect);
				}

				thread AutoThread(AutoPlay, title);
				AutoThread.detach();

				firstStart = FALSE;
			}
			else if (songTime > 0) {
				if (songTime == prevTime) {
					songStarted = FALSE;
				}
				else {
					songStarted = TRUE;
				}
			} prevTime = songTime;
		}
		else if (title == L"osu!" && songStarted) {
			firstStart = TRUE;
			songStarted = FALSE;
		}
		else if (title == L"osu!" && !songStarted) {
			firstStart = TRUE;
		}
		else if (title == L"") {
			if (!songStarted) {
				if (true) /*if Option to close Osu!Bot on osu! exit.*/
				{
					TerminateProcess(GetCurrentProcess(), 0);
				}
			}
		}
		this_thread::sleep_for(chrono::milliseconds(100));
	}
}

void FindGame() {
	osuWindow = FindWindow(NULL, L"osu!");
	if (osuWindow == NULL) {
		/* EventLog */	fwprintf(wEventLog, L"[WARNING]  The process \"osu!\" was not found!\n");
		fflush(wEventLog);

		statusText = L"\"osu!\" NOT found!   Please start \"osu!\"...";
		DrawTextToWindow(hWnd, statusText, rectStatus);

		while (osuWindow == NULL) {
			osuWindow = FindWindow(NULL, L"osu!");
			Sleep(500);
		}
	}

	/* EventLog */	fwprintf(wEventLog, L"[EVENT]  osu!.exe FOUND!\n");

	if (threadOffset == 0x0) {
		/* EventLog */	fwprintf(wEventLog, L"[WARNING]  Thread / Pointer Offsets not set!\n");
		fflush(wEventLog);

		statusText = L"Thread / Pointer Offsets not set!  Change it under Settings!";
		DrawTextToWindow(hWnd, statusText, rectStatus);

		while (threadOffset == 0x0) { Sleep(500); }
	}

	timeAddress = GetTimeAddress();

	if (timeAddress == nullptr) {
		CloseHandle(osuProcessHandle);

		/* EventLog */	fwprintf(wEventLog, L"[WARNING]  timeAddress NOT FOUND!\n");
		fflush(wEventLog);

		statusText = L"timeAddress NOT found!";
		DrawTextToWindow(hWnd, statusText, rectStatus);

		FindGame();
	}

	wstringstream timeAddressString;
	timeAddressString << "0x" << hex << PtrToUlong(timeAddress);

	/* EventLog */	fwprintf(wEventLog, (L"[EVENT]  \"timeAddress\" FOUND!  Starting Checker and Time threads!\n            timeAddress: " + timeAddressString.str() + L"\n").c_str());
	fflush(wEventLog);

	statusText = L"Waiting for user...";
	DrawTextToWindow(hWnd, statusText, rectStatus);

	thread gameActiveThread(GameActiveChecker);
	gameActiveThread.detach();
	thread timeThread(TimeThread);
	timeThread.detach();
}

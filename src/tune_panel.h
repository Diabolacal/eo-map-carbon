#pragma once

#include "tune_params.h"

#include <Windows.h>

struct TunePanel
{
	HWND hwnd = nullptr;
	TuneParams* params = nullptr;
};

bool TunePanel_Create(HINSTANCE instance, HWND owner, TuneParams* params, TunePanel& out);
void TunePanel_Destroy(TunePanel& panel);
void TunePanel_SyncFromParams(const TunePanel& panel);
void TunePanel_Show(const TunePanel& panel, bool show);

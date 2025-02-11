/******************************************************************************
/ Analysis.cpp
/
/ Copyright (c) 2011 Tim Payne (SWS)
/
/
/ Permission is hereby granted, free of charge, to any person obtaining a copy
/ of this software and associated documentation files (the "Software"), to deal
/ in the Software without restriction, including without limitation the rights to
/ use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
/ of the Software, and to permit persons to whom the Software is furnished to
/ do so, subject to the following conditions:
/
/ The above copyright notice and this permission notice shall be included in all
/ copies or substantial portions of the Software.
/
/ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
/ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
/ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
/ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
/ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
/ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
/ OTHER DEALINGS IN THE SOFTWARE.
/
******************************************************************************/

#include "stdafx.h"
#include "Analysis.h"
#include "../sws_waitdlg.h"
#include "../reaper/localize.h"


void AnalyzePCMSource(ANALYZE_PCM* a)
{
	if (!a->pcm)
		return;

	// Init local transfer block "t" and sum of squares
	PCM_source_transfer_t t={0,};
	t.samplerate = a->pcm->GetSampleRate();
	t.nch = a->pcm->GetNumChannels();
	t.length = a->dWindowSize == 0.0 ? 16384 : (int)(a->dWindowSize * t.samplerate);
	t.samples = new ReaSample[t.length * t.nch];
	t.time_s = 0.0;

	ReaSample* prevBuf = NULL;
	if (a->dWindowSize != 0.0)
	{
		prevBuf = new ReaSample[t.length * t.nch];
		memset(prevBuf, 0, t.length * t.nch);
	}

	double* dSumSquares = new double[t.nch];
	for (int i = 0; i < t.nch; i++)
		dSumSquares[i] = 0.0;

	// Init output variables.  Note can have different channel count.
	for (int i = 0; i < a->iChannels; i++)
	{
		if (a->dPeakVals) a->dPeakVals[i] = 0.0;
		if (a->dRMSs) a->dRMSs[i] = 0.0;
		if (a->peakSamples) a->peakSamples[i] = 0;
	}
	a->dPeakVal = 0.0;
	a->dRMS = 0.0;
	a->peakSample = 0;
	a->dProgress = 0.0;
	a->sampleCount = 0;

	INT64 totalSamples = (INT64)(a->pcm->GetLength() * t.samplerate);
	int iFrame = 0;

	a->pcm->GetSamples(&t);
	while (t.samples_out)
	{
		for (int samp = 0; samp < t.samples_out; samp++)
		{
			for (int chan = 0; chan < t.nch; chan++)
			{
				int i = samp*t.nch + chan;
				dSumSquares[chan] += t.samples[i] * t.samples[i];
				double absamp = fabs(t.samples[i]);
				if (absamp > a->dPeakVal)
				{
					a->dPeakVal = absamp;
					a->peakSample = a->sampleCount;
				}

				if (a->dPeakVals && chan < a->iChannels && absamp > a->dPeakVals[chan])
				{
					a->dPeakVals[chan] = absamp;
					if (a->peakSamples)
						a->peakSamples[chan] = a->sampleCount;
				}
				if (a->dWindowSize != 0.0)
				{
					dSumSquares[chan] -= prevBuf[i] * prevBuf[i];
					double curRMS = sqrt(dSumSquares[chan] / t.length);
					if (curRMS > a->dRMS) // Overall
						a->dRMS = curRMS;
					if (a->dRMSs && curRMS > a->dRMSs[chan]) // Single channel, if enabled
						a->dRMSs[chan] = curRMS;
				}
			}
			a->sampleCount++;
		}
		if (a->dWindowSize != 0.0)
		{	// Swap buffers in windowed mode for history
			ReaSample* temp = t.samples;
			t.samples = prevBuf;
			prevBuf = temp;
		}


		a->dProgress = (double)a->sampleCount / totalSamples;

		iFrame++;
		t.time_s = (double)t.length * iFrame / t.samplerate;
		// Get next block
		a->pcm->GetSamples(&t);
	}

	if (a->dWindowSize == 0.0)
	{
		// Non-windowed mode.  Calculate the RMS for the entire item
		// First per channel
		if (a->dRMSs && a->sampleCount)
			for (int i = 0; i < a->iChannels && i < t.nch; i++)
				a->dRMSs[i] = sqrt(dSumSquares[i] / a->sampleCount);

		// Then for all channels combined
		double dSS = 0.0;
		for (int i = 0; i < t.nch; i++)
			dSS += dSumSquares[i];
		a->dRMS = sqrt(dSS / (a->sampleCount * t.nch));
	}

	delete[] t.samples;
	delete[] prevBuf;
	delete[] dSumSquares;

	// Ensure dProgress is exactly 1.0
	a->dProgress = 1.0;
}


DWORD WINAPI AnalyzePCMThread(void* pAnalyze)
{
	AnalyzePCMSource((ANALYZE_PCM*)pAnalyze);
	return 0;
}

// return true for successful analysis
// wraps AnalyzePCM to check item validity and create a wait dialog
bool AnalyzeItem(MediaItem* mi, ANALYZE_PCM* a)
{
	a->dProgress = 0.0;
	a->pcm = (PCM_source*)mi;

	if (!a->pcm || strcmp(a->pcm->GetType(), "MIDI") == 0 || strcmp(a->pcm->GetType(), "MIDIPOOL") == 0)
		return false;

	a->pcm = a->pcm->Duplicate();
	if (!a->pcm || !a->pcm->GetNumChannels())
		return false;

	double dZero = 0.0;
	GetSetMediaItemInfo((MediaItem*)a->pcm, "D_POSITION", &dZero);

	const char* cName = NULL;
	MediaItem_Take* take = GetMediaItemTake(mi, -1);
	if (take)
		cName = (const char*)GetSetMediaItemTakeInfo(take, "P_NAME", NULL);

	CreateThread(NULL, 0, AnalyzePCMThread, a, 0, NULL);

	WDL_String title;
	title.AppendFormatted(100, __LOCALIZE_VERFMT("Please wait, analyzing %s...","sws_analysis"), cName ? cName : __LOCALIZE("item","sws_analysis"));
	SWS_WaitDlg wait(title.Get(), &a->dProgress);

	delete a->pcm;
	return true;
}

void DoAnalyzeItem(COMMAND_T*)
{
	WDL_TypedBuf<MediaItem*> items;
	SWS_GetSelectedMediaItems(&items);
	bool bDidWork = false;
	for (int i = 0; i < items.GetSize(); i++)
	{
		MediaItem* mi = items.Get()[i];
		int iChannels = ((PCM_source*)mi)->GetNumChannels();
		if (iChannels)
		{
			bDidWork = true;
			ANALYZE_PCM a;
			memset(&a, 0, sizeof(a));
			a.iChannels = iChannels;
			a.dPeakVals = new double[iChannels];
			a.dRMSs     = new double[iChannels];

			if (AnalyzeItem(mi, &a))
			{
				WDL_String str;
				str.Set(__LOCALIZE("Peak level:","sws_analysis"));
				for (int i = 0; i < iChannels; i++) {
					str.Append(" ");
					str.AppendFormatted(50, __LOCALIZE_VERFMT("Channel %d = %.2f dB","sws_analysis"), i+1, VAL2DB(a.dPeakVals[i]));
				}
				str.Append("\n");
				str.Append(__LOCALIZE("RMS level:","sws_analysis"));
				for (int i = 0; i < iChannels; i++) {
					str.Append(" ");
					str.AppendFormatted(50, __LOCALIZE_VERFMT("Channel %d = %.2f dB","sws_analysis"), i+1, VAL2DB(a.dRMSs[i]));
				}
				MessageBox(g_hwndParent, str.Get(), __LOCALIZE("Item analysis","sws_analysis"), MB_OK);
			}
			delete [] a.dPeakVals;
			delete [] a.dRMSs;
		}
	}
	if (!bDidWork)
	{
		MessageBox(NULL, __LOCALIZE("No items selected to analyze.","sws_analysis"), __LOCALIZE("SWS - Error","sws_analysis"), MB_OK);
		return;
	}
}

void FindItemPeak(COMMAND_T*)
{
	// Just use the first item
	MediaItem* mi = GetSelectedMediaItem(NULL, 0);
	if (mi)
	{
		ANALYZE_PCM a;
		memset(&a, 0, sizeof(a));
		if (AnalyzeItem(mi, &a))
		{
			double dSrate = ((PCM_source*)mi)->GetSampleRate();
			double dPos = *(double*)GetSetMediaItemInfo(mi, "D_POSITION", NULL);
			dPos += a.peakSample / dSrate;
			SetEditCurPos(dPos, true, false);
		}
	}
	else
		MessageBox(NULL, __LOCALIZE("No items selected to analyze.","sws_analysis"), __LOCALIZE("SWS - Error","sws_analysis"), MB_OK);
}

void OrganizeByVol(COMMAND_T* ct)
{
	for (int iTrack = 1; iTrack <= GetNumTracks(); iTrack++)
	{
		WDL_TypedBuf<MediaItem*> items;
		SWS_GetSelectedMediaItemsOnTrack(&items, CSurf_TrackFromID(iTrack, false));
		if (items.GetSize() > 1)
		{
			double dStart = *(double*)GetSetMediaItemInfo(items.Get()[0], "D_POSITION", NULL);
			double* pVol = new double[items.GetSize()];
			ANALYZE_PCM a;
			memset(&a, 0, sizeof(a));
			if (ct->user == 2)
			{	// Windowed mode, set the window size
				char str[100];
				GetPrivateProfileString(SWS_INI, SWS_RMS_KEY, "-20,0.1", str, 100, get_ini_file());
				char* pWindow = strchr(str, ',');
				a.dWindowSize = pWindow ? atof(pWindow+1) : 0.1;
			}
			for (int i = 0; i < items.GetSize(); i++)
			{
				pVol[i] = -1.0;
				if (AnalyzeItem(items.Get()[i], &a))
					pVol[i] = ct->user ? a.dRMS : a.dPeakVal;
			}
			// Sort and arrange items from min to max RMS
			while (true)
			{
				int iItem = -1;
				double dMinVol = 1e99;
				for (int i = 0; i < items.GetSize(); i++)
					if (pVol[i] >= 0.0 && pVol[i] < dMinVol)
					{
						dMinVol = pVol[i];
						iItem = i;
					}
				if (iItem == -1)
					break;
				pVol[iItem] = -1.0;
				GetSetMediaItemInfo(items.Get()[iItem], "D_POSITION", &dStart);
				dStart += *(double*)GetSetMediaItemInfo(items.Get()[iItem], "D_LENGTH", NULL);
			}
			delete [] pVol;
			UpdateTimeline();
			Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(ct), UNDO_STATE_ITEMS, -1);
		}
	}
}

void RMSNormalize(double dTargetDb, double dWindowSize)
{
	WDL_TypedBuf<MediaItem*> items;
	SWS_GetSelectedMediaItems(&items);
	bool bDidWork = false;
	ANALYZE_PCM a;
	memset(&a, 0, sizeof(a));
	a.dWindowSize = dWindowSize;

	for (int i = 0; i < items.GetSize(); i++)
	{
		MediaItem* mi = items.Get()[i];
		MediaItem_Take* take = GetMediaItemTake(mi, -1);
		if (take && AnalyzeItem(mi, &a) && a.dRMS != 0.0)
		{
			bDidWork = true;
			double dVol = *(double*)GetSetMediaItemTakeInfo(take, "D_VOL", NULL);
			dVol *= DB2VAL(dTargetDb) / a.dRMS;
			GetSetMediaItemTakeInfo(take, "D_VOL", &dVol);
		}
	}
	if (bDidWork)
	{
		UpdateTimeline();
		Undo_OnStateChangeEx(__LOCALIZE("Normalize items to RMS","sws_undo"), UNDO_STATE_ITEMS, -1);
	}
}

void RMSNormalizeAll(double dTargetDb, double dWindowSize)
{
	WDL_TypedBuf<MediaItem*> items;
	SWS_GetSelectedMediaItems(&items);
	double dMaxRMS = -DBL_MAX;
	ANALYZE_PCM a;
	memset(&a, 0, sizeof(a));
	a.dWindowSize = dWindowSize;

	for (int i = 0; i < items.GetSize(); i++)
	{
		MediaItem* mi = items.Get()[i];
		MediaItem_Take* take = GetMediaItemTake(mi, -1);
		if (take && AnalyzeItem(mi, &a) && a.dRMS != 0.0 && a.dRMS > dMaxRMS)
			dMaxRMS = a.dRMS;
	}

	if (dMaxRMS > -DBL_MAX)
	{
		for (int i = 0; i < items.GetSize(); i++)
		{
			MediaItem* mi = items.Get()[i];
			MediaItem_Take* take = GetMediaItemTake(mi, -1);
			if (take)
			{
				double dVol = *(double*)GetSetMediaItemTakeInfo(take, "D_VOL", NULL);
				dVol *= DB2VAL(dTargetDb) / dMaxRMS;
				GetSetMediaItemTakeInfo(take, "D_VOL", &dVol);
			}
		}
		UpdateTimeline();
		Undo_OnStateChangeEx(__LOCALIZE("Normalize items to RMS","sws_undo"), UNDO_STATE_ITEMS, -1);
	}
}

// ct->user == 0 full item, otherwise windowed
void DoRMSNormalize(COMMAND_T* ct)
{
	char str[100];
	GetPrivateProfileString(SWS_INI, SWS_RMS_KEY, "-20,0.1", str, 100, get_ini_file());
	char* pWindow = strchr(str, ',');
	double dTarget = str[0] ? atof(str) : -20;
	double dWindow = pWindow ? atof(pWindow+1) : 0.1;

	if (ct->user != 2)
		RMSNormalize(dTarget, ct->user ? dWindow : 0.0);
	else
		RMSNormalizeAll(dTarget, dWindow);
}

void SetRMSOptions(COMMAND_T*)
{
	char reply[100];
	GetPrivateProfileString(SWS_INI, SWS_RMS_KEY, "-20,0.1", reply, 100, get_ini_file());
	if (GetUserInputs(__LOCALIZE("SWS RMS options","sws_analysis"), 2, __LOCALIZE("Target RMS normalize level (db),Window size for peak RMS (s)","sws_analysis"), reply, 100))
	{	// Do really basic input check
		if (strchr(reply, ',') && strlen(reply) > 2)
			WritePrivateProfileString(SWS_INI, SWS_RMS_KEY, reply, get_ini_file());
	}
}

//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
static COMMAND_T g_commandTable[] =
{
	{ { DEFACCEL, "SWS: Analyze and display item peak and RMS" },	"SWS_ANALYZEITEM",		DoAnalyzeItem,		NULL, },
	{ { DEFACCEL, "SWS: Move cursor to item peak sample" },			"SWS_FINDITEMPEAK",		FindItemPeak,		NULL, },
	{ { DEFACCEL, "SWS: Organize items by peak" },					"SWS_PEAKORGANIZE",		OrganizeByVol,		NULL, 0, },
	{ { DEFACCEL, "SWS: Organize items by RMS (entire item)" },		"SWS_RMSORGANIZE",		OrganizeByVol,		NULL, 1, },
	{ { DEFACCEL, "SWS: Organize items by peak RMS" },				"SWS_RMSPEAKORGANIZE",	OrganizeByVol,		NULL, 2, },
	{ { DEFACCEL, "SWS: Normalize items to RMS (entire item)" },	"SWS_NORMRMS",			DoRMSNormalize,		NULL, 0, },
	{ { DEFACCEL, "SWS: Normalize item(s) to peak RMS" },			"SWS_NORMPEAKRMS",		DoRMSNormalize,		NULL, 1, },
	{ { DEFACCEL, "SWS: Normalize items to overall peak RMS" },		"SWS_NORMPEAKRMSALL",	DoRMSNormalize,		NULL, 2, },
	{ { DEFACCEL, "SWS: Set RMS analysis/normalize options" },		"SWS_SETRMSOPTIONS",	SetRMSOptions,		NULL, },

	{ {}, LAST_COMMAND, }, // Denote end of table
};
//!WANT_LOCALIZE_1ST_STRING_END

int AnalysisInit()
{
	SWSRegisterCommands(g_commandTable);
	return 1;
}
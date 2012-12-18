/******************************************************************************
/ SnM_LiveConfigs.h
/
/ Copyright (c) 2010-2012 Jeffos
/ http://www.standingwaterstudios.com/reaper
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

//#pragma once

#ifndef _SNM_LIVECONFIGS_H_
#define _SNM_LIVECONFIGS_H_


class PresetMsg {
public:
	PresetMsg(int _fx=-1, const char* _preset="") : m_fx(_fx), m_preset(_preset) {}
	int m_fx;
	WDL_FastString m_preset;
};


class LiveConfigItem {
public:
	LiveConfigItem(int _cc, const char* _desc="", MediaTrack* _track=NULL, 
		const char* _trTemplate="", const char* _fxChain="", const char* _presets="", 
		const char* _onAction="", const char* _offAction="");
	LiveConfigItem(LiveConfigItem* _item);
	void Paste(LiveConfigItem* _item);
	bool IsDefault(bool _ignoreComment);
	void Clear(bool _trDataOnly = false);
	bool Equals(LiveConfigItem* _item, bool _ignoreComment);
	void GetInfo(WDL_FastString* _info);
	int m_cc;
	MediaTrack* m_track;
	WDL_FastString m_desc, m_trTemplate, m_fxChain, m_presets, m_onAction, m_offAction;
};


class LiveConfig {
public:
	LiveConfig();
	~LiveConfig() { m_ccConfs.Empty(true); }
	int SetInputTrack(MediaTrack* _newInputTr, bool _updateSends);
	bool IsLastConfiguredTrack(MediaTrack* _tr);
	MediaTrack* m_inputTr;
	WDL_PtrList<LiveConfigItem> m_ccConfs;
	int m_version, m_ccDelay, m_fade, m_enable, m_muteOthers, m_selScroll, m_offlineOthers, m_cc123, m_ignoreEmpty, m_autoSends;
	int m_activeMidiVal, m_curMidiVal, m_preloadMidiVal, m_curPreloadMidiVal;
};


class SNM_LiveConfigView : public SWS_ListView {
public:
	SNM_LiveConfigView(HWND hwndList, HWND hwndEdit);
protected:
	void GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax);
	void SetItemText(SWS_ListItem* item, int iCol, const char* str);
	void GetItemList(SWS_ListItemList* pList);
	void OnItemSelChanged(SWS_ListItem* item, int iState);
	void OnItemDblClk(SWS_ListItem* item, int iCol);
};


// edition window
class SNM_LiveConfigsWnd : public SWS_DockWnd {
public:
	SNM_LiveConfigsWnd();
	void Update();
	void OnCommand(WPARAM wParam, LPARAM lParam);
	void CSurfSetTrackTitle();
	void CSurfSetTrackListChange();
	void FillComboInputTrack();
	bool SelectByCCValue(int _configId, int _cc, bool _selectOnly = true);
protected:
	void OnInitDlg();
	void OnDestroy();
	INT_PTR OnUnhandledMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void AddFXSubMenu(HMENU _menu, MediaTrack* _tr, WDL_FastString* _curPresetConf);
	void AddLearnMenu(HMENU _menu, bool _subItems);
	void AddOptionsMenu(HMENU _menu, bool _subItems);
	HMENU OnContextMenu(int x, int y, bool* wantDefaultItems);
	int OnKey(MSG* msg, int iKeyState);
	void DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight = NULL);
	bool GetToolTipString(int _xpos, int _ypos, char* _bufOut, int _bufOutSz);
	bool Insert(int _dir = 1);

	WDL_PtrList_DeleteOnDestroy<PresetMsg> m_lastPresetMsg;
	WDL_VirtualComboBox m_cbConfig, m_cbInputTr;
	WDL_VirtualIconButton m_btnEnable;
	WDL_VirtualStaticText m_txtInputTr;
	SNM_ToolbarButton m_btnOptions, m_btnLearn, m_btnMonitor;
	SNM_Knob m_knobCC, m_knobFade;
	SNM_KnobCaption m_vwndCC, m_vwndFade;
};


// monitoring window (several instances)
class SNM_LiveConfigMonitorWnd : public SWS_DockWnd {
public:
	SNM_LiveConfigMonitorWnd(int _cfgId);
	void Update(int _whatFlags, int _commitFlags);
protected:
	void OnInitDlg();
	void OnDestroy();
	INT_PTR OnUnhandledMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight = NULL);
	int OnMouseDown(int xpos, int ypos);
	bool OnMouseUp(int xpos, int ypos);
	int m_cfgId;
	SNM_FiveMonitors m_mons;
	SNM_DynSizedText m_txtMon[5];
};


class ApplyLiveConfigJob : public SNM_MidiActionJob {
public:
	ApplyLiveConfigJob(int _jobId, int _approxDelayMs, int _val, int _valhw, int _relmode, HWND _hwnd, int _cfgId) 
		: SNM_MidiActionJob(_jobId,_approxDelayMs,_val,_valhw,_relmode,_hwnd), m_cfgId(_cfgId) {}
	void Perform();
	int m_cfgId;
};


class PreloadLiveConfigJob : public SNM_MidiActionJob {
public:
	PreloadLiveConfigJob(int _jobId, int _approxDelayMs, int _val, int _valhw, int _relmode, HWND _hwnd, int _cfgId) 
		: SNM_MidiActionJob(_jobId,_approxDelayMs,_val,_valhw,_relmode,_hwnd), m_cfgId(_cfgId)  {}
	void Perform();
	int m_cfgId;
};


class LiveConfigsUpdateJob : public SNM_ScheduledJob {
public:
	LiveConfigsUpdateJob() : SNM_ScheduledJob(SNM_SCHEDJOB_LIVECFG_UPDATE, 150) {}
	void Perform();
};

class LiveConfigsUndoJob : public SNM_ScheduledJob {
public:
	LiveConfigsUndoJob() : SNM_ScheduledJob(SNM_SCHEDJOB_LIVECFG_UNDO, SNM_SCHEDJOB_DEFAULT_DELAY) {}
	void Perform();
};

class LiveConfigsUpdateFadeJob : public SNM_ScheduledJob {
public:
	LiveConfigsUpdateFadeJob(int _value) : SNM_ScheduledJob(SNM_SCHEDJOB_LIVECFG_FADE_UPDATE, SNM_SCHEDJOB_DEFAULT_DELAY), m_value(_value) {}
	void Perform();
	int m_value;
};


int LiveConfigViewInit();
void LiveConfigViewExit();
void OpenLiveConfigView(COMMAND_T*);
bool IsLiveConfigViewDisplayed(COMMAND_T*);

void ApplyLiveConfig(int _cfgId, int _val, int _valhw, int _relmode, HWND _hwnd, bool _immediate = false);
void PreloadLiveConfig(int _cfgId, int _val, int _valhw, int _relmode, HWND _hwnd, bool _immediate = false);

void OpenLiveConfigMonitorWnd(COMMAND_T*);
bool IsLiveConfigMonitorWndDisplayed(COMMAND_T*);

// actions
void ApplyLiveConfig(MIDI_COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd);
void PreloadLiveConfig(MIDI_COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd);

void NextLiveConfig(COMMAND_T*);
void PreviousLiveConfig(COMMAND_T*);

bool IsLiveConfigEnabled(COMMAND_T*);
void UpdateEnableLiveConfig(int _cfgId, int _val);
void EnableLiveConfig(COMMAND_T*);
void DisableLiveConfig(COMMAND_T*);
void ToggleEnableLiveConfig(COMMAND_T*);

bool IsMuteOthersLiveConfigEnabled(COMMAND_T*);
void EnableMuteOthersLiveConfig(COMMAND_T*);
void DisableMuteOthersLiveConfig(COMMAND_T*);
void ToggleMuteOthersLiveConfig(COMMAND_T*);

bool IsOfflineOthersLiveConfigEnabled(COMMAND_T*);
void EnableOfflineOthersLiveConfig(COMMAND_T*);
void DisableOfflineOthersLiveConfig(COMMAND_T*);
void ToggleOfflineOthersLiveConfig(COMMAND_T*);

#endif

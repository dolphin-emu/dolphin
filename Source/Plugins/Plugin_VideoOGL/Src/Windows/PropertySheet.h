// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma once

#include "stdafx.h"
#include <vector>

namespace W32Util
{
	class PropSheet;

	class Tab
	{
	public:
		PropSheet *sheet; //back pointer ..
		virtual void Init(HWND hDlg) {}
		virtual void Command(HWND hDlg, WPARAM wParam) {}
		virtual void Apply(HWND hDlg) {}
		virtual bool HasPrev()   {return true;}
		virtual bool HasFinish() {return false;}
		virtual bool HasNext()   {return true;}
		static INT_PTR __stdcall TabDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	};

	
	class WizExteriorPage : public Tab
	{
		INT captionID;
	public:
		WizExteriorPage(INT caption) {captionID = caption;}
		void Init(HWND hDlg);
	};
	
	
	class WizFirstPage : public WizExteriorPage
	{
	public:
		WizFirstPage(INT caption) : WizExteriorPage(caption) {}
		bool HasPrev() {return false;}
	};


	class WizLastPage : public WizExteriorPage
	{
	public:
		WizLastPage(INT caption) : WizExteriorPage(caption) {}
		bool HasNext() {return false;}
		bool HasFinish() {return true;}
	};


	class WizInteriorPage : public Tab
	{
	public:
	};

	class PropSheet
	{
		LPCTSTR watermark;
		LPCTSTR header;
		HFONT hTitleFont;
		HFONT hDialogFont;
		HICON icon;
		struct Page 
		{
			Page(Tab *_tab, LPCTSTR _resource, LPCTSTR _title, LPCTSTR _subtitle = 0)
				: tab(_tab), resource(_resource), title(_title), hdrSubTitle(_subtitle) {}
			Tab *tab;
			LPCTSTR resource;
			LPCTSTR title;
			LPCTSTR hdrSubTitle;
		};
	public:
		PropSheet();
		typedef std::vector<Page> DlgList;
		DlgList list;
		void SetWaterMark(LPCTSTR _watermark) {watermark=_watermark;}
		void SetHeader(LPCTSTR _header) {header=_header;}
		void SetIcon(HICON _icon) {icon = _icon;}
		void Add(Tab *tab, LPCTSTR resource, LPCTSTR title, LPCTSTR subtitle = 0);
		void Show(HINSTANCE hInstance, HWND hParent, std::string title, int startpage=0, bool floating = false, bool wizard = false);
		HFONT GetTitleFont() {return hTitleFont;}
		HFONT GetFont() {return hDialogFont;}
		static int CALLBACK Callback(HWND hwndDlg, UINT uMsg, LPARAM lParam);
	};



}
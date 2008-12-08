#include "Misc.h"
#include "PropertySheet.h"

namespace W32Util
{
	bool centered;

	PropSheet::PropSheet()
	{
		watermark = 0;
		header = 0;
		icon = 0;
	}

	int CALLBACK PropSheet::Callback(HWND hwndDlg, UINT uMsg, LPARAM lParam)
	{
		switch (uMsg) {
		case PSCB_PRECREATE:
			{
				if (uMsg == PSCB_PRECREATE) 
				{
					/*
					if (lParam)
					{
						DLGTEMPLATE *pDlgTemplate;
						DLGTEMPLATEEX *pDlgTemplateEx;

						pDlgTemplateEx = (DLGTEMPLATEEX *)lParam;
						if (pDlgTemplateEx->signature == 0xFFFF)
						{
							// pDlgTemplateEx points to an extended  
							// dialog template structure.

							//pDlgTemplate->style |= DS_SETFONT;
							u8 *tmp1 = (u8*)&pDlgTemplateEx + sizeof(DLGTEMPLATEEX);
							u16 *tmp = (u16*)tmp1;
							tmp++; //skip menu
							tmp++; //skip dlg class
							//Crash();
							//Here we should bash in Segoe UI
							//It turns out to be way complicated though
							//Not worth it
						}
						else
						{
							// This is a standard dialog template
							//  structure.
							pDlgTemplate = (DLGTEMPLATE *)lParam;
						}
					} */   
				}

			}
			break;
		case PSCB_INITIALIZED:
			{
			}
			return 0;
		}
		return 0;
	}
	 
	void PropSheet::Show(HINSTANCE hInstance, HWND hParent, std::string title, int startpage, bool floating, bool wizard)
	{	
		HPROPSHEETPAGE *pages = new HPROPSHEETPAGE[list.size()];
		PROPSHEETPAGE page;
		//common settings
		memset((void*)&page,0,sizeof(PROPSHEETPAGE));
		page.dwSize = sizeof(PROPSHEETPAGE);
		page.hInstance = hInstance;

		int i=0;
		for (DlgList::iterator iter = list.begin(); iter != list.end(); iter++, i++)
		{
			if (wizard)
			{
				if (i == 0 || i == list.size()-1)
					page.dwFlags = PSP_HIDEHEADER;
				else
					page.dwFlags = PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
			}
			else
			{
				page.dwFlags = PSP_USETITLE;
			}
			page.pszTemplate = iter->resource;
			page.pfnDlgProc = Tab::TabDlgProc; 
			page.pszTitle = iter->title;
			page.pszHeaderTitle = wizard?iter->title:0;
			page.pszHeaderSubTitle = wizard?iter->hdrSubTitle:0;
			page.lParam = (LPARAM)iter->tab;
			pages[i] = CreatePropertySheetPage(&page);
		}

		PROPSHEETHEADER sheet;
		memset(&sheet,0,sizeof(sheet));
		sheet.dwSize = sizeof(PROPSHEETHEADER);
		sheet.hInstance = hInstance;
		sheet.hwndParent = hParent;
		sheet.pszbmWatermark = watermark;
		sheet.pszbmHeader = header;
		
		if (icon)
			sheet.hIcon = icon;

		if (wizard)
			sheet.dwFlags = PSH_USECALLBACK | PSH_WIZARD97 | (watermark?PSH_WATERMARK:0) | (header?PSH_HEADER:0);
		else
			sheet.dwFlags = PSH_USECALLBACK | PSH_PROPTITLE;

		sheet.dwFlags |= PSH_NOCONTEXTHELP;

		if (floating)
			sheet.dwFlags |= PSH_MODELESS;
		//else
		//	sheet.dwFlags |= PSH_NOAPPLYNOW;

		if (icon) 
			sheet.dwFlags |= PSH_USEHICON;

		sheet.pszCaption = title.c_str();
		sheet.nPages = (UINT)list.size();
		sheet.phpage = pages;
		sheet.nStartPage = startpage;
		sheet.pfnCallback = (PFNPROPSHEETCALLBACK)Callback;

		NONCLIENTMETRICS ncm = {0};
		ncm.cbSize = sizeof(ncm);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0);
		hDialogFont = CreateFontIndirect(&ncm.lfMessageFont);
		
		if (wizard)
		{

			//Create the intro/end title font
			LOGFONT TitleLogFont = ncm.lfMessageFont;
			TitleLogFont.lfWeight = FW_BOLD;
			lstrcpy(TitleLogFont.lfFaceName, TEXT("Verdana Bold"));
			//StringCchCopy(TitleLogFont.lfFaceName, 32, TEXT("Verdana Bold"));

			HDC hdc = GetDC(NULL); //gets the screen DC
			int FontSize = 12;
			TitleLogFont.lfHeight = 0 - GetDeviceCaps(hdc, LOGPIXELSY) * FontSize / 72;
			hTitleFont = CreateFontIndirect(&TitleLogFont);
			ReleaseDC(NULL, hdc);
		}
		else
			hTitleFont = 0;

		centered=false;
		PropertySheet(&sheet);
		if (!floating)
		{
			for (DlgList::iterator iter = list.begin(); iter != list.end(); iter++)
			{
				delete iter->tab;
			}
			DeleteObject(hTitleFont);
		}
		DeleteObject(hDialogFont);
		delete [] pages;
	}
	void PropSheet::Add(Tab *tab, LPCTSTR resource, LPCTSTR title, LPCTSTR subtitle)
	{
		tab->sheet = this;
		list.push_back(Page(tab,resource,title,subtitle));
	}


	void WizExteriorPage::Init(HWND hDlg)
	{
        HWND hwndControl = GetDlgItem(hDlg, captionID);
        //SetWindowFont(hwndControl, sheet->GetTitleFont(), TRUE);
		SendMessage(hwndControl,WM_SETFONT,(WPARAM)sheet->GetTitleFont(),0);
	}

	INT_PTR Tab::TabDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		Tab *tab = (Tab *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
		switch(message)
		{
		case WM_INITDIALOG:
			{
				if (!centered) //HACK
				{
					CenterWindow(GetParent(hDlg));
					centered=true;
				}
				LPARAM l = ((LPPROPSHEETPAGE)lParam)->lParam;
				tab = (Tab *)l;
				SetWindowLongPtr(hDlg, GWLP_USERDATA, (DWORD_PTR)l);
				tab->Init(hDlg);
			}
			break;

		case WM_COMMAND:
			tab->Command(hDlg,wParam);
			break;
		case WM_NOTIFY:
			{
				LPPSHNOTIFY lppsn = (LPPSHNOTIFY) lParam;
				HWND sheet = lppsn->hdr.hwndFrom;
				switch(lppsn->hdr.code) {
				case PSN_APPLY:
					tab->Apply(hDlg);
					break;
				case PSN_SETACTIVE:
					PropSheet_SetWizButtons(GetParent(hDlg), 
						(tab->HasPrev()?PSWIZB_BACK:0) | 
						(tab->HasNext()?PSWIZB_NEXT:0) | 
						(tab->HasFinish()?PSWIZB_FINISH:0));
					break;
				case PSN_WIZNEXT: 
					tab->Apply(hDlg); //maybe not always good
					break;
				case PSN_WIZBACK:
				case PSN_RESET: //cancel
					break;
				}
			}
			break;
		}
		return 0;
	}
}
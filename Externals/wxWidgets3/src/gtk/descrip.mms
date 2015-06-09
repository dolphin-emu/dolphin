#*****************************************************************************
#                                                                            *
# Make file for VMS                                                          *
# Author : J.Jansen (joukj@hrem.nano.tudelft.nl)                             *
# Date : 6 November 2012                                                     *
#                                                                            *
#*****************************************************************************
.first
	define wx [--.include.wx]

.ifdef __WXUNIVERSAL__
CXX_DEFINE = /define=(__WXGTK__=1,__WXUNIVERSAL__==1)/float=ieee\
	/name=(as_is,short)/ieee=denorm/assume=(nostdnew,noglobal_array_new)
CC_DEFINE = /define=(__WXGTK__=1,__WXUNIVERSAL__==1)/float=ieee\
	/name=(as_is,short)/ieee=denorm
.else
.ifdef __WXGTK2__
CXX_DEFINE = /define=(__WXGTK__=1,VMS_GTK2==1)/float=ieee\
	/name=(as_is,short)/ieee=denorm/assume=(nostdnew,noglobal_array_new)
CC_DEFINE = /define=(__WXGTK__=1,VMS_GTK2==1)/float=ieee\
	/name=(as_is,short)/ieee=denorm
.else
CXX_DEFINE = /define=(__WXGTK__=1)/float=ieee/name=(as_is,short)/iee=denorm\
	   /assume=(nostdnew,noglobal_array_new)
CC_DEFINE = /define=(__WXGTK__=1)/float=ieee/name=(as_is,short)/iee=denorm
.endif
.endif

.suffixes : .cpp

.cpp.obj :
	cxx $(CXXFLAGS)$(CXX_DEFINE) $(MMS$TARGET_NAME).cpp
.c.obj :
	cc $(CFLAGS)$(CC_DEFINE) $(MMS$TARGET_NAME).c

OBJECTS = \
	animate.obj,\
	app.obj,\
	artgtk.obj,\
	bitmap.obj,\
	brush.obj,\
	clipbrd.obj,\
	colordlg.obj,\
	colour.obj,\
	collpane.obj,\
	cursor.obj,\
	dataobj.obj,\
	dc.obj,\
	dcclient.obj,\
	dcmemory.obj,\
	dcscreen.obj,\
        dnd.obj,\
        evtloop.obj,\
	filedlg.obj,\
	font.obj,\
        glcanvas.obj,\
	sockgtk.obj,\
	minifram.obj,\
	pen.obj,\
	popupwin.obj,\
	renderer.obj,\
	region.obj,\
	settings.obj,\
	timer.obj,\
	tooltip.obj,\
	toplevel.obj,\
	utilsgtk.obj,\
	window.obj

OBJECTS0= \
        bmpbuttn.obj,\
	button.obj,\
	checkbox.obj,\
	checklst.obj,\
	choice.obj,\
        combobox.obj,\
	control.obj,\
	dialog.obj,\
	fontdlg.obj,\
	frame.obj,\
	gauge.obj,\
	listbox.obj,\
	mdi.obj,\
	menu.obj,\
	notebook.obj,\
	radiobox.obj,\
	radiobut.obj,\
	scrolbar.obj,\
	scrolwin.obj,\
	slider.obj,\
        spinbutt.obj,\
	spinctrl.obj,\
	statbmp.obj,\
	statbox.obj,\
	statline.obj,\
	stattext.obj,\
	toolbar.obj,\
	textctrl.obj,\
	tglbtn.obj,\
	msgdlg.obj,\
	treeentry_gtk.obj,textentry.obj,filectrl.obj,print.obj,win_gtk.obj,\
	mnemonics.obj,private.obj,assertdlg_gtk.obj,infobar.obj,anybutton.obj,\
	nonownedwnd.obj,textmeasure.obj

SOURCES =\
	animate.cpp,\
	app.cpp,\
	artgtk.cpp, \
	bitmap.cpp,\
        bmpbuttn.cpp,\
	brush.cpp,\
	button.cpp,\
	checkbox.cpp,\
	checklst.cpp,\
	choice.cpp,\
	clipbrd.cpp,\
	colordlg.cpp,\
	colour.cpp,\
	collpane.cpp,\
        combobox.cpp,\
	control.cpp,\
	cursor.cpp,\
	dataobj.cpp,\
	dc.cpp,\
	dcclient.cpp,\
	dcmemory.cpp,\
	dcscreen.cpp,\
	dialog.cpp,\
        dnd.cpp,\
        evtloop.cpp,\
	filedlg.cpp,\
	font.cpp,\
	fontdlg.cpp,\
	frame.cpp,\
	gauge.cpp,\
        glcanvas.cpp,\
	sockgtk.cpp,\
	listbox.cpp,\
	mdi.cpp,\
	menu.cpp,\
	minifram.cpp,\
	msgdlg.cpp,\
	notebook.cpp,\
	pen.cpp,\
	popupwin.cpp,\
	radiobox.cpp,\
	radiobut.cpp,\
	renderer.cpp,\
	region.cpp,\
	scrolbar.cpp,\
	scrolwin.cpp,\
	settings.cpp,\
	slider.cpp,\
        spinbutt.cpp,\
	spinctrl.cpp,\
	statbmp.cpp,\
	statbox.cpp,\
	statline.cpp,\
	stattext.cpp,\
	toolbar.cpp,\
	textctrl.cpp,\
	tglbtn.cpp,\
	timer.cpp,\
	tooltip.cpp,\
	toplevel.cpp,\
	utilsgtk.cpp,\
	window.cpp,\
	treeentry_gtk.c,textentry.cpp,filectrl.cpp,print.cpp,win_gtk.cpp,\
	mnemonics.cpp,private.cpp,assertdlg_gtk.cpp,infobar.cpp,anybutton.cpp,\
	nonownedwnd.cpp,textmeasure.cpp

all : $(SOURCES)
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS)
.ifdef __WXUNIVERSAL__
	library [--.lib]libwx_gtk_univ.olb $(OBJECTS)
	If f$getsyi("HW_MODEL") .le. 2048 then library [--.lib]libwx_gtk_univ.olb [.CXX_REPOSITORY]*.obj
.else
.ifdef __WXGTK2__
	library [--.lib]libwx_gtk2.olb $(OBJECTS)
	If f$getsyi("HW_MODEL") .le. 2048 then library [--.lib]libwx_gtk2.olb [.CXX_REPOSITORY]*.obj
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS0)
	library [--.lib]libwx_gtk2.olb $(OBJECTS0)
.else
	library [--.lib]libwx_gtk.olb $(OBJECTS)
	If f$getsyi("HW_MODEL") .le. 2048 then library [--.lib]libwx_gtk.olb [.CXX_REPOSITORY]*.obj
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS0)
	library [--.lib]libwx_gtk.olb $(OBJECTS0)
.endif
.endif

$(OBJECTS) : [--.include.wx]setup.h
$(OBJECTS0) : [--.include.wx]setup.h

animate.obj : animate.cpp
app.obj : app.cpp
artgtk.obj : artgtk.cpp
bitmap.obj : bitmap.cpp
bmpbuttn.obj : bmpbuttn.cpp
brush.obj : brush.cpp
button.obj : button.cpp
checkbox.obj : checkbox.cpp
checklst.obj : checklst.cpp
choice.obj : choice.cpp
clipbrd.obj :clipbrd.cpp
colordlg.obj : colordlg.cpp
colour.obj : colour.cpp
collpane.obj : collpane.cpp
combobox.obj : combobox.cpp
control.obj : control.cpp
cursor.obj : cursor.cpp
dataobj.obj : dataobj.cpp
dc.obj : dc.cpp
dcclient.obj : dcclient.cpp
dcmemory.obj : dcmemory.cpp
dcscreen.obj : dcscreen.cpp
dialog.obj : dialog.cpp
dnd.obj : dnd.cpp
evtloop.obj : evtloop.cpp
filedlg.obj : filedlg.cpp
font.obj : font.cpp
fontdlg.obj : fontdlg.cpp
frame.obj : frame.cpp
gauge.obj : gauge.cpp
glcanvas.obj : glcanvas.cpp
sockgtk.obj : sockgtk.cpp
listbox.obj : listbox.cpp
msgdlg.obj : msgdlg.cpp
mdi.obj : mdi.cpp
menu.obj : menu.cpp
minifram.obj : minifram.cpp
notebook.obj : notebook.cpp
pen.obj : pen.cpp
popupwin.obj : popupwin.cpp
radiobox.obj : radiobox.cpp
radiobut.obj : radiobut.cpp
renderer.obj : renderer.cpp
region.obj : region.cpp
scrolbar.obj : scrolbar.cpp
scrolwin.obj : scrolwin.cpp
settings.obj : settings.cpp
slider.obj : slider.cpp
spinbutt.obj : spinbutt.cpp
spinctrl.obj : spinctrl.cpp
statbmp.obj : statbmp.cpp
statbox.obj : statbox.cpp
statline.obj : statline.cpp
stattext.obj : stattext.cpp
toolbar.obj : toolbar.cpp
textctrl.obj : textctrl.cpp
tglbtn.obj : tglbtn.cpp
timer.obj : timer.cpp
tooltip.obj : tooltip.cpp
toplevel.obj : toplevel.cpp
utilsgtk.obj : utilsgtk.cpp
window.obj : window.cpp
treeentry_gtk.obj : treeentry_gtk.c
	cc $(CFLAGS)$(CC_DEFINE)/warn=disab=CHAROVERFL $(MMS$TARGET_NAME).c
textentry.obj : textentry.cpp
filectrl.obj : filectrl.cpp
print.obj : print.cpp
win_gtk.obj : win_gtk.cpp
mnemonics.obj : mnemonics.cpp
private.obj : private.cpp
assertdlg_gtk.obj : assertdlg_gtk.cpp
infobar.obj : infobar.cpp
anybutton.obj : anybutton.cpp
nonownedwnd.obj : nonownedwnd.cpp
textmeasure.obj : textmeasure.cpp

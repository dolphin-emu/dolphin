/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QPLATFORMTHEME_H
#define QPLATFORMTHEME_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//

#include <QtGui/qtguiglobal.h>
#include <QtCore/QScopedPointer>
#include <QtGui/QKeySequence>

QT_BEGIN_NAMESPACE

class QIcon;
class QIconEngine;
class QMenu;
class QMenuBar;
class QPlatformMenuItem;
class QPlatformMenu;
class QPlatformMenuBar;
class QPlatformDialogHelper;
class QPlatformSystemTrayIcon;
class QPlatformThemePrivate;
class QVariant;
class QPalette;
class QFont;
class QPixmap;
class QSizeF;
class QFileInfo;

class Q_GUI_EXPORT QPlatformTheme
{
    Q_DECLARE_PRIVATE(QPlatformTheme)
public:
    Q_DISABLE_COPY_MOVE(QPlatformTheme)

    enum ThemeHint {
        CursorFlashTime,
        KeyboardInputInterval,
        MouseDoubleClickInterval,
        StartDragDistance,
        StartDragTime,
        KeyboardAutoRepeatRate,
        PasswordMaskDelay,
        StartDragVelocity,
        TextCursorWidth,
        DropShadow,
        MaximumScrollBarDragDistance,
        ToolButtonStyle,
        ToolBarIconSize,
        ItemViewActivateItemOnSingleClick,
        SystemIconThemeName,
        SystemIconFallbackThemeName,
        IconThemeSearchPaths,
        StyleNames,
        WindowAutoPlacement,
        DialogButtonBoxLayout,
        DialogButtonBoxButtonsHaveIcons,
        UseFullScreenForPopupMenu,
        KeyboardScheme,
        UiEffects,
        SpellCheckUnderlineStyle,
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        TabFocusBehavior,
#else
        TabAllWidgets,
        TabFocusBehavior = TabAllWidgets,
#endif
        IconPixmapSizes,
        PasswordMaskCharacter,
        DialogSnapToDefaultButton,
        ContextMenuOnMouseRelease,
        MousePressAndHoldInterval,
        MouseDoubleClickDistance,
        WheelScrollLines,
        TouchDoubleTapDistance,
        ShowShortcutsInContextMenus,
        IconFallbackSearchPaths,
        MouseQuickSelectionThreshold
    };

    enum DialogType {
        FileDialog,
        ColorDialog,
        FontDialog,
        MessageDialog
    };

    enum Palette {
        SystemPalette,
        ToolTipPalette,
        ToolButtonPalette,
        ButtonPalette,
        CheckBoxPalette,
        RadioButtonPalette,
        HeaderPalette,
        ComboBoxPalette,
        ItemViewPalette,
        MessageBoxLabelPelette,
        MessageBoxLabelPalette = MessageBoxLabelPelette,
        TabBarPalette,
        LabelPalette,
        GroupBoxPalette,
        MenuPalette,
        MenuBarPalette,
        TextEditPalette,
        TextLineEditPalette,
        NPalettes
    };

    enum Font {
        SystemFont,
        MenuFont,
        MenuBarFont,
        MenuItemFont,
        MessageBoxFont,
        LabelFont,
        TipLabelFont,
        StatusBarFont,
        TitleBarFont,
        MdiSubWindowTitleFont,
        DockWidgetTitleFont,
        PushButtonFont,
        CheckBoxFont,
        RadioButtonFont,
        ToolButtonFont,
        ItemViewFont,
        ListViewFont,
        HeaderViewFont,
        ListBoxFont,
        ComboMenuItemFont,
        ComboLineEditFont,
        SmallFont,
        MiniFont,
        FixedFont,
        GroupBoxTitleFont,
        TabButtonFont,
        EditorFont,
        NFonts
    };

    enum StandardPixmap {  // Keep in sync with QStyle::StandardPixmap
        TitleBarMenuButton,
        TitleBarMinButton,
        TitleBarMaxButton,
        TitleBarCloseButton,
        TitleBarNormalButton,
        TitleBarShadeButton,
        TitleBarUnshadeButton,
        TitleBarContextHelpButton,
        DockWidgetCloseButton,
        MessageBoxInformation,
        MessageBoxWarning,
        MessageBoxCritical,
        MessageBoxQuestion,
        DesktopIcon,
        TrashIcon,
        ComputerIcon,
        DriveFDIcon,
        DriveHDIcon,
        DriveCDIcon,
        DriveDVDIcon,
        DriveNetIcon,
        DirOpenIcon,
        DirClosedIcon,
        DirLinkIcon,
        DirLinkOpenIcon,
        FileIcon,
        FileLinkIcon,
        ToolBarHorizontalExtensionButton,
        ToolBarVerticalExtensionButton,
        FileDialogStart,
        FileDialogEnd,
        FileDialogToParent,
        FileDialogNewFolder,
        FileDialogDetailedView,
        FileDialogInfoView,
        FileDialogContentsView,
        FileDialogListView,
        FileDialogBack,
        DirIcon,
        DialogOkButton,
        DialogCancelButton,
        DialogHelpButton,
        DialogOpenButton,
        DialogSaveButton,
        DialogCloseButton,
        DialogApplyButton,
        DialogResetButton,
        DialogDiscardButton,
        DialogYesButton,
        DialogNoButton,
        ArrowUp,
        ArrowDown,
        ArrowLeft,
        ArrowRight,
        ArrowBack,
        ArrowForward,
        DirHomeIcon,
        CommandLink,
        VistaShield,
        BrowserReload,
        BrowserStop,
        MediaPlay,
        MediaStop,
        MediaPause,
        MediaSkipForward,
        MediaSkipBackward,
        MediaSeekForward,
        MediaSeekBackward,
        MediaVolume,
        MediaVolumeMuted,
        LineEditClearButton,
        // do not add any values below/greater than this
        CustomBase = 0xf0000000
    };

    enum KeyboardSchemes
    {
        WindowsKeyboardScheme,
        MacKeyboardScheme,
        X11KeyboardScheme,
        KdeKeyboardScheme,
        GnomeKeyboardScheme,
        CdeKeyboardScheme
    };

    enum UiEffect
    {
        GeneralUiEffect = 0x1,
        AnimateMenuUiEffect = 0x2,
        FadeMenuUiEffect = 0x4,
        AnimateComboUiEffect = 0x8,
        AnimateTooltipUiEffect = 0x10,
        FadeTooltipUiEffect = 0x20,
        AnimateToolBoxUiEffect = 0x40,
        HoverEffect = 0x80
    };

    enum IconOption {
        DontUseCustomDirectoryIcons = 0x01
    };
    Q_DECLARE_FLAGS(IconOptions, IconOption)

    explicit QPlatformTheme();
    virtual ~QPlatformTheme();

    virtual QPlatformMenuItem* createPlatformMenuItem() const;
    virtual QPlatformMenu* createPlatformMenu() const;
    virtual QPlatformMenuBar* createPlatformMenuBar() const;
    virtual void showPlatformMenuBar() {}

    virtual bool usePlatformNativeDialog(DialogType type) const;
    virtual QPlatformDialogHelper *createPlatformDialogHelper(DialogType type) const;

#ifndef QT_NO_SYSTEMTRAYICON
    virtual QPlatformSystemTrayIcon *createPlatformSystemTrayIcon() const;
#endif

    virtual const QPalette *palette(Palette type = SystemPalette) const;

    virtual const QFont *font(Font type = SystemFont) const;

    virtual QVariant themeHint(ThemeHint hint) const;

    virtual QPixmap standardPixmap(StandardPixmap sp, const QSizeF &size) const;
    virtual QIcon fileIcon(const QFileInfo &fileInfo,
                           QPlatformTheme::IconOptions iconOptions = { }) const;
    virtual QIconEngine *createIconEngine(const QString &iconName) const;

#ifndef QT_NO_SHORTCUT
    virtual QList<QKeySequence> keyBindings(QKeySequence::StandardKey key) const;
#endif

    virtual QString standardButtonText(int button) const;
    virtual QKeySequence standardButtonShortcut(int button) const;

    static QVariant defaultThemeHint(ThemeHint hint);
    static QString defaultStandardButtonText(int button);
    static QString removeMnemonics(const QString &original);

protected:
    explicit QPlatformTheme(QPlatformThemePrivate *priv);
    QScopedPointer<QPlatformThemePrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QPLATFORMTHEME_H

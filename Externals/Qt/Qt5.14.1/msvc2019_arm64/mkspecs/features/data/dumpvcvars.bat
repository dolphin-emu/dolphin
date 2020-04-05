:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::
:: Copyright (C) 2018 The Qt Company Ltd.
:: Contact: https://www.qt.io/licensing/
::
:: This file is part of the tools applications of the Qt Toolkit.
::
:: $QT_BEGIN_LICENSE:GPL-EXCEPT$
:: Commercial License Usage
:: Licensees holding valid commercial Qt licenses may use this file in
:: accordance with the commercial license agreement provided with the
:: Software or, alternatively, in accordance with the terms contained in
:: a written agreement between you and The Qt Company. For licensing terms
:: and conditions see https://www.qt.io/terms-conditions. For further
:: information use the contact form at https://www.qt.io/contact-us.
::
:: GNU General Public License Usage
:: Alternatively, this file may be used under the terms of the GNU
:: General Public License version 3 as published by the Free Software
:: Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
:: included in the packaging of this file. Please review the following
:: information to ensure the GNU General Public License requirements will
:: be met: https://www.gnu.org/licenses/gpl-3.0.html.
::
:: $QT_END_LICENSE$
::
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

@echo off

REM We clear INCLUDE and LIB, because we want to obtain pristine values.
REM PATH cannot be cleared, because then the script does not even run,
REM and it would be counterproductive anyway (see toolchain.prf).
set INCLUDE=
set LIB=

call %* || exit 1
REM VS2015 does not set errorlevel in case of failure.
if "%INCLUDE%" == "" exit 1

echo =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=
echo %INCLUDE%
echo %LIB%
echo %PATH%

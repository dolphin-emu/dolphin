#!/bin/bash

#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is the build configuration utility of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU Lesser General Public License version 3 requirements
## will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 2.0 or (at your option) the GNU General
## Public license version 3 or any later version approved by the KDE Free
## Qt Foundation. The licenses are as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-2.0.html and
## https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
scheme=$1
shift
booted_simulator=$($DIR/devices.py --state booted $@ | tail -n 1)
echo "SIMULATOR_DEVICES = $booted_simulator"

xcodebuild test -scheme $scheme -destination 'id=0' -destination-timeout 1 2>&1| sed -n 's/{ \(platform:.*\) }/\1/p' | while read destination; do
    id=$(echo $destination | sed -n -E 's/.*id:([^ ,]+).*/\1/p')
    [[ $id == *"placeholder"* ]] && continue

    echo $destination | tr ',' '\n' | while read keyval; do
        key=$(echo $keyval | cut -d ':' -f 1 | tr '[:lower:]' '[:upper:]')
        val=$(echo $keyval | cut -d ':' -f 2)
        echo "%_$id: DESTINATION_${key} = $val"

        if [ $key = 'PLATFORM' ]; then
            if [ "$val" = "iOS" ]; then
                echo "HARDWARE_DEVICES += $id"
            elif [ "$val" = "iOS Simulator" -a "$id" != "$booted_simulator" ]; then
                echo "SIMULATOR_DEVICES += $id"
            elif [ "$val" = "tvOS" ]; then
                echo "HARDWARE_DEVICES += $id"
            elif [ "$val" = "tvOS Simulator" -a "$id" != "$booted_simulator" ]; then
                echo "SIMULATOR_DEVICES += $id"
            elif [ "$val" = "watchOS" ]; then
                echo "HARDWARE_DEVICES += $id"
            elif [ "$val" = "watchOS Simulator" -a "$id" != "$booted_simulator" ]; then
                echo "SIMULATOR_DEVICES += $id"
            fi
        fi
    done
    echo
done

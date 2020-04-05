#!/bin/bash

#############################################################################
##
## Copyright (C) 2017 The Qt Company Ltd.
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

script_argument_prefix="-Wobjc_namespace,--"

required_arguments="target suffix original_ld"
optional_arguments="exclude_list exclude_regex slient"

for argument in $required_arguments $optional_arguments; do
    declare "$argument="
done

declare -i silent=0
declare -a linker_arguments

for i in "$@"; do
    case $1 in
        $script_argument_prefix*)
            declare "${1#$script_argument_prefix}"
            ;;
        -o)
            if [ -n "$2" ]; then
                target="$2"
            fi
            linker_arguments+=("$1")
            ;;
        *)
            linker_arguments+=("$1")
            ;;
    esac
    shift
done

get_entry() {
    local map=$1 key=$2
    local i="${map}_map_${2}"
    printf '%s' "${!i}"
}

error() {
    echo "$0: error: $*" >&2
    exit 1
}

for argument in $required_arguments; do
    if [ -z "${!argument}" ]; then
        error "missing argument --${argument}"
    fi
done

# Normalize suffix so we can use it as a bash variable
suffix=${suffix//[-. ]/_}

link_binary() {
    (PS4=; test $silent -ne 1 && set -x; $original_ld "${linker_arguments[@]}" "$@") 2>&1 || exit 1
}

sanitize_address() {
    local address="$1"
    address=${address#0x} # Remove hex prefix
    address=${address: ${#address} < 8 ? 0 : -8} # Limit to 32-bit
    echo "0x$address"
}

read_binary() {
    local address=$1
    local length=$2

    dd if="$target" bs=1 iseek=$address count=$length 2>|/dev/null
}

read_32bit_value() {
    local address=$1
    read_binary $address 4 | xxd -p | dd conv=swab 2>/dev/null | rev
}

inspect_binary() {
    inspect_mode="$1"

    echo -n "🔎  Inspecting binary '$target', "
    if [ ! -f "$target" ]; then
        echo "target does not exist!"
        exit 1
    fi

    read -a mach_header <<< "$(otool -h "$target" -v | tail -n 1)"
    if [ "${mach_header[1]}" != "X86_64" ]; then
        echo "binary is not 64-bit, only 64-bit binaries are supported!"
        exit 1
    fi

    classnames_section="__objc_classname"
    classnames=$(otool -v -s __TEXT $classnames_section "$target" | tail -n +3)
    while read -a classname; do
        address=$(sanitize_address ${classname[0]})
        name=${classname[1]}

        declare "address_to_classname_map_$address=$name"
        declare "classname_to_address_map_$name=$address"
    done <<< "$classnames"

    extra_classnames_file="$(mktemp -t ${classnames_section}_additions).S"

    if [ "$inspect_mode" == "inject_classnames" ]; then
        echo "class names have not been namespaced, adding suffix '$suffix'..."
        printf ".section __TEXT,$classnames_section,cstring_literals,no_dead_strip\n" > $extra_classnames_file
    elif [ "$inspect_mode" == "patch_classes" ]; then
        echo "found namespaced class names, updating class entries..."
    fi

    classes=$(otool -o -v "$target" | grep class_ro_t)
    while read -a class; do
        address="$(sanitize_address ${class[1]})"

        class_flags="0x$(read_32bit_value $address)"
        if [ -z "$class_flags" ]; then
            echo " 💥  failed to read class flags for class at $address"
            continue
        fi

        is_metaclass=$(($class_flags & 0x1))

        name_offset=$(($address + 24))
        classname_address="0x$(read_32bit_value $name_offset)"
        if [ -z "$classname_address" ]; then
            echo " 💥  failed to read class name address for class at $address"
            continue
        fi

        classname=$(get_entry address_to_classname $classname_address)
        if [ -z "$classname" ]; then
            echo " 💥  failed to resolve class name for address '$classname_address'"
            continue
        fi

        if [[ $exclude_list =~ $classname || $classname =~ $exclude_regex ]]; then
            if [ $is_metaclass -eq 1 ]; then
                class_type="meta class"
            else
                class_type="class"
            fi
            echo " 🚽  skipping excluded $class_type '$classname'"
            continue
        fi

        newclassname="${classname}_${suffix}"

        if [ "$inspect_mode" == "inject_classnames" ]; then
            if [ $is_metaclass -eq 1 ]; then
                continue
            fi

            echo " 💉  injecting $classnames_section entry '$newclassname' for '$classname'"
            printf ".asciz \"$newclassname\"\n" >> $extra_classnames_file

        elif [ "$inspect_mode" == "patch_classes" ]; then
            newclassname_address=$(get_entry classname_to_address ${newclassname})
            if [ -z "$newclassname_address" ]; then
                echo " 💥  failed to resolve class name address for class '$newclassname'"
                continue
            fi

            if [ $is_metaclass -eq 1 ]; then
                class_type="meta"
            else
                class_type="class"
            fi

            echo " 🔨  patching class_ro_t at $address ($class_type) from $classname_address ($classname) to $newclassname_address ($newclassname)"
            echo ${newclassname_address: -8} | rev | dd conv=swab 2>/dev/null | xxd -p -r -seek $name_offset -l 4 - "$target"
        fi
    done <<< "$classes"
}

echo "🔩  Linking binary using '$original_ld'..."
link_binary

inspect_binary inject_classnames

echo "🔩  Re-linking binary with extra __objc_classname section..."
link_binary $extra_classnames_file

inspect_binary patch_classes


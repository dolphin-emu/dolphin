// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui

class RiivolutionItem {
    val mDiscIndex: Int
    val mSectionIndex: Int
    val mOptionIndex: Int

    /**
     * Constructor for a disc.
     */
    constructor(discIndex: Int) {
        mDiscIndex = discIndex
        mSectionIndex = -1
        mOptionIndex = -1
    }

    /**
     * Constructor for a section.
     */
    constructor(discIndex: Int, sectionIndex: Int) {
        mDiscIndex = discIndex
        mSectionIndex = sectionIndex
        mOptionIndex = -1
    }

    /**
     * Constructor for an option.
     */
    constructor(discIndex: Int, sectionIndex: Int, optionIndex: Int) {
        mDiscIndex = discIndex
        mSectionIndex = sectionIndex
        mOptionIndex = optionIndex
    }
}

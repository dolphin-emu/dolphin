// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.model

import androidx.annotation.Keep

class RiivolutionPatches(
    private val gameId: String,
    private val revision: Int,
    private val discNumber: Int
) {
    private var unsavedChanges = false

    @Keep
    private val pointer: Long = initialize()

    external fun finalize()

    external fun getDiscCount(): Int

    external fun getDiscName(discIndex: Int): String

    external fun getSectionCount(discIndex: Int): Int

    external fun getSectionName(discIndex: Int, sectionIndex: Int): String

    external fun getOptionCount(discIndex: Int, sectionIndex: Int): Int

    external fun getOptionName(discIndex: Int, sectionIndex: Int, optionIndex: Int): String

    external fun getChoiceCount(discIndex: Int, sectionIndex: Int, optionIndex: Int): Int

    external fun getChoiceName(
        discIndex: Int,
        sectionIndex: Int,
        optionIndex: Int,
        choiceIndex: Int
    ): String

    /**
     * @return 0 if no choice is selected, otherwise the index of the selected choice plus one.
     */
    external fun getSelectedChoice(discIndex: Int, sectionIndex: Int, optionIndex: Int): Int

    /**
     * @param choiceIndex 0 to select no choice, otherwise the choice index plus one.
     */
    fun setSelectedChoice(discIndex: Int, sectionIndex: Int, optionIndex: Int, choiceIndex: Int) {
        unsavedChanges = true
        setSelectedChoiceImpl(discIndex, sectionIndex, optionIndex, choiceIndex)
    }

    /**
     * @param choiceIndex 0 to select no choice, otherwise the choice index plus one.
     */
    private external fun setSelectedChoiceImpl(
        discIndex: Int, sectionIndex: Int, optionIndex: Int,
        choiceIndex: Int
    )

    fun loadConfig() {
        loadConfigImpl(gameId, revision, discNumber)
    }

    private external fun loadConfigImpl(gameId: String, revision: Int, discNumber: Int)

    fun saveConfig() {
        if (unsavedChanges) {
            unsavedChanges = false
            saveConfigImpl(gameId)
        }
    }

    private external fun saveConfigImpl(gameId: String)

    companion object {
        @JvmStatic
        private external fun initialize(): Long
    }
}

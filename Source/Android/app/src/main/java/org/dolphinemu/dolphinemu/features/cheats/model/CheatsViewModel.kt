// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import org.dolphinemu.dolphinemu.features.cheats.model.ARCheat.Companion.loadCodes
import org.dolphinemu.dolphinemu.features.cheats.model.ARCheat.Companion.saveCodes
import kotlin.collections.ArrayList

class CheatsViewModel : ViewModel() {
    private var loaded = false

    private var selectedCheatPosition = -1
    private val _selectedCheat = MutableLiveData<Cheat?>(null)
    val selectedCheat: LiveData<Cheat?> get() = _selectedCheat
    private val _isAdding = MutableLiveData(false)
    val isAdding: LiveData<Boolean> get() = _isAdding
    private val _isEditing = MutableLiveData(false)
    val isEditing: LiveData<Boolean> get() = _isEditing

    private val _cheatAddedEvent = MutableLiveData<Int?>(null)
    val cheatAddedEvent: LiveData<Int?> get() = _cheatAddedEvent
    private val _cheatChangedEvent = MutableLiveData<Int?>(null)
    val cheatChangedEvent: LiveData<Int?> get() = _cheatChangedEvent
    private val _cheatDeletedEvent = MutableLiveData<Int?>(null)
    val cheatDeletedEvent: LiveData<Int?> get() = _cheatDeletedEvent
    private val _geckoCheatsDownloadedEvent = MutableLiveData<Int?>(null)
    val geckoCheatsDownloadedEvent: LiveData<Int?> get() = _geckoCheatsDownloadedEvent
    private val _openDetailsViewEvent = MutableLiveData(false)
    val openDetailsViewEvent: LiveData<Boolean> get() = _openDetailsViewEvent

    private var graphicsModGroup: GraphicsModGroup? = null
    var graphicsMods: ArrayList<GraphicsMod> = ArrayList()
    var patchCheats: ArrayList<PatchCheat> = ArrayList()
    var aRCheats: ArrayList<ARCheat> = ArrayList()
    var geckoCheats: ArrayList<GeckoCheat> = ArrayList()

    private var graphicsModsNeedSaving = false
    private var patchCheatsNeedSaving = false
    private var aRCheatsNeedSaving = false
    private var geckoCheatsNeedSaving = false

    fun load(gameID: String, revision: Int) {
        if (loaded) return

        graphicsModGroup = GraphicsModGroup.load(gameID)
        graphicsMods.addAll(graphicsModGroup!!.mods)
        patchCheats.addAll(PatchCheat.loadCodes(gameID, revision))
        aRCheats.addAll(loadCodes(gameID, revision))
        geckoCheats.addAll(GeckoCheat.loadCodes(gameID, revision))

        for (mod in graphicsMods) {
            mod.setChangedCallback { graphicsModsNeedSaving = true }
        }
        for (cheat in patchCheats) {
            cheat.setChangedCallback { patchCheatsNeedSaving = true }
        }
        for (cheat in aRCheats) {
            cheat.setChangedCallback { aRCheatsNeedSaving = true }
        }
        for (cheat in geckoCheats) {
            cheat.setChangedCallback { geckoCheatsNeedSaving = true }
        }

        loaded = true
    }

    fun saveIfNeeded(gameID: String, revision: Int) {
        if (graphicsModsNeedSaving) {
            graphicsModGroup!!.save()
            graphicsModsNeedSaving = false
        }

        if (patchCheatsNeedSaving) {
            PatchCheat.saveCodes(gameID, revision, patchCheats.toTypedArray())
            patchCheatsNeedSaving = false
        }

        if (aRCheatsNeedSaving) {
            saveCodes(gameID, revision, aRCheats.toTypedArray())
            aRCheatsNeedSaving = false
        }

        if (geckoCheatsNeedSaving) {
            GeckoCheat.saveCodes(gameID, revision, geckoCheats.toTypedArray())
            geckoCheatsNeedSaving = false
        }
    }

    fun setSelectedCheat(cheat: Cheat?, position: Int) {
        if (isEditing.value!!) setIsEditing(false)

        _selectedCheat.value = cheat
        selectedCheatPosition = position
    }

    fun startAddingCheat(cheat: Cheat?, position: Int) {
        _selectedCheat.value = cheat
        selectedCheatPosition = position

        _isAdding.value = true
        _isEditing.value = true
    }

    fun finishAddingCheat() {
        check(isAdding.value!!)

        _isAdding.value = false
        _isEditing.value = false

        when (val cheat = selectedCheat.value) {
            is PatchCheat -> {
                patchCheats.add(cheat)
                cheat.setChangedCallback(Runnable { patchCheatsNeedSaving = true })
                patchCheatsNeedSaving = true
            }
            is ARCheat -> {
                aRCheats.add(cheat)
                cheat.setChangedCallback(Runnable { patchCheatsNeedSaving = true })
                aRCheatsNeedSaving = true
            }
            is GeckoCheat -> {
                geckoCheats.add(cheat)
                cheat.setChangedCallback(Runnable { geckoCheatsNeedSaving = true })
                geckoCheatsNeedSaving = true
            }
            else -> throw UnsupportedOperationException()
        }

        notifyCheatAdded()
    }

    fun setIsEditing(isEditing: Boolean) {
        _isEditing.value = isEditing
        if (isAdding.value!! && !isEditing) {
            _isAdding.value = false
            setSelectedCheat(null, -1)
        }
    }

    private fun notifyCheatAdded() {
        _cheatAddedEvent.value = selectedCheatPosition
        _cheatAddedEvent.value = null
    }

    /**
     * Notifies that an edit has been made to the contents of the currently selected cheat.
     */
    fun notifySelectedCheatChanged() {
        notifyCheatChanged(selectedCheatPosition)
    }

    /**
     * Notifies that an edit has been made to the contents of the cheat at the given position.
     */
    private fun notifyCheatChanged(position: Int) {
        _cheatChangedEvent.value = position
        _cheatChangedEvent.value = null
    }

    fun deleteSelectedCheat() {
        val cheat = selectedCheat.value
        val position = selectedCheatPosition

        setSelectedCheat(null, -1)

        if (patchCheats.remove(cheat)) patchCheatsNeedSaving = true
        if (aRCheats.remove(cheat)) aRCheatsNeedSaving = true
        if (geckoCheats.remove(cheat)) geckoCheatsNeedSaving = true

        notifyCheatDeleted(position)
    }

    /**
     * Notifies that the cheat at the given position has been deleted.
     */
    private fun notifyCheatDeleted(position: Int) {
        _cheatDeletedEvent.value = position
        _cheatDeletedEvent.value = null
    }

    fun addDownloadedGeckoCodes(cheats: Array<GeckoCheat>): Int {
        var cheatsAdded = 0

        for (cheat in cheats) {
            if (!geckoCheats.contains(cheat)) {
                geckoCheats.add(cheat)
                cheatsAdded++
            }
        }

        if (cheatsAdded != 0) {
            geckoCheatsNeedSaving = true
            _geckoCheatsDownloadedEvent.value = cheatsAdded
            _geckoCheatsDownloadedEvent.value = null
        }

        return cheatsAdded
    }

    fun openDetailsView() {
        _openDetailsViewEvent.value = true
        _openDetailsViewEvent.value = false
    }
}

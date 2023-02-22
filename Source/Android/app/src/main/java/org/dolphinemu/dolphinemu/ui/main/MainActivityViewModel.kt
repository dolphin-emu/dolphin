// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.main

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class MainActivityViewModel : ViewModel() {
    val shrunkFab: LiveData<Boolean> get() = _shrunkFab
    private val _shrunkFab = MutableLiveData<Boolean>()

    val visibleFab: LiveData<Boolean> get() = _visibleFab
    private val _visibleFab = MutableLiveData<Boolean>()

    val shouldReloadGrid: LiveData<Boolean> get() = _shouldReloadGrid
    private val _shouldReloadGrid = MutableLiveData<Boolean>()

    init {
        _shrunkFab.value = false
        _visibleFab.value = true

        _shouldReloadGrid.value = false
    }

    fun setFabVisible(visible: Boolean) {
        _visibleFab.postValue(visible)
    }

    fun setShrunkFab(shrunk: Boolean) {
        _shrunkFab.postValue(shrunk)
    }

    fun setShouldReloadGrid(shouldReload: Boolean) {
        _shouldReloadGrid.postValue(shouldReload)
    }
}

// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.server

object SkylanderWebUi {
    val HTML = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dolphin - Skylanders Portal</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: #1e293b;
            --card-hover: #334155;
            --primary: #3b82f6;
            --primary-hover: #2563eb;
            --accent: #10b981;
            --danger: #ef4444;
            --danger-hover: #dc2626;
            --text: #f8fafc;
            --text-muted: #94a3b8;
            --border: #334155;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
        body { background: var(--bg-color); color: var(--text); padding: 16px; min-height: 100vh; }
        header { display: flex; justify-content: space-between; align-items: center; padding-bottom: 16px; border-bottom: 1px solid var(--border); margin-bottom: 20px; flex-wrap: wrap; gap: 12px; }
        .logo-area { display: flex; align-items: center; gap: 12px; }
        .logo-title { font-size: 1.4rem; font-weight: 700; color: #60a5fa; }
        .status-badge { display: inline-flex; align-items: center; gap: 6px; padding: 4px 10px; border-radius: 9999px; font-size: 0.85rem; background: #064e3b; color: #6ee7b7; font-weight: 500; }
        .status-dot { width: 8px; height: 8px; border-radius: 50%; background: #10b981; }
        .section-title { font-size: 1.1rem; font-weight: 600; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }
        .slots-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); gap: 12px; margin-bottom: 24px; }
        .slot-card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 10px; padding: 14px; display: flex; flex-direction: column; justify-content: space-between; transition: all 0.2s ease; }
        .slot-card.occupied { border-color: #3b82f6; box-shadow: 0 0 12px rgba(59, 130, 246, 0.2); }
        .slot-header { display: flex; justify-content: space-between; font-size: 0.8rem; color: var(--text-muted); text-transform: uppercase; font-weight: 600; margin-bottom: 6px; }
        .slot-name { font-size: 1.1rem; font-weight: 600; color: #fff; margin-bottom: 12px; min-height: 28px; word-break: break-word; }
        .btn { padding: 8px 14px; border: none; border-radius: 6px; font-size: 0.9rem; font-weight: 600; cursor: pointer; transition: background 0.15s ease; }
        .btn-primary { background: var(--primary); color: white; }
        .btn-primary:hover { background: var(--primary-hover); }
        .btn-danger { background: var(--danger); color: white; }
        .btn-danger:hover { background: var(--danger-hover); }
        .btn-clear { background: #374151; color: #e5e7eb; }
        .btn-clear:hover { background: #4b5563; }
        .search-container { display: flex; gap: 10px; margin-bottom: 16px; flex-wrap: wrap; }
        .search-input { flex: 1; min-width: 200px; padding: 10px 14px; background: var(--card-bg); border: 1px solid var(--border); border-radius: 8px; color: var(--text); font-size: 0.95rem; }
        .search-input:focus { outline: none; border-color: var(--primary); }
        .catalog-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 10px; max-height: 480px; overflow-y: auto; padding-right: 4px; }
        .figure-item { background: var(--card-bg); border: 1px solid var(--border); border-radius: 8px; padding: 12px; cursor: pointer; display: flex; flex-direction: column; justify-content: space-between; gap: 8px; transition: transform 0.15s ease, border-color 0.15s ease; }
        .figure-item:hover { transform: translateY(-2px); border-color: var(--primary); background: var(--card-hover); }
        .figure-name { font-weight: 600; font-size: 0.95rem; color: #f1f5f9; }
        .figure-meta { font-size: 0.75rem; color: var(--text-muted); }
        .modal-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.7); display: none; align-items: center; justify-content: center; padding: 16px; z-index: 100; }
        .modal { background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 20px; max-width: 360px; width: 100%; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5); }
        .modal-title { font-size: 1.2rem; font-weight: 700; margin-bottom: 8px; }
        .modal-desc { font-size: 0.9rem; color: var(--text-muted); margin-bottom: 16px; }
        .slot-select-list { display: flex; flex-direction: column; gap: 8px; margin-bottom: 16px; }
        .slot-btn { display: flex; justify-content: space-between; align-items: center; padding: 10px 14px; background: #0f172a; border: 1px solid var(--border); border-radius: 6px; color: var(--text); cursor: pointer; text-align: left; }
        .slot-btn:hover { border-color: var(--primary); background: #1e293b; }
        .toast { position: fixed; bottom: 20px; right: 20px; background: #10b981; color: white; padding: 12px 20px; border-radius: 8px; font-weight: 600; box-shadow: 0 4px 12px rgba(0,0,0,0.4); opacity: 0; transition: opacity 0.3s ease; pointer-events: none; z-index: 1000; }
        .toast.show { opacity: 1; }
    </style>
</head>
<body>
    <header>
        <div class="logo-area">
            <div class="logo-title">🎮 Dolphin Skylanders Portal</div>
            <div class="status-badge"><div class="status-dot"></div> Connected</div>
        </div>
        <button class="btn btn-clear" onclick="clearAllSlots()">🧹 Clear All Slots</button>
    </header>

    <div class="section-title">
        <span>Portal of Power (Slots)</span>
        <span id="activeCount" style="font-size: 0.9rem; color: var(--text-muted);">0 / 16 placed</span>
    </div>
    <div class="slots-grid" id="slotsContainer">
        <!-- Rendered dynamically -->
    </div>

    <div class="section-title">
        <span>Skylanders Catalog</span>
        <span id="catalogCount" style="font-size: 0.85rem; color: var(--text-muted);">0 figures</span>
    </div>
    <div class="search-container">
        <input type="text" id="searchInput" class="search-input" placeholder="Search Skylander by name..." oninput="filterCatalog()">
    </div>
    <div class="catalog-grid" id="catalogContainer">
        <!-- Rendered dynamically -->
    </div>

    <!-- Modal for Slot Selection -->
    <div class="modal-overlay" id="slotModal">
        <div class="modal">
            <div class="modal-title" id="modalFigureName">Place Figure</div>
            <div class="modal-desc">Select the Portal slot where you want to place this figure:</div>
            <div class="slot-select-list" id="modalSlotsList"></div>
            <button class="btn btn-clear" style="width: 100%;" onclick="closeModal()">Cancel</button>
        </div>
    </div>

    <div class="toast" id="toastMsg">Operation completed</div>

    <script>
        var allSkylanders = [];
        var currentSlots = [];
        var selectedFigure = null;

        function showToast(msg, isError) {
            var toast = document.getElementById('toastMsg');
            toast.innerText = msg;
            toast.style.background = isError ? '#ef4444' : '#10b981';
            toast.classList.add('show');
            setTimeout(function() { toast.classList.remove('show'); }, 2500);
        }

        function fetchStatus() {
            fetch('/api/status')
                .then(function(res) { return res.json(); })
                .then(function(data) {
                    if (data.slots) {
                        currentSlots = data.slots;
                        renderSlots();
                    }
                })
                .catch(function(e) { console.error('Error fetching status', e); });
        }

        function fetchCatalog() {
            fetch('/api/skylanders')
                .then(function(res) { return res.json(); })
                .then(function(data) {
                    allSkylanders = data;
                    document.getElementById('catalogCount').innerText = allSkylanders.length + ' figures';
                    renderCatalog(allSkylanders);
                })
                .catch(function(e) { console.error('Error fetching catalog', e); });
        }

        function renderSlots() {
            var container = document.getElementById('slotsContainer');
            container.innerHTML = '';
            var occupiedCount = 0;

            var displayCount = 4;
            currentSlots.forEach(function(s) {
                if (s.occupied) occupiedCount++;
            });

            currentSlots.slice(0, Math.max(displayCount, 4)).forEach(function(slot) {
                var card = document.createElement('div');
                card.className = 'slot-card ' + (slot.occupied ? 'occupied' : '');
                var html = '<div class="slot-header">Slot ' + (slot.slot + 1) + '</div>';
                html += '<div class="slot-name">' + (slot.occupied ? slot.name : '<span style="color: var(--text-muted); font-weight: normal;">[ Empty ]</span>') + '</div>';
                html += '<div>';
                if (slot.occupied) {
                    html += '<button class="btn btn-danger" style="width: 100%;" onclick="removeSlot(' + slot.slot + ')">Remove</button>';
                } else {
                    html += '<button class="btn btn-primary" style="width: 100%;" onclick="openCatalogForSlot(' + slot.slot + ')">+ Place</button>';
                }
                html += '</div>';
                card.innerHTML = html;
                container.appendChild(card);
            });
            document.getElementById('activeCount').innerText = occupiedCount + ' / 16 placed';
        }

        function renderCatalog(list) {
            var container = document.getElementById('catalogContainer');
            container.innerHTML = '';
            list.slice(0, 100).forEach(function(sky) {
                var item = document.createElement('div');
                item.className = 'figure-item';
                item.innerHTML = '<div class="figure-name">' + sky.name + '</div><div class="figure-meta">ID: ' + sky.id + ' | Var: ' + sky.variant + '</div>';
                item.onclick = function() { openSlotSelectionModal(sky); };
                container.appendChild(item);
            });
        }

        function filterCatalog() {
            var query = document.getElementById('searchInput').value.toLowerCase().trim();
            var filtered = allSkylanders.filter(function(s) { return s.name.toLowerCase().indexOf(query) !== -1; });
            renderCatalog(filtered);
        }

        function openSlotSelectionModal(sky) {
            selectedFigure = sky;
            document.getElementById('modalFigureName').innerText = sky.name;
            var list = document.getElementById('modalSlotsList');
            list.innerHTML = '';

            for (var i = 0; i < 4; i++) {
                (function(idx) {
                    var current = currentSlots.filter(function(s) { return s.slot === idx; })[0];
                    var btn = document.createElement('button');
                    btn.className = 'slot-btn';
                    btn.innerHTML = '<span>Slot ' + (idx + 1) + '</span> <span style="font-size: 0.8rem; color: var(--text-muted);">' + (current && current.occupied ? current.name : '(Empty)') + '</span>';
                    btn.onclick = function() { loadFigureIntoSlot(sky, idx); };
                    list.appendChild(btn);
                })(i);
            }
            document.getElementById('slotModal').style.display = 'flex';
        }

        function openCatalogForSlot(slotIndex) {
            document.getElementById('searchInput').focus();
            showToast('Select a figure below for Slot ' + (slotIndex + 1), false);
        }

        function closeModal() {
            document.getElementById('slotModal').style.display = 'none';
        }

        function loadFigureIntoSlot(sky, slotIndex) {
            closeModal();
            fetch('/api/load', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: sky.id, variant: sky.variant, slot: slotIndex })
            })
            .then(function(res) { return res.json(); })
            .then(function(result) {
                if (result.success) {
                    showToast('Placed ' + sky.name + ' onto Slot ' + (slotIndex + 1), false);
                    fetchStatus();
                } else {
                    showToast(result.error || 'Failed to place figure', true);
                }
            })
            .catch(function() { showToast('Connection error', true); });
        }

        function removeSlot(slotIndex) {
            fetch('/api/remove', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slot: slotIndex })
            })
            .then(function(res) { return res.json(); })
            .then(function(result) {
                if (result.success) {
                    showToast('Slot ' + (slotIndex + 1) + ' cleared', false);
                    fetchStatus();
                } else {
                    showToast(result.error || 'Failed to remove figure', true);
                }
            })
            .catch(function() { showToast('Connection error', true); });
        }

        function clearAllSlots() {
            fetch('/api/clear', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' }
            })
            .then(function(res) { return res.json(); })
            .then(function(result) {
                if (result.success) {
                    showToast('All slots cleared', false);
                    fetchStatus();
                } else {
                    showToast(result.error || 'Error', true);
                }
            })
            .catch(function() { showToast('Connection error', true); });
        }

        // Initialize
        fetchStatus();
        fetchCatalog();
        setInterval(fetchStatus, 3000);
    </script>
</body>
</html>
"""
}

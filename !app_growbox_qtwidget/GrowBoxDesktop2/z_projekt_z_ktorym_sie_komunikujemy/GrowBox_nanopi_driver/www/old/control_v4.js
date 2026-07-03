(() => {
  const OUTPUT_COUNT = 16;
  const SOUND_SLOT_COUNT = 10;

  const el = (id) => document.getElementById(id);

  function nowStr() {
    const d = new Date();
    const pad = (n) => String(n).padStart(2, "0");
    return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
  }

  function logLine(text) {
    const out = el("logOut");
    out.textContent = `[${nowStr()}] ${text}\n` + out.textContent;
  }

  async function apiGet(path) {
    const url = path;
    logLine(`GET ${url}`);
    try {
      const r = await fetch(url, { method: "GET" });
      const ct = r.headers.get("content-type") || "";
      let body = null;

      if (ct.includes("application/json")) {
        body = await r.json();
      } else {
        body = await r.text();
      }

      if (!r.ok) {
        logLine(`HTTP ${r.status}: ${JSON.stringify(body)}`);
        return { ok: false, status: r.status, body };
      }

      logLine(`OK: ${JSON.stringify(body)}`);
      return { ok: true, status: r.status, body };
    } catch (e) {
      logLine(`ERR: ${String(e)}`);
      return { ok: false, status: 0, body: String(e) };
    }
  }

  function qs(params) {
    const p = new URLSearchParams();
    Object.keys(params).forEach(k => {
      const v = params[k];
      if (v === undefined || v === null) return;
      p.set(k, String(v));
    });
    const s = p.toString();
    return s ? `?${s}` : "";
  }

  async function refreshStatus() {
    const res = await apiGet("/api/status");
    const out = el("statusOut");
    if (res.ok) out.textContent = JSON.stringify(res.body, null, 2);
    else out.textContent = `Error: ${res.status}\n` + JSON.stringify(res.body, null, 2);
  }

  function mkOutputCard(id) {
    const card = document.createElement("div");
    card.className = "card";

    const title = document.createElement("h3");
    title.textContent = `Output #${id}`;
    card.appendChild(title);

    const row1 = document.createElement("div");
    row1.className = "row";

    const badge = document.createElement("span");
    badge.className = "badge";
    badge.innerHTML = `<span>Stan:</span> <span class="kv" id="out_state_${id}">?</span>
                       <span>Manual:</span> <span class="kv" id="out_manual_${id}">?</span>`;
    row1.appendChild(badge);

    const bOn = document.createElement("button");
    bOn.className = "btn btn-green";
    bOn.textContent = "ON";
    bOn.onclick = async () => { await apiGet(`/api/output/${id}/on`); await refreshStatus(); };

    const bOff = document.createElement("button");
    bOff.className = "btn btn-red";
    bOff.textContent = "OFF";
    bOff.onclick = async () => { await apiGet(`/api/output/${id}/off`); await refreshStatus(); };

    const bManOn = document.createElement("button");
    bManOn.className = "btn";
    bManOn.textContent = "manual ON";
    bManOn.onclick = async () => { await apiGet(`/api/output/${id}/manual/on`); await refreshStatus(); };

    const bManOff = document.createElement("button");
    bManOff.className = "btn";
    bManOff.textContent = "manual OFF";
    bManOff.onclick = async () => { await apiGet(`/api/output/${id}/manual/off`); await refreshStatus(); };

    row1.appendChild(bOn);
    row1.appendChild(bOff);
    row1.appendChild(bManOn);
    row1.appendChild(bManOff);

    card.appendChild(row1);

    const row2 = document.createElement("div");
    row2.className = "row";
    row2.innerHTML = `<span class="badge">
        <span>Auto ON:</span> <span class="kv" id="out_autoon_${id}">--:--</span>
        <span>Auto OFF:</span> <span class="kv" id="out_autooff_${id}">--:--</span>
      </span>`;
    card.appendChild(row2);

    const row3 = document.createElement("div");
    row3.className = "row";

    const inpOnH = document.createElement("input");
    inpOnH.className = "inp small";
    inpOnH.type = "number";
    inpOnH.min = "0";
    inpOnH.max = "23";
    inpOnH.value = "18";

    const inpOnM = document.createElement("input");
    inpOnM.className = "inp small";
    inpOnM.type = "number";
    inpOnM.min = "0";
    inpOnM.max = "59";
    inpOnM.value = "0";

    const btnSetOn = document.createElement("button");
    btnSetOn.className = "btn btn-primary";
    btnSetOn.textContent = "set auto ON";
    btnSetOn.onclick = async () => {
      const hour = Number(inpOnH.value);
      const minute = Number(inpOnM.value);
      await apiGet(`/api/output/${id}/autoon` + qs({ hour, minute }));
      await refreshStatus();
    };

    row3.appendChild(document.createTextNode("autoon hour/min"));
    row3.appendChild(inpOnH);
    row3.appendChild(inpOnM);
    row3.appendChild(btnSetOn);
    card.appendChild(row3);

    const row4 = document.createElement("div");
    row4.className = "row";

    const inpOffH = document.createElement("input");
    inpOffH.className = "inp small";
    inpOffH.type = "number";
    inpOffH.min = "0";
    inpOffH.max = "23";
    inpOffH.value = "22";

    const inpOffM = document.createElement("input");
    inpOffM.className = "inp small";
    inpOffM.type = "number";
    inpOffM.min = "0";
    inpOffM.max = "59";
    inpOffM.value = "0";

    const btnSetOff = document.createElement("button");
    btnSetOff.className = "btn btn-primary";
    btnSetOff.textContent = "set auto OFF";
    btnSetOff.onclick = async () => {
      const hour = Number(inpOffH.value);
      const minute = Number(inpOffM.value);
      await apiGet(`/api/output/${id}/autooff` + qs({ hour, minute }));
      await refreshStatus();
    };

    row4.appendChild(document.createTextNode("autooff hour/min"));
    row4.appendChild(inpOffH);
    row4.appendChild(inpOffM);
    row4.appendChild(btnSetOff);
    card.appendChild(row4);

    return card;
  }

  function mkSoundCard(slot) {
    const card = document.createElement("div");
    card.className = "card";

    const title = document.createElement("h3");
    title.textContent = `Alarm slot #${slot}`;
    card.appendChild(title);

    const row1 = document.createElement("div");
    row1.className = "row";
    row1.innerHTML = `<span class="badge">
        <span>enabled:</span> <span class="kv" id="snd_en_${slot}">?</span>
        <span>time:</span> <span class="kv" id="snd_time_${slot}">--:--</span>
        <span>vol:</span> <span class="kv" id="snd_vol_${slot}">--</span>
      </span>`;
    card.appendChild(row1);

    const row2 = document.createElement("div");
    row2.className = "row";

    const btnEn = document.createElement("button");
    btnEn.className = "btn btn-green";
    btnEn.textContent = "enable";
    btnEn.onclick = async () => { await apiGet(`/api/sound/alarm/${slot}/enable`); await refreshStatus(); };

    const btnDis = document.createElement("button");
    btnDis.className = "btn btn-red";
    btnDis.textContent = "disable";
    btnDis.onclick = async () => { await apiGet(`/api/sound/alarm/${slot}/disable`); await refreshStatus(); };

    row2.appendChild(btnEn);
    row2.appendChild(btnDis);

    const inpEn = document.createElement("input");
    inpEn.className = "inp small";
    inpEn.type = "number";
    inpEn.min = "0";
    inpEn.max = "1";
    inpEn.value = "1";

    const inpH = document.createElement("input");
    inpH.className = "inp small";
    inpH.type = "number";
    inpH.min = "0";
    inpH.max = "23";
    inpH.value = "18";

    const inpM = document.createElement("input");
    inpM.className = "inp small";
    inpM.type = "number";
    inpM.min = "0";
    inpM.max = "59";
    inpM.value = "30";

    const inpFile = document.createElement("input");
    inpFile.className = "inp";
    inpFile.type = "text";
    inpFile.value = "alarm.mp3";

    const inpVol = document.createElement("input");
    inpVol.className = "inp small";
    inpVol.type = "number";
    inpVol.min = "0";
    inpVol.max = "100";
    inpVol.value = "70";

    const btnSet = document.createElement("button");
    btnSet.className = "btn btn-primary";
    btnSet.textContent = "set";
    btnSet.onclick = async () => {
      const enabled = Number(inpEn.value);
      const hour = Number(inpH.value);
      const minute = Number(inpM.value);
      const file = String(inpFile.value || "");
      const volume = Number(inpVol.value);
      await apiGet(`/api/sound/alarm/${slot}/set` + qs({ enabled, hour, minute, file, volume }));
      await refreshStatus();
    };

    const row3 = document.createElement("div");
    row3.className = "row";
    row3.appendChild(document.createTextNode("enabled(0/1)"));
    row3.appendChild(inpEn);
    row3.appendChild(document.createTextNode("hour"));
    row3.appendChild(inpH);
    row3.appendChild(document.createTextNode("minute"));
    row3.appendChild(inpM);
    row3.appendChild(btnSet);

    const row4 = document.createElement("div");
    row4.className = "row";
    row4.appendChild(document.createTextNode("file"));
    row4.appendChild(inpFile);
    row4.appendChild(document.createTextNode("volume"));
    row4.appendChild(inpVol);

    card.appendChild(row2);
    card.appendChild(row3);
    card.appendChild(row4);

    return card;
  }

  function applyStatusToBadges(statusJson) {
    if (!statusJson || !statusJson.outputs || !Array.isArray(statusJson.outputs)) return;

    for (const o of statusJson.outputs) {
      const id = o.id;
      const st = el(`out_state_${id}`);
      const mm = el(`out_manual_${id}`);
      const aon = el(`out_autoon_${id}`);
      const aoff = el(`out_autooff_${id}`);

      if (st) st.textContent = o.state ? "ON" : "OFF";
      if (mm) mm.textContent = o.manualMode ? "ON" : "OFF";
      if (aon) aon.textContent = `${String(o.autoOnHour).padStart(2, "0")}:${String(o.autoOnMinute).padStart(2, "0")}`;
      if (aoff) aoff.textContent = `${String(o.autoOffHour).padStart(2, "0")}:${String(o.autoOffMinute).padStart(2, "0")}`;
    }

    if (statusJson.soundAlarms && Array.isArray(statusJson.soundAlarms)) {
      for (const s of statusJson.soundAlarms) {
        const slot = s.slot;
        const en = el(`snd_en_${slot}`);
        const tm = el(`snd_time_${slot}`);
        const vv = el(`snd_vol_${slot}`);
        if (en) en.textContent = s.enabled ? "1" : "0";
        if (tm) tm.textContent = `${String(s.hour).padStart(2, "0")}:${String(s.minute).padStart(2, "0")}`;
        if (vv) vv.textContent = String(s.volume);
      }
    }
  }

  async function refreshAll() {
    const res = await apiGet("/api/status");
    const out = el("statusOut");
    if (res.ok) {
      out.textContent = JSON.stringify(res.body, null, 2);
      applyStatusToBadges(res.body);
    } else {
      out.textContent = `Error: ${res.status}\n` + JSON.stringify(res.body, null, 2);
    }
  }

  function init() {
    // Outputs UI
    const outputsRoot = el("outputsRoot");
    for (let i = 1; i <= OUTPUT_COUNT; i++) {
      outputsRoot.appendChild(mkOutputCard(i));
    }

    // Sound alarms UI
    const soundRoot = el("soundRoot");
    for (let s = 1; s <= SOUND_SLOT_COUNT; s++) {
      soundRoot.appendChild(mkSoundCard(s));
    }

    // Top buttons
    el("btnClearLog").onclick = () => { el("logOut").textContent = ""; };
    el("btnGetStatus").onclick = refreshAll;
    el("btnRefreshAll").onclick = refreshAll;

    // System
    el("btnAutoOn").onclick = async () => { await apiGet("/api/system/auto/on"); await refreshAll(); };
    el("btnAutoOff").onclick = async () => { await apiGet("/api/system/auto/off"); await refreshAll(); };

    // Volume
    el("btnVolGet").onclick = async () => { await apiGet("/api/sound/volume/get"); await refreshAll(); };
    el("btnVolSet").onclick = async () => {
      const value = Number(el("inpVolSet").value);
      await apiGet("/api/sound/volume/set" + qs({ value }));
      await refreshAll();
    };

    // Play
    el("btnPlay").onclick = async () => {
      const file = String(el("inpPlayFile").value || "");
      const volume = Number(el("inpPlayVol").value);
      await apiGet("/api/sound/play" + qs({ file, volume }));
      await refreshAll();
    };

    el("btnPlayCurrent").onclick = async () => {
      const file = String(el("inpPlayFile").value || "");
      await apiGet("/api/sound/play_current" + qs({ file }));
      await refreshAll();
    };

    el("btnSoundStatus").onclick = async () => { await apiGet("/api/sound/status"); await refreshAll(); };

    // Initial
    refreshAll();
  }

  window.addEventListener("DOMContentLoaded", init);
})();
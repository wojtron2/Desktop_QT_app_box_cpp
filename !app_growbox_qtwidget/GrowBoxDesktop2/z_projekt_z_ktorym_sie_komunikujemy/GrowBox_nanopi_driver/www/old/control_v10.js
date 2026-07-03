// file: control_v10.js

(() => {
  // Komentarze krotkie, po polsku, ascii

  const API = {
    status: "/api/status",
    outputOn: (id) => `/api/output/${id}/on`,
    outputOff: (id) => `/api/output/${id}/off`,
    outputsOnAll: "/api/outputs/on_all",
    outputsOffAll: "/api/outputs/off_all",
    outputAutoOn: (id, hour, minute) => `/api/output/${id}/autoon?hour=${encodeURIComponent(hour)}&minute=${encodeURIComponent(minute)}`,
    outputAutoOff: (id, hour, minute) => `/api/output/${id}/autooff?hour=${encodeURIComponent(hour)}&minute=${encodeURIComponent(minute)}`,
    outputsAutoOnAll: (hour, minute) => `/api/outputs/autoon_all?hour=${encodeURIComponent(hour)}&minute=${encodeURIComponent(minute)}`,
    outputsAutoOffAll: (hour, minute) => `/api/outputs/autooff_all?hour=${encodeURIComponent(hour)}&minute=${encodeURIComponent(minute)}`,
    outputManualOn: (id) => `/api/output/${id}/manual/on`,
    outputManualOff: (id) => `/api/output/${id}/manual/off`,
    outputsManualOnAll: "/api/outputs/manual/on_all",
    outputsManualOffAll: "/api/outputs/manual/off_all",
    systemAutoOn: "/api/system/auto/on",
    systemAutoOff: "/api/system/auto/off",

    soundStatus: "/api/sound/status",
    soundAutoOn: "/api/sound/auto/on",
    soundAutoOff: "/api/sound/auto/off",
    soundVolumeGet: "/api/sound/volume/get",
    soundVolumeSet: (value) => `/api/sound/volume/set?value=${encodeURIComponent(value)}`,
    soundPlay: (file, volume) => `/api/sound/play?file=${encodeURIComponent(file)}&volume=${encodeURIComponent(volume)}`,
    soundPlayCurrent: (file) => `/api/sound/play_current?file=${encodeURIComponent(file)}`,
    soundAlarmSet: (slot, enabled, hour, minute, file, volume) =>
      `/api/sound/alarm/${slot}/set?enabled=${encodeURIComponent(enabled ? 1 : 0)}&hour=${encodeURIComponent(hour)}&minute=${encodeURIComponent(minute)}&file=${encodeURIComponent(file)}&volume=${encodeURIComponent(volume)}`,
    soundAlarmEnable: (slot) => `/api/sound/alarm/${slot}/enable`,
    soundAlarmDisable: (slot) => `/api/sound/alarm/${slot}/disable`,
  };

  function el(tag, attrs, ...children) {
    const n = document.createElement(tag);

    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) {
        if (k === "class") n.className = v;
        else if (k.startsWith("on") && typeof v === "function") n.addEventListener(k.slice(2), v);
        else if (k === "value") n.value = v;
        else if (v === true) n.setAttribute(k, k);
        else if (v !== false && v != null) n.setAttribute(k, String(v));
      }
    }

    for (const c of children.flat()) {
      if (c == null) continue;
      if (typeof c === "string" || typeof c === "number") n.appendChild(document.createTextNode(String(c)));
      else n.appendChild(c);
    }

    return n;
  }

  function Btn(label, opts = {}) {
    const cls = ["btn", opts.variant || "ghost", opts.small ? "smallBtn" : ""].filter(Boolean).join(" ");
    return el("button", {
      class: cls,
      onclick: opts.onClick,
      type: "button",
      disabled: !!opts.disabled
    }, label);
  }

  function Card(title, right, body) {
    return el("div", { class: "card" },
      el("div", { class: "cardHeader" },
        el("div", { class: "cardTitle" }, title),
        right || el("div", { class: "kv" })
      ),
      el("div", { class: "cardBody" }, body)
    );
  }

  function clamp100(v) {
    const n = Number(v);
    if (!Number.isFinite(n)) return 0;
    if (n < 0) return 0;
    if (n > 100) return 100;
    return Math.trunc(n);
  }

  function pad2(v) {
    return String(v).padStart(2, "0");
  }

  function formatTimeLabel(hour, minute) {
    if (hour == null || hour < 0) return "--:--";
    return `${pad2(hour)}:${pad2(minute || 0)}`;
  }

  function formatTimeInput(hour, minute) {
    if (hour == null || hour < 0) return "";
    return `${pad2(hour)}:${pad2(minute || 0)}`;
  }

  function parseTimeValue(value) {
    const s = String(value || "").trim();
    const m = s.match(/^(\d{2}):(\d{2})$/);
    if (!m) throw new Error("Podaj czas HH:MM");

    const hour = Number(m[1]);
    const minute = Number(m[2]);

    if (!Number.isInteger(hour) || hour < 0 || hour > 23) throw new Error("Godzina poza zakresem 0..23");
    if (!Number.isInteger(minute) || minute < 0 || minute > 59) throw new Error("Minuta poza zakresem 0..59");

    return { hour, minute };
  }

  function assignTimeToObject(obj, hourKey, minuteKey, value) {
    const s = String(value || "").trim();

    if (!s) {
      obj[hourKey] = -1;
      obj[minuteKey] = 0;
      return;
    }

    try {
      const t = parseTimeValue(s);
      obj[hourKey] = t.hour;
      obj[minuteKey] = t.minute;
    } catch {
      obj[hourKey] = -1;
      obj[minuteKey] = 0;
    }
  }

  const state = {
    route: "home",
    globalAutoMode: true,

    outputs: Array.from({ length: 16 }, (_, i) => ({
      id: i + 1,
      name: `OUT${i + 1}`,
      state: false,
      autoOnHour: -1,
      autoOnMinute: 0,
      autoOffHour: -1,
      autoOffMinute: 0,
      manualMode: false,
    })),

    outputsScheduler: {
      autoOnHour: 8,
      autoOnMinute: 0,
      autoOffHour: 20,
      autoOffMinute: 0,
    },

    sounds: {
      globalSoundVolume: 100,
      globalSoundAutoMode: true,
      soundAlarms: Array.from({ length: 10 }, (_, i) => ({
        slot: i + 1,
        enabled: false,
        hour: -1,
        minute: 0,
        file: "",
        volume: 100,
      })),
      manualFile: "alarm.mp3",
      manualVolume: 70,
    },

    camScheduler: {
      autoOnHour: 8,
      autoOnMinute: 0,
      autoOffHour: 20,
      autoOffMinute: 0,
    },

    lastRefreshMs: 0,
    deviceLocalTime: "",
    deviceUtcTime: "",
    deviceEpoch: 0,
    deviceEpochBaseMs: 0,
    loading: false,
    error: "",
  };

  async function httpGetJson(url) {
    const res = await fetch(url, { method: "GET", cache: "no-store" });
    const text = await res.text();

    let data = null;
    try {
      data = text ? JSON.parse(text) : null;
    } catch {
      // ignore
    }

    if (!res.ok) {
      const msg = (data && (data.error || data.message))
        ? (data.error || data.message)
        : `${res.status} ${res.statusText}`;
      throw new Error(msg);
    }

    return data;
  }

  async function httpGet(url) {
    const res = await fetch(url, { method: "GET", cache: "no-store" });
    const text = await res.text();

    let data = null;
    try {
      data = text ? JSON.parse(text) : null;
    } catch {
      // ignore
    }

    if (!res.ok) {
      const msg = (data && (data.error || data.message))
        ? (data.error || data.message)
        : `${res.status} ${res.statusText}`;
      throw new Error(msg);
    }

    return data;
  }

  async function refreshOutputs() {
    state.loading = true;
    state.error = "";
    render();

    try {
      const data = await httpGetJson(API.status);

      if (data && Array.isArray(data.outputs)) {
        if (typeof data.globalAutoMode === "boolean") {
          state.globalAutoMode = data.globalAutoMode;
        }

        state.deviceLocalTime = data.deviceLocalTime || "";
        state.deviceUtcTime = data.deviceUtcTime || "";
        state.deviceEpoch = Number.isFinite(Number(data.deviceEpoch)) ? Number(data.deviceEpoch) : 0;
        state.deviceEpochBaseMs = Date.now();

        for (const o of data.outputs) {
          const id = Number(o.id);
          const idx = state.outputs.findIndex(x => x.id === id);

          if (idx >= 0) {
            state.outputs[idx].state = !!o.state;
            state.outputs[idx].autoOnHour = Number.isFinite(Number(o.autoOnHour)) ? Number(o.autoOnHour) : -1;
            state.outputs[idx].autoOnMinute = Number.isFinite(Number(o.autoOnMinute)) ? Number(o.autoOnMinute) : 0;
            state.outputs[idx].autoOffHour = Number.isFinite(Number(o.autoOffHour)) ? Number(o.autoOffHour) : -1;
            state.outputs[idx].autoOffMinute = Number.isFinite(Number(o.autoOffMinute)) ? Number(o.autoOffMinute) : 0;
            state.outputs[idx].manualMode = !!o.manualMode;
          }
        }

        state.lastRefreshMs = Date.now();
      } else {
        state.error = "Brak pola outputs w /api/status";
      }
    } catch (e) {
      state.error = `Refresh error: ${e.message || e}`;
    } finally {
      state.loading = false;
      render();
    }
  }

  async function refreshSounds() {
    state.loading = true;
    state.error = "";
    render();

    try {
      const data = await httpGetJson(API.soundStatus);

      if (data) {
        if (typeof data.globalSoundVolume === "number") {
          state.sounds.globalSoundVolume = data.globalSoundVolume;
        }

        if (typeof data.globalSoundAutoMode === "boolean") {
          state.sounds.globalSoundAutoMode = data.globalSoundAutoMode;
        }

        if (Array.isArray(data.soundAlarms)) {
          for (const s of data.soundAlarms) {
            const slot = Number(s.slot);
            const idx = state.sounds.soundAlarms.findIndex(x => x.slot === slot);

            if (idx >= 0) {
              state.sounds.soundAlarms[idx] = {
                slot,
                enabled: !!s.enabled,
                hour: Number.isFinite(Number(s.hour)) ? Number(s.hour) : -1,
                minute: Number.isFinite(Number(s.minute)) ? Number(s.minute) : 0,
                file: s.file || "",
                volume: Number.isFinite(Number(s.volume)) ? Number(s.volume) : 100,
              };
            }
          }
        }

        state.lastRefreshMs = Date.now();
      } else {
        state.error = "Brak danych w /api/sound/status";
      }
    } catch (e) {
      state.error = `Sound refresh error: ${e.message || e}`;
    } finally {
      state.loading = false;
      render();
    }
  }

  async function setOutput(id, on) {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(on ? API.outputOn(id) : API.outputOff(id));

      const idx = state.outputs.findIndex(x => x.id === id);
      if (idx >= 0) state.outputs[idx].state = !!on;

      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `Set error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setAllOutputs(on) {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(on ? API.outputsOnAll : API.outputsOffAll);

      for (const o of state.outputs) {
        o.state = !!on;
      }

      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `SetAll error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setGlobalAuto(on) {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(on ? API.systemAutoOn : API.systemAutoOff);
      state.globalAutoMode = !!on;
      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `Auto error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setOutputAutoOn(id) {
    state.loading = true;
    state.error = "";
    render();

    try {
      const o = state.outputs.find(x => x.id === id);
      if (!o) throw new Error("Nie znaleziono wyjscia");
      if (o.autoOnHour < 0) throw new Error("Podaj czas ON");

      await httpGet(API.outputAutoOn(id, o.autoOnHour, o.autoOnMinute));
      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `AutoOn error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setOutputAutoOff(id) {
    state.loading = true;
    state.error = "";
    render();

    try {
      const o = state.outputs.find(x => x.id === id);
      if (!o) throw new Error("Nie znaleziono wyjscia");
      if (o.autoOffHour < 0) throw new Error("Podaj czas OFF");

      await httpGet(API.outputAutoOff(id, o.autoOffHour, o.autoOffMinute));
      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `AutoOff error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setAllOutputsAutoOn() {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(API.outputsAutoOnAll(state.outputsScheduler.autoOnHour, state.outputsScheduler.autoOnMinute));
      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `AutoOnAll error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setAllOutputsAutoOff() {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(API.outputsAutoOffAll(state.outputsScheduler.autoOffHour, state.outputsScheduler.autoOffMinute));
      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `AutoOffAll error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setOutputManual(id, on) {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(on ? API.outputManualOn(id) : API.outputManualOff(id));

      const idx = state.outputs.findIndex(x => x.id === id);
      if (idx >= 0) state.outputs[idx].manualMode = !!on;

      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `Manual error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setAllManual(on) {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(on ? API.outputsManualOnAll : API.outputsManualOffAll);

      for (const o of state.outputs) {
        o.manualMode = !!on;
      }

      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `ManualAll error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setSoundAuto(on) {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(on ? API.soundAutoOn : API.soundAutoOff);
      state.sounds.globalSoundAutoMode = !!on;
      state.lastRefreshMs = Date.now();
      await refreshSounds();
    } catch (e) {
      state.error = `SoundAuto error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setGlobalSoundVolume(value) {
    state.loading = true;
    state.error = "";
    render();

    try {
      const v = clamp100(value);
      await httpGet(API.soundVolumeSet(v));
      state.sounds.globalSoundVolume = v;
      state.lastRefreshMs = Date.now();
      await refreshSounds();
    } catch (e) {
      state.error = `Volume error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function playSoundManual(useCurrentVolume) {
    state.loading = true;
    state.error = "";
    render();

    try {
      const file = (state.sounds.manualFile || "").trim();
      if (!file) throw new Error("Podaj nazwe pliku");

      if (useCurrentVolume) {
        await httpGet(API.soundPlayCurrent(file));
      } else {
        const v = clamp100(state.sounds.manualVolume);
        await httpGet(API.soundPlay(file, v));
      }

      state.lastRefreshMs = Date.now();
    } catch (e) {
      state.error = `Play error: ${e.message || e}`;
    } finally {
      state.loading = false;
      render();
    }
  }

  async function saveSoundAlarm(slot) {
    state.loading = true;
    state.error = "";
    render();

    try {
      const s = state.sounds.soundAlarms.find(x => x.slot === slot);
      if (!s) throw new Error("Nie znaleziono slotu");
      if (s.hour < 0) throw new Error("Podaj czas slotu");

      const file = String(s.file || "").trim();
      if (!file) throw new Error("Podaj nazwe pliku");

      const volume = clamp100(s.volume);

      await httpGet(API.soundAlarmSet(slot, s.enabled, s.hour, s.minute, file, volume));
      state.lastRefreshMs = Date.now();
      await refreshSounds();
    } catch (e) {
      state.error = `Sound save error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  async function setSoundAlarmEnabled(slot, enabled) {
    state.loading = true;
    state.error = "";
    render();

    try {
      await httpGet(enabled ? API.soundAlarmEnable(slot) : API.soundAlarmDisable(slot));

      const idx = state.sounds.soundAlarms.findIndex(x => x.slot === slot);
      if (idx >= 0) state.sounds.soundAlarms[idx].enabled = !!enabled;

      state.lastRefreshMs = Date.now();
      await refreshSounds();
    } catch (e) {
      state.error = `Sound slot error: ${e.message || e}`;
      state.loading = false;
      render();
    }
  }

  function getEstimatedDeviceEpoch() {
    if (!state.deviceEpoch || !state.deviceEpochBaseMs) return 0;
    const elapsedMs = Date.now() - state.deviceEpochBaseMs;
    return state.deviceEpoch + Math.floor(elapsedMs / 1000);
  }

  function formatEpochLocal(epochSec) {
    if (!epochSec) return "--";

    const d = new Date(epochSec * 1000);
    const y = d.getFullYear();
    const mo = pad2(d.getMonth() + 1);
    const da = pad2(d.getDate());
    const h = pad2(d.getHours());
    const mi = pad2(d.getMinutes());
    const s = pad2(d.getSeconds());

    return `${y}-${mo}-${da} ${h}:${mi}:${s}`;
  }

  function updateDeviceClockLabels() {
    const epoch = getEstimatedDeviceEpoch();
    const text = epoch ? `device ${formatEpochLocal(epoch)}` : "device --";

    const topbarClock = document.getElementById("deviceClockLabel");
    if (topbarClock) topbarClock.textContent = text;

    const homeLocal = document.getElementById("homeDeviceLocalTime");
    if (homeLocal) homeLocal.textContent = epoch ? formatEpochLocal(epoch) : "--";
  }

  function startDeviceClockTicker() {
    setInterval(() => {
      updateDeviceClockLabels();
    }, 1000);
  }

  function TopBar() {
	  const badgeText = state.loading ? "busy..." : (state.error ? "error" : "ok");
	  const deviceTimeText = state.deviceEpoch ? `device ${formatEpochLocal(getEstimatedDeviceEpoch())}` : "device --";

	  const navBtn = (route, label) => Btn(label, {
		variant: state.route === route ? "primary" : "ghost",
		disabled: state.loading && state.route !== route,
		onClick: async () => {
		  state.route = route;
		  render();

		  if (route === "outputs" || route === "outputs_scheduler" || route === "home" || route === "cam" || route === "cam_scheduler") {
			await refreshOutputs();
		  }

		  if (route === "sounds" || route === "sound_scheduler") {
			await refreshSounds();
		  }
		},
	  });

	  return el("div", { class: "topbar" },
		el("div", {
		  style: "display:flex; flex-direction:column; gap:10px;"
		},
		  el("div", {
			style: "display:flex; justify-content:space-between; align-items:center; gap:12px; flex-wrap:wrap;"
		  },
			el("div", { class: "brand" },
			  el("div", { class: "logoDot" }),
			  el("div", null,
				el("div", { class: "brandTitle" }, "GROWBOX - control_v10"),
				el("div", { class: "brandSub" }, "green/black UI")
			  )
			),
			el("div", {
			  style: "display:flex; gap:8px; align-items:center; flex-wrap:wrap;"
			},
			  el("div", { class: "badge", id: "deviceClockLabel" }, deviceTimeText),
			  el("div", { class: "badge" }, badgeText)
			)
		  ),
		  el("div", {
			class: "nav",
			style: "display:flex; flex-wrap:wrap; gap:8px;"
		  },
			navBtn("home", "Home"),
			navBtn("outputs", "Outputs"),
			navBtn("outputs_scheduler", "Outputs scheduler"),
			navBtn("sounds", "Sounds"),
			navBtn("sound_scheduler", "Sound scheduler"),
			navBtn("cam", "Cam"),
			navBtn("cam_scheduler", "Cam scheduler")
		  )
		)
	  );
	}

  function HomeView() {
    const updaterUrl = `${window.location.protocol}//${window.location.hostname}:8081`;
    const deviceLocalText = state.deviceEpoch ? formatEpochLocal(getEstimatedDeviceEpoch()) : "--";

    const body = el("div", null,
      el("div", { class: "kv" },
        "Start: ", el("b", null, "Home"), " | ",
        "Menu: ", el("b", null, "Outputs / Outputs scheduler / Sounds / Sound scheduler / Cam / Cam scheduler")
      ),
      el("div", { class: "footerNote" },
        "Device local time: ",
        el("span", { id: "homeDeviceLocalTime" }, deviceLocalText)
      ),
      el("div", { class: "footerNote" },
        `Device UTC time: ${state.deviceUtcTime || "--"}`
      ),
      el("div", { class: "footerNote" },
        "Outputs scheduler: global auto, auto on/off all, manual on/off all i ustawienia per wyjscie."
      ),
      el("div", { class: "footerNote" },
        "Sound scheduler: auto sounds on/off i konfiguracja slotow alarmow. Sounds: volume i manual play."
      ),
      el("div", { class: "footerNote" },
        "Cam scheduler: na razie tylko placeholder UI. Backend cam scheduler nie jest jeszcze dodany."
      ),
      el("div", { class: "footerNote" },
        "Updater: ",
        el("a", {
          href: updaterUrl,
          target: "_blank",
          rel: "noopener noreferrer"
        }, updaterUrl)
      ),
      state.error ? el("div", { class: "footerNote" }, state.error) : null
    );

    return Card("Strona glowna", null, body);
  }

  function OutputsView() {
    const right = el("div", { class: "kv" },
      "Last: ", el("b", null, state.lastRefreshMs ? new Date(state.lastRefreshMs).toLocaleTimeString() : "-")
    );

    const toolbar = el("div", { class: "outputsToolbar" },
      el("div", { class: "outputsToolbarLeft" },
        Btn("Refresh", { variant: "primary", onClick: refreshOutputs, disabled: state.loading }),
        Btn("On all", { onClick: () => setAllOutputs(true), disabled: state.loading }),
        Btn("Off all", { variant: "danger", onClick: () => setAllOutputs(false), disabled: state.loading })
      ),
      el("div", { class: "outputsToolbarRight" },
        state.loading ? el("span", { class: "pill" }, "working...") : null,
        state.error ? el("span", { class: "pill" }, state.error) : null
      )
    );

    const grid = el("div", { class: "outputsGrid" },
      state.outputs.map(o => OutputTile(o))
    );

    const body = el("div", null,
      toolbar,
      grid,
      el("div", { class: "footerNote" },
        "Klikniecie ON/OFF na tej zakladce ustawia dane wyjscie lub wszystkie wyjscia w MANUAL po stronie backendu."
      )
    );

    return Card("Outputs", right, body);
  }

  function OutputTile(o) {
    const pill = el("span", { class: `pill ${o.state ? "on" : "off"}` }, o.state ? "ON" : "OFF");

    const toggleBtn = o.state
      ? Btn("Off", { small: true, variant: "danger", onClick: () => setOutput(o.id, false), disabled: state.loading })
      : Btn("On", { small: true, variant: "primary", onClick: () => setOutput(o.id, true), disabled: state.loading });

    return el("div", { class: "outputTile" },
      el("div", { class: "outputLeft" },
        el("div", { class: "outputName" }, o.name),
        el("div", { class: "outputMeta" },
          pill,
          el("span", { class: "pill" }, `id=${o.id}`)
        )
      ),
      el("div", null, toggleBtn)
    );
  }

 function OutputsSchedulerView() {
	  const right = el("div", { class: "kv" },
		"Last: ", el("b", null, state.lastRefreshMs ? new Date(state.lastRefreshMs).toLocaleTimeString() : "-")
	  );

	  const autoPill = el("span", { class: `pill ${state.globalAutoMode ? "on" : "off"}` },
		state.globalAutoMode ? "AUTO ON" : "AUTO OFF"
	  );

	  const toolbar = el("div", { class: "outputsToolbar" },
		el("div", { class: "outputsToolbarLeft" },
		  Btn("Refresh", { variant: "primary", onClick: refreshOutputs, disabled: state.loading }),
		  Btn("Scheduler ON", { onClick: () => setGlobalAuto(true), disabled: state.loading }),
		  Btn("Scheduler OFF", { variant: "danger", onClick: () => setGlobalAuto(false), disabled: state.loading }),
		  autoPill
		),
		el("div", { class: "outputsToolbarRight" },
		  state.loading ? el("span", { class: "pill" }, "working...") : null,
		  state.error ? el("span", { class: "pill" }, state.error) : null
		)
	  );

	  const bulkRow = el("div", { class: "outputTile" },
		el("div", { class: "outputLeft" },
		  el("div", { class: "outputName" }, "All outputs scheduler"),
		  el("div", { class: "outputMeta" },
			el("span", { class: "pill" }, `auto on ${formatTimeLabel(state.outputsScheduler.autoOnHour, state.outputsScheduler.autoOnMinute)}`),
			el("span", { class: "pill" }, `auto off ${formatTimeLabel(state.outputsScheduler.autoOffHour, state.outputsScheduler.autoOffMinute)}`)
		  ),
		  el("div", { class: "footerNote" }, "Set MANUAL all blokuje automat dla wszystkich wyjsc.")
		),
		el("div", { class: "outputsToolbarLeft" },
		  el("input", {
			type: "time",
			value: formatTimeInput(state.outputsScheduler.autoOnHour, state.outputsScheduler.autoOnMinute),
			oninput: (e) => assignTimeToObject(state.outputsScheduler, "autoOnHour", "autoOnMinute", e.target.value)
		  }),
		  Btn("Set ON all", { variant: "primary", onClick: setAllOutputsAutoOn, disabled: state.loading }),
		  el("input", {
			type: "time",
			value: formatTimeInput(state.outputsScheduler.autoOffHour, state.outputsScheduler.autoOffMinute),
			oninput: (e) => assignTimeToObject(state.outputsScheduler, "autoOffHour", "autoOffMinute", e.target.value)
		  }),
		  Btn("Set OFF all", { variant: "primary", onClick: setAllOutputsAutoOff, disabled: state.loading }),
		  Btn("Set MANUAL all", { onClick: () => setAllManual(true), disabled: state.loading }),
		  Btn("Set AUTO all", { variant: "danger", onClick: () => setAllManual(false), disabled: state.loading })
		)
	  );

	  const grid = el("div", { class: "outputsGrid" },
		state.outputs.map(o => OutputSchedulerTile(o))
	  );

	  const body = el("div", null,
		toolbar,
		bulkRow,
		grid,
		el("div", { class: "footerNote" },
		  "Jesli ustawisz auto on i auto off, scheduler traktuje to jako okno czasowe."
		)
	  );

	  return Card("Outputs scheduler", right, body);
	}

  function OutputSchedulerTile(o) {
    return el("div", { class: "outputTile" },
      el("div", { class: "outputLeft" },
        el("div", { class: "outputName" }, o.name),
        el("div", { class: "outputMeta" },
          el("span", { class: `pill ${o.state ? "on" : "off"}` }, o.state ? "ON" : "OFF"),
          el("span", { class: `pill ${o.manualMode ? "off" : "on"}` }, o.manualMode ? "MANUAL" : "AUTO"),
          el("span", { class: "pill" }, `on ${formatTimeLabel(o.autoOnHour, o.autoOnMinute)}`),
          el("span", { class: "pill" }, `off ${formatTimeLabel(o.autoOffHour, o.autoOffMinute)}`)
        )
      ),
      el("div", { class: "outputsToolbarLeft" },
        el("input", {
          type: "time",
          value: formatTimeInput(o.autoOnHour, o.autoOnMinute),
          oninput: (e) => assignTimeToObject(o, "autoOnHour", "autoOnMinute", e.target.value)
        }),
        Btn("Set ON", { small: true, variant: "primary", onClick: () => setOutputAutoOn(o.id), disabled: state.loading }),
        el("input", {
          type: "time",
          value: formatTimeInput(o.autoOffHour, o.autoOffMinute),
          oninput: (e) => assignTimeToObject(o, "autoOffHour", "autoOffMinute", e.target.value)
        }),
        Btn("Set OFF", { small: true, variant: "primary", onClick: () => setOutputAutoOff(o.id), disabled: state.loading }),
        o.manualMode
          ? Btn("Set AUTO", { small: true, variant: "danger", onClick: () => setOutputManual(o.id, false), disabled: state.loading })
          : Btn("Set MANUAL", { small: true, onClick: () => setOutputManual(o.id, true), disabled: state.loading })
      )
    );
  }

  function SoundsView() {
    const right = el("div", { class: "kv" },
      "Last: ", el("b", null, state.lastRefreshMs ? new Date(state.lastRefreshMs).toLocaleTimeString() : "-")
    );

    const toolbar = el("div", { class: "outputsToolbar" },
      el("div", { class: "outputsToolbarLeft" },
        Btn("Refresh", { variant: "primary", onClick: refreshSounds, disabled: state.loading })
      ),
      el("div", { class: "outputsToolbarRight" },
        state.loading ? el("span", { class: "pill" }, "working...") : null,
        state.error ? el("span", { class: "pill" }, state.error) : null
      )
    );

    const volumeRow = el("div", { class: "outputTile" },
      el("div", { class: "outputLeft" },
        el("div", { class: "outputName" }, "Global sound volume"),
        el("div", { class: "outputMeta" },
          el("span", { class: "pill" }, `value=${state.sounds.globalSoundVolume}`)
        )
      ),
      el("div", { class: "outputsToolbarLeft" },
        el("input", {
          type: "number",
          min: "0",
          max: "100",
          value: state.sounds.globalSoundVolume,
          style: "width:90px;",
          oninput: (e) => {
            state.sounds.globalSoundVolume = clamp100(e.target.value);
          }
        }),
        Btn("Set volume", { variant: "primary", onClick: () => setGlobalSoundVolume(state.sounds.globalSoundVolume), disabled: state.loading })
      )
    );

    const manualPlayRow = el("div", { class: "outputTile" },
      el("div", { class: "outputLeft" },
        el("div", { class: "outputName" }, "Manual play"),
        el("div", { class: "outputMeta" },
          el("span", { class: "pill" }, "on demand"),
          el("span", { class: "pill" }, "no scheduler")
        )
      ),
      el("div", { class: "outputsToolbarLeft" },
        el("input", {
          type: "text",
          value: state.sounds.manualFile,
          placeholder: "alarm.mp3",
          style: "width:180px;",
          oninput: (e) => {
            state.sounds.manualFile = e.target.value;
          }
        }),
        el("input", {
          type: "number",
          min: "0",
          max: "100",
          value: state.sounds.manualVolume,
          style: "width:90px;",
          oninput: (e) => {
            state.sounds.manualVolume = clamp100(e.target.value);
          }
        }),
        Btn("Play", { variant: "primary", onClick: () => playSoundManual(false), disabled: state.loading }),
        Btn("Play current", { onClick: () => playSoundManual(true), disabled: state.loading })
      )
    );

    const body = el("div", null,
      toolbar,
      volumeRow,
      manualPlayRow,
      el("div", { class: "footerNote" },
        "Funkcje schedulera dzwiekow przeniesione do zakladki Sound scheduler."
      )
    );

    return Card("Sounds", right, body);
  }

  function SoundSchedulerView() {
    const right = el("div", { class: "kv" },
      "Last: ", el("b", null, state.lastRefreshMs ? new Date(state.lastRefreshMs).toLocaleTimeString() : "-")
    );

    const soundAutoPill = el("span", { class: `pill ${state.sounds.globalSoundAutoMode ? "on" : "off"}` },
      state.sounds.globalSoundAutoMode ? "AUTO ON" : "AUTO OFF"
    );

    const toolbar = el("div", { class: "outputsToolbar" },
      el("div", { class: "outputsToolbarLeft" },
        Btn("Refresh", { variant: "primary", onClick: refreshSounds, disabled: state.loading }),
        Btn("Auto sounds ON", { onClick: () => setSoundAuto(true), disabled: state.loading }),
        Btn("Auto sounds OFF", { variant: "danger", onClick: () => setSoundAuto(false), disabled: state.loading }),
        soundAutoPill
      ),
      el("div", { class: "outputsToolbarRight" },
        state.loading ? el("span", { class: "pill" }, "working...") : null,
        state.error ? el("span", { class: "pill" }, state.error) : null
      )
    );

    const grid = el("div", { class: "outputsGrid" },
      state.sounds.soundAlarms.map(s => SoundSchedulerTile(s))
    );

    const body = el("div", null,
      toolbar,
      grid,
      el("div", { class: "footerNote" },
        "Tu sa auto sounds oraz sloty alarmow. Sounds zostal tylko do recznego play i volume."
      )
    );

    return Card("Sound scheduler", right, body);
  }

  function SoundSchedulerTile(s) {
    const enabledPill = el("span", { class: `pill ${s.enabled ? "on" : "off"}` }, s.enabled ? "ENABLED" : "DISABLED");

    return el("div", { class: "outputTile" },
      el("div", { class: "outputLeft" },
        el("div", { class: "outputName" }, `SLOT ${s.slot}`),
        el("div", { class: "outputMeta" },
          enabledPill,
          el("span", { class: "pill" }, formatTimeLabel(s.hour, s.minute)),
          el("span", { class: "pill" }, `vol=${s.volume}`)
        ),
        el("div", { class: "footerNote" }, `file=${s.file || "-"}`)
      ),
      el("div", { class: "outputsToolbarLeft" },
        el("input", {
          type: "time",
          value: formatTimeInput(s.hour, s.minute),
          oninput: (e) => assignTimeToObject(s, "hour", "minute", e.target.value)
        }),
        el("input", {
          type: "text",
          value: s.file,
          placeholder: "alarm.mp3",
          style: "width:160px;",
          oninput: (e) => {
            s.file = e.target.value;
          }
        }),
        el("input", {
          type: "number",
          min: "0",
          max: "100",
          value: s.volume,
          style: "width:90px;",
          oninput: (e) => {
            s.volume = clamp100(e.target.value);
          }
        }),
        Btn("Save", { small: true, variant: "primary", onClick: () => saveSoundAlarm(s.slot), disabled: state.loading }),
        s.enabled
          ? Btn("Disable", { small: true, variant: "danger", onClick: () => setSoundAlarmEnabled(s.slot, false), disabled: state.loading })
          : Btn("Enable", { small: true, onClick: () => setSoundAlarmEnabled(s.slot, true), disabled: state.loading })
      )
    );
  }

  function CamView() {
    return Card("Cam", null, el("div", null,
      el("div", { class: "kv" }, "Placeholder."),
      el("div", { class: "footerNote" }, "Dopniemy potem stream kamery.")
    ));
  }

  function CamSchedulerView() {
    const right = el("div", { class: "kv" },
      "Last: ", el("b", null, state.lastRefreshMs ? new Date(state.lastRefreshMs).toLocaleTimeString() : "-")
    );

    const toolbar = el("div", { class: "outputsToolbar" },
      el("div", { class: "outputsToolbarLeft" },
        Btn("Refresh UI", { variant: "primary", onClick: render, disabled: state.loading }),
        el("span", { class: "pill" }, "UI only")
      ),
      el("div", { class: "outputsToolbarRight" },
        state.loading ? el("span", { class: "pill" }, "working...") : null,
        state.error ? el("span", { class: "pill" }, state.error) : null
      )
    );

    const row = el("div", { class: "outputTile" },
      el("div", { class: "outputLeft" },
        el("div", { class: "outputName" }, "Cam scheduler"),
        el("div", { class: "outputMeta" },
          el("span", { class: "pill" }, `auto on ${formatTimeLabel(state.camScheduler.autoOnHour, state.camScheduler.autoOnMinute)}`),
          el("span", { class: "pill" }, `auto off ${formatTimeLabel(state.camScheduler.autoOffHour, state.camScheduler.autoOffMinute)}`)
        ),
        el("div", { class: "footerNote" }, "Brak endpointow backendu dla cam scheduler w tej wersji.")
      ),
      el("div", { class: "outputsToolbarLeft" },
        el("input", {
          type: "time",
          value: formatTimeInput(state.camScheduler.autoOnHour, state.camScheduler.autoOnMinute),
          oninput: (e) => assignTimeToObject(state.camScheduler, "autoOnHour", "autoOnMinute", e.target.value)
        }),
        el("input", {
          type: "time",
          value: formatTimeInput(state.camScheduler.autoOffHour, state.camScheduler.autoOffMinute),
          oninput: (e) => assignTimeToObject(state.camScheduler, "autoOffHour", "autoOffMinute", e.target.value)
        })
      )
    );

    const body = el("div", null,
      toolbar,
      row,
      el("div", { class: "footerNote" },
        "Ta zakladka jest przygotowana pod przyszly backend kamery. Teraz nic nie wysyla do /api."
      )
    );

    return Card("Cam scheduler", right, body);
  }

  function App() {
    const view =
      state.route === "outputs" ? OutputsView()
      : state.route === "outputs_scheduler" ? OutputsSchedulerView()
      : state.route === "sounds" ? SoundsView()
      : state.route === "sound_scheduler" ? SoundSchedulerView()
      : state.route === "cam" ? CamView()
      : state.route === "cam_scheduler" ? CamSchedulerView()
      : HomeView();

    return el("div", { class: "app" },
      TopBar(),
      el("div", { class: "content" },
        el("div", { class: "grid" }, view)
      )
    );
  }

  function render() {
    const root = document.getElementById("app");
    root.innerHTML = "";
    root.appendChild(App());
  }

  render();
  refreshOutputs();
  startDeviceClockTicker();
})();
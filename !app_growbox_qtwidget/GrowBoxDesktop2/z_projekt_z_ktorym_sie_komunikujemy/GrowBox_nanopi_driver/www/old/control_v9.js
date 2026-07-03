(() => {
  // Komentarze krotkie, po polsku, ascii

  const API = {
    status: "/api/status",
    outputOn: (id) => `/api/output/${id}/on`,
    outputOff: (id) => `/api/output/${id}/off`,
    outputsOnAll: "/api/outputs/on_all",
    outputsOffAll: "/api/outputs/off_all",
    soundStatus: "/api/sound/status",
    soundAutoOn: "/api/sound/auto/on",
    soundAutoOff: "/api/sound/auto/off",
    soundVolumeGet: "/api/sound/volume/get",
    soundVolumeSet: (value) => `/api/sound/volume/set?value=${encodeURIComponent(value)}`,
    soundPlay: (file, volume) => `/api/sound/play?file=${encodeURIComponent(file)}&volume=${encodeURIComponent(volume)}`,
    soundPlayCurrent: (file) => `/api/sound/play_current?file=${encodeURIComponent(file)}`,
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
    return el("button", { class: cls, onclick: opts.onClick, type: "button", disabled: !!opts.disabled }, label);
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

  const state = {
    route: "home",
    outputs: Array.from({ length: 16 }, (_, i) => ({
      id: i + 1,
      name: `OUT${i + 1}`,
      state: false,
    })),
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
    lastRefreshMs: 0,
    loading: false,
    error: "",
  };

  async function httpGetJson(url) {
    const res = await fetch(url, { method: "GET", cache: "no-store" });
    const text = await res.text();
    let data = null;
    try { data = text ? JSON.parse(text) : null; } catch { /* ignore */ }
    if (!res.ok) {
      const msg = (data && (data.error || data.message)) ? (data.error || data.message) : `${res.status} ${res.statusText}`;
      throw new Error(msg);
    }
    return data;
  }

  async function httpGet(url) {
    const res = await fetch(url, { method: "GET", cache: "no-store" });
    const text = await res.text();
    let data = null;
    try { data = text ? JSON.parse(text) : null; } catch { /* ignore */ }
    if (!res.ok) {
      const msg = (data && (data.error || data.message)) ? (data.error || data.message) : `${res.status} ${res.statusText}`;
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
        for (const o of data.outputs) {
          const id = Number(o.id);
          const idx = state.outputs.findIndex(x => x.id === id);
          if (idx >= 0) state.outputs[idx].state = !!o.state;
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
        if (typeof data.globalSoundVolume === "number") state.sounds.globalSoundVolume = data.globalSoundVolume;
        if (typeof data.globalSoundAutoMode === "boolean") state.sounds.globalSoundAutoMode = data.globalSoundAutoMode;
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
    } catch (e) {
      state.error = `Set error: ${e.message || e}`;
    } finally {
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
      for (const o of state.outputs) o.state = !!on;
      state.lastRefreshMs = Date.now();
      await refreshOutputs();
    } catch (e) {
      state.error = `SetAll error: ${e.message || e}`;
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
      const v = Math.max(0, Math.min(100, Number(value) || 0));
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
        const v = Math.max(0, Math.min(100, Number(state.sounds.manualVolume) || 0));
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

  function TopBar() {
    const badgeText = state.loading ? "busy..." : (state.error ? "error" : "ok");

    const navBtn = (route, label) => Btn(label, {
      variant: state.route === route ? "primary" : "ghost",
      disabled: state.loading && state.route !== route,
      onClick: async () => {
        state.route = route;
        render();
        if (route === "outputs") await refreshOutputs();
        if (route === "sounds") await refreshSounds();
      },
    });

    return el("div", { class: "topbar" },
      el("div", { class: "topbarRow" },
        el("div", { class: "brand" },
          el("div", { class: "logoDot" }),
          el("div", null,
            el("div", { class: "brandTitle" }, "GROWBOX - control_v9"),
            el("div", { class: "brandSub" }, "green/black UI")
          )
        ),
        el("div", { class: "nav" },
          navBtn("home", "Home"),
          navBtn("outputs", "Outputs"),
          navBtn("sounds", "Sounds"),
          navBtn("cam", "Cam"),
          el("div", { class: "badge" }, badgeText)
        )
      )
    );
  }

  function HomeView() {
    const body = el("div", null,
      el("div", { class: "kv" },
        "Start: ", el("b", null, "Home"), " | ",
        "Menu: ", el("b", null, "Outputs / Sounds / Cam"), " | ",
        "SetAll: ", el("b", null, "/api/outputs/on_all i /api/outputs/off_all")
      ),
      el("div", { class: "footerNote" },
        "Sounds: reczne play, global volume, global sound auto on/off i podglad slotow."
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
        Btn("Off all", { variant: "danger", onClick: () => setAllOutputs(false), disabled: state.loading }),
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
        "ON/OFF to stan z /api/status. SetAll uzywa /api/outputs/on_all i /api/outputs/off_all."
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

  function SoundsView() {
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
            state.sounds.globalSoundVolume = Math.max(0, Math.min(100, Number(e.target.value) || 0));
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
          el("span", { class: "pill" }, "does not use auto toggle")
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
            state.sounds.manualVolume = Math.max(0, Math.min(100, Number(e.target.value) || 0));
          }
        }),
        Btn("Play", { variant: "primary", onClick: () => playSoundManual(false), disabled: state.loading }),
        Btn("Play current", { onClick: () => playSoundManual(true), disabled: state.loading })
      )
    );

    const slots = el("div", { class: "outputsGrid" },
      state.sounds.soundAlarms.map(s => SoundTile(s))
    );

    const body = el("div", null,
      toolbar,
      volumeRow,
      manualPlayRow,
      slots,
      el("div", { class: "footerNote" },
        "Auto sounds OFF nie zmienia enable/disable slotow. Tylko blokuje ich automatyczne odtwarzanie."
      )
    );

    return Card("Sounds", right, body);
  }

  function SoundTile(s) {
    const enabledPill = el("span", { class: `pill ${s.enabled ? "on" : "off"}` }, s.enabled ? "ENABLED" : "DISABLED");
    const timeText = s.hour >= 0 ? `${String(s.hour).padStart(2, "0")}:${String(s.minute).padStart(2, "0")}` : "--:--";

    const toggleBtn = s.enabled
      ? Btn("Disable", { small: true, variant: "danger", onClick: () => setSoundAlarmEnabled(s.slot, false), disabled: state.loading })
      : Btn("Enable", { small: true, variant: "primary", onClick: () => setSoundAlarmEnabled(s.slot, true), disabled: state.loading });

    return el("div", { class: "outputTile" },
      el("div", { class: "outputLeft" },
        el("div", { class: "outputName" }, `SLOT ${s.slot}`),
        el("div", { class: "outputMeta" },
          enabledPill,
          el("span", { class: "pill" }, timeText),
          el("span", { class: "pill" }, `vol=${s.volume}`)
        ),
        el("div", { class: "footerNote" }, `file=${s.file || "-"}`)
      ),
      el("div", null, toggleBtn)
    );
  }

  function CamView() {
    return Card("Cam", null, el("div", null,
      el("div", { class: "kv" }, "Placeholder."),
      el("div", { class: "footerNote" }, "Dopniemy potem stream kamery.")
    ));
  }

  function App() {
    const view =
      state.route === "outputs" ? OutputsView()
      : state.route === "sounds" ? SoundsView()
      : state.route === "cam" ? CamView()
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
})();
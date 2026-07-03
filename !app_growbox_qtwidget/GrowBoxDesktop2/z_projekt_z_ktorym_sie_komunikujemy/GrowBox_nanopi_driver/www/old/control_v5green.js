(() => {
  const OUTPUT_COUNT = 16;
  const SOUND_SLOT_COUNT = 10;

  // --------------------------
  // Mini Flutter-like core
  // --------------------------
  class State {
    constructor(v) { this._v = v; this._subs = []; }
    get value() { return this._v; }
    set(v) { this._v = v; for (const fn of this._subs) fn(v); }
    update(fn) { this.set(fn(this._v)); }
    subscribe(fn) { this._subs.push(fn); return () => { this._subs = this._subs.filter(x => x !== fn); }; }
  }

  function h(tag, props, ...children) {
    const el = document.createElement(tag);
    if (props) {
      for (const [k, v] of Object.entries(props)) {
        if (v === undefined || v === null) continue;

        if (k === "class") el.className = v;
        else if (k === "text") el.textContent = String(v);
        else if (k === "html") el.innerHTML = String(v);
        else if (k === "style" && typeof v === "object") Object.assign(el.style, v);
        else if (k.startsWith("on") && typeof v === "function") {
          el.addEventListener(k.substring(2).toLowerCase(), v);
        } else if (k in el) {
          try { el[k] = v; } catch { el.setAttribute(k, String(v)); }
        } else {
          el.setAttribute(k, String(v));
        }
      }
    }
    for (const c of children.flat()) {
      if (c === undefined || c === null) continue;
      if (typeof c === "string" || typeof c === "number") el.appendChild(document.createTextNode(String(c)));
      else el.appendChild(c);
    }
    return el;
  }

  const Text = (t, cls) => h("span", { class: cls || "", text: t });
  const H1 = (t) => h("h1", { text: t });
  const H2 = (t) => h("h2", { text: t });
  const H3 = (t) => h("h3", { text: t });

  const Row = (...kids) => h("div", { class: "row" }, kids);
  const Column = (...kids) => h("div", { class: "stack" }, kids);
  const Card = (...kids) => h("div", { class: "card" }, kids);
  const Section = (title, ...kids) => h("section", { class: "section" }, [H2(title), ...kids]);
  const Badge = (html) => h("span", { class: "badge", html });

  const Btn = (label, cls, onClick) =>
    h("button", { class: `btn ${cls || ""}`.trim(), onclick: onClick, text: label });

  const InputNumber = (id, value, min, max, cls) =>
    h("input", { id, class: `inp ${cls || ""}`.trim(), type: "number", value, min, max });

  const InputText = (id, value, placeholder, cls) =>
    h("input", { id, class: `inp ${cls || ""}`.trim(), type: "text", value, placeholder });

  function qs(params) {
    const p = new URLSearchParams();
    for (const k of Object.keys(params)) {
      const v = params[k];
      if (v === undefined || v === null) continue;
      p.set(k, String(v));
    }
    const s = p.toString();
    return s ? `?${s}` : "";
  }

  function pad2(n) { return String(n).padStart(2, "0"); }
  function nowStr() {
    const d = new Date();
    return `${pad2(d.getHours())}:${pad2(d.getMinutes())}:${pad2(d.getSeconds())}`;
  }

  // --------------------------
  // State
  // --------------------------
  const st = {
    status: new State(null),
    statusText: new State(""),
    logText: new State(""),
    toast: new State(null), // { text }
  };

  function setToast(text) {
    st.toast.set({ text });
    setTimeout(() => {
      if (st.toast.value && st.toast.value.text === text) st.toast.set(null);
    }, 1400);
  }

  function logLine(text) {
    st.logText.update(prev => `[${nowStr()}] ${text}\n` + prev);
  }

  async function apiGet(path) {
    logLine(`GET ${path}`);
    try {
      const r = await fetch(path, { method: "GET" });
      const ct = r.headers.get("content-type") || "";
      let body = null;

      if (ct.includes("application/json")) body = await r.json();
      else body = await r.text();

      if (!r.ok) {
        logLine(`HTTP ${r.status}: ${JSON.stringify(body)}`);
        setToast(`HTTP ${r.status}`);
        return { ok: false, status: r.status, body };
      }

      logLine(`OK: ${JSON.stringify(body)}`);
      return { ok: true, status: r.status, body };
    } catch (e) {
      logLine(`ERR: ${String(e)}`);
      setToast(`ERR`);
      return { ok: false, status: 0, body: String(e) };
    }
  }

  async function refreshAll() {
    const res = await apiGet("/api/status");
    if (res.ok) {
      st.status.set(res.body);
      st.statusText.set(JSON.stringify(res.body, null, 2));
      setToast("OK");
    } else {
      st.status.set(null);
      st.statusText.set(`Error: ${res.status}\n` + JSON.stringify(res.body, null, 2));
    }
  }

  // --------------------------
  // Widgets
  // --------------------------
  function OutputCard(id) {
    const s = st.status.value;
    const out = (s && Array.isArray(s.outputs)) ? s.outputs.find(x => x.id === id) : null;

    const stateTxt = out ? (out.state ? "ON" : "OFF") : "?";
    const manualTxt = out ? (out.manualMode ? "ON" : "OFF") : "?";
    const aonTxt = out ? `${pad2(out.autoOnHour)}:${pad2(out.autoOnMinute)}` : "--:--";
    const aoffTxt = out ? `${pad2(out.autoOffHour)}:${pad2(out.autoOffMinute)}` : "--:--";

    const inpOnH = `inp_out_${id}_aon_h`;
    const inpOnM = `inp_out_${id}_aon_m`;
    const inpOffH = `inp_out_${id}_aoff_h`;
    const inpOffM = `inp_out_${id}_aoff_m`;

    const call = async (path) => { await apiGet(path); await refreshAll(); };

    return Card(
      H3(`Output #${id}`),

      Row(
        Badge(`<span>Stan:</span> <span class="kv">${stateTxt}</span>
               <span>Manual:</span> <span class="kv">${manualTxt}</span>`),

        Btn("ON", "btn-green", () => call(`/api/output/${id}/on`)),
        Btn("OFF", "btn-red", () => call(`/api/output/${id}/off`)),
        Btn("manual ON", "", () => call(`/api/output/${id}/manual/on`)),
        Btn("manual OFF", "", () => call(`/api/output/${id}/manual/off`))
      ),

      Row(
        Badge(`<span>Auto ON:</span> <span class="kv">${aonTxt}</span>
               <span>Auto OFF:</span> <span class="kv">${aoffTxt}</span>`)
      ),

      Row(
        Text("autoon hour/min"),
        InputNumber(inpOnH, 18, 0, 23, "small"),
        InputNumber(inpOnM, 0, 0, 59, "small"),
        Btn("set auto ON", "btn-primary", async () => {
          const hour = Number(document.getElementById(inpOnH).value);
          const minute = Number(document.getElementById(inpOnM).value);
          await call(`/api/output/${id}/autoon` + qs({ hour, minute }));
        })
      ),

      Row(
        Text("autooff hour/min"),
        InputNumber(inpOffH, 22, 0, 23, "small"),
        InputNumber(inpOffM, 0, 0, 59, "small"),
        Btn("set auto OFF", "btn-primary", async () => {
          const hour = Number(document.getElementById(inpOffH).value);
          const minute = Number(document.getElementById(inpOffM).value);
          await call(`/api/output/${id}/autooff` + qs({ hour, minute }));
        })
      )
    );
  }

  function SoundAlarmCard(slot) {
    const s = st.status.value;
    const a = (s && Array.isArray(s.soundAlarms)) ? s.soundAlarms.find(x => x.slot === slot) : null;

    const enTxt = a ? (a.enabled ? "1" : "0") : "?";
    const timeTxt = a ? `${pad2(a.hour)}:${pad2(a.minute)}` : "--:--";
    const volTxt = a ? String(a.volume) : "--";

    const inpEn = `inp_snd_${slot}_en`;
    const inpH  = `inp_snd_${slot}_h`;
    const inpM  = `inp_snd_${slot}_m`;
    const inpFile = `inp_snd_${slot}_file`;
    const inpVol  = `inp_snd_${slot}_vol`;

    const call = async (path) => { await apiGet(path); await refreshAll(); };

    return Card(
      H3(`Alarm slot #${slot}`),

      Row(
        Badge(`<span>enabled:</span> <span class="kv">${enTxt}</span>
               <span>time:</span> <span class="kv">${timeTxt}</span>
               <span>vol:</span> <span class="kv">${volTxt}</span>`)
      ),

      Row(
        Btn("enable", "btn-green", () => call(`/api/sound/alarm/${slot}/enable`)),
        Btn("disable", "btn-red", () => call(`/api/sound/alarm/${slot}/disable`))
      ),

      Row(
        Text("enabled(0/1)"),
        InputNumber(inpEn, 1, 0, 1, "small"),
        Text("hour"),
        InputNumber(inpH, 18, 0, 23, "small"),
        Text("minute"),
        InputNumber(inpM, 30, 0, 59, "small"),
        Btn("set", "btn-primary", async () => {
          const enabled = Number(document.getElementById(inpEn).value);
          const hour = Number(document.getElementById(inpH).value);
          const minute = Number(document.getElementById(inpM).value);
          const file = String(document.getElementById(inpFile).value || "");
          const volume = Number(document.getElementById(inpVol).value);
          await call(`/api/sound/alarm/${slot}/set` + qs({ enabled, hour, minute, file, volume }));
        })
      ),

      Row(
        Text("file"),
        InputText(inpFile, "alarm.mp3", "np. alarm.mp3", ""),
        Text("volume"),
        InputNumber(inpVol, 70, 0, 100, "small")
      )
    );
  }

  function SoundPanel() {
    const call = async (path) => { await apiGet(path); await refreshAll(); };

    return Section("Sound",
      h("div", { class: "grid2" },
        Card(
          H3("Global Volume"),
          Row(Btn("GET /api/sound/volume/get", "", () => call("/api/sound/volume/get"))),
          Row(
            h("label", { class: "lbl", text: "value (0..100)" }),
            InputNumber("inpVolSet", 100, 0, 100, ""),
            Btn("SET", "btn-primary", async () => {
              const value = Number(document.getElementById("inpVolSet").value);
              await call("/api/sound/volume/set" + qs({ value }));
            })
          )
        ),

        Card(
          H3("Play now"),
          Row(
            h("label", { class: "lbl", text: "file" }),
            InputText("inpPlayFile", "alarm.mp3", "np. alarm.mp3", "")
          ),
          Row(
            h("label", { class: "lbl", text: "volume (0..100)" }),
            InputNumber("inpPlayVol", 70, 0, 100, "")
          ),
          Row(
            Btn("/api/sound/play", "btn-primary", async () => {
              const file = String(document.getElementById("inpPlayFile").value || "");
              const volume = Number(document.getElementById("inpPlayVol").value);
              await call("/api/sound/play" + qs({ file, volume }));
            }),
            Btn("/api/sound/play_current", "", async () => {
              const file = String(document.getElementById("inpPlayFile").value || "");
              await call("/api/sound/play_current" + qs({ file }));
            })
          )
        )
      ),

      Row(
        Btn("GET /api/sound/status", "", () => call("/api/sound/status")),
        Text("Zwraca globalSoundVolume + soundAlarms[]", "hint")
      ),

      h("h3", { class: "subh", text: "Sound alarms (1..10)" }),
      Column(...Array.from({ length: SOUND_SLOT_COUNT }, (_, i) => SoundAlarmCard(i + 1)))
    );
  }

  function App() {
    const statusPre = h("pre", { class: "pre", text: st.statusText.value || "" });
    const logPre = h("pre", { class: "pre pre-log", text: st.logText.value || "" });

    const toast = st.toast.value
      ? h("div", { class: "toast", text: st.toast.value.text })
      : null;

    return h("div", { class: "box" },
      h("header", { class: "header" },
        H1("Growbox Control v5 green"),
        h("div", { class: "top-actions" },
          Btn("Odswiez wszystko", "btn-primary", refreshAll),
          Btn("Wyczysc log", "", () => st.logText.set(""))
        )
      ),

      Section("Status",
        Row(
          Btn("GET /api/status", "btn-primary", refreshAll),
          Text("Pobiera caly snapshot (outputs + sound).", "hint")
        ),
        statusPre
      ),

      Section("System",
        h("div", { class: "grid2" },
          Card(
            H3("Global Auto Mode"),
            Row(
              Btn("/api/system/auto/on", "btn-green", async () => { await apiGet("/api/system/auto/on"); await refreshAll(); }),
              Btn("/api/system/auto/off", "btn-red", async () => { await apiGet("/api/system/auto/off"); await refreshAll(); })
            ),
            Row(Text("Wplywa na dzialanie automatyki czasowej.", "hint"))
          ),
          Card(
            H3("Log odpowiedzi"),
            Row(Text("Klikasz -> widzisz JSON i endpoint w logu nizej.", "hint"))
          )
        )
      ),

      Section("Outputs (1..16)",
        Column(...Array.from({ length: OUTPUT_COUNT }, (_, i) => OutputCard(i + 1)))
      ),

      SoundPanel(),

      Section("Log", logPre),

      toast
    );
  }

  // --------------------------
  // Render
  // --------------------------
  let mount = null;

  function rerender() {
    if (!mount) return;
    mount.innerHTML = "";
    mount.appendChild(App());
  }

  function bindRerender() {
    [st.status, st.statusText, st.logText, st.toast].forEach(s => s.subscribe(() => rerender()));
  }

  window.addEventListener("DOMContentLoaded", async () => {
    mount = document.getElementById("app");
    bindRerender();
    rerender();
    await refreshAll();
  });
})();
/* control_v7g.js */

(() => {
  // Komentarze krotkie, po polsku, ascii

  // ===== API (podmien pod swoje endpointy) =====
  const API = {
    // Stan wyjsc: GET -> { outputs: [ { id:1, state:true, name:"OUT1" }, ... ] }
    outputsStatus: "/api/outputs/status",

    // Ustawienie pojedynczego: POST { id: 1, state: true }
    outputsSet: "/api/outputs/set",

    // Masowe: POST { state: true } albo { state:false }
    outputsSetAll: "/api/outputs/set_all",
  };

  // ===== Mini "flutter-like" UI helper =====
  function el(tag, attrs, ...children) {
    const n = document.createElement(tag);
    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) {
        if (k === "class") n.className = v;
        else if (k === "style") n.setAttribute("style", v);
        else if (k.startsWith("on") && typeof v === "function") n.addEventListener(k.slice(2), v);
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
    return el("button", { class: cls, onclick: opts.onClick, type: "button" }, label);
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

  // ===== App state =====
  const state = {
    route: "home", // home | outputs | sounds | cam
    outputs: Array.from({ length: 16 }, (_, i) => ({
      id: i + 1,
      name: `OUT${i + 1}`,
      state: false,
      lastSeen: null,
    })),
    lastRefreshMs: 0,
    loading: false,
    error: "",
  };

  // ===== HTTP =====
  async function httpJson(url, opts = {}) {
    const res = await fetch(url, {
      method: opts.method || "GET",
      headers: { "Content-Type": "application/json", ...(opts.headers || {}) },
      body: opts.body ? JSON.stringify(opts.body) : undefined,
      cache: "no-store",
    });
    const text = await res.text();
    let data = null;
    try { data = text ? JSON.parse(text) : null; } catch { /* ignore */ }
    if (!res.ok) {
      const msg = (data && (data.error || data.message)) ? (data.error || data.message) : `${res.status} ${res.statusText}`;
      throw new Error(msg);
    }
    return data;
  }

  // ===== Outputs logic =====
  async function refreshOutputs() {
    state.loading = true;
    state.error = "";
    render();

    try {
      const data = await httpJson(API.outputsStatus);
      // oczekiwane: data.outputs = [{id,state,name?},...]
      if (data && Array.isArray(data.outputs)) {
        const now = Date.now();
        for (const o of data.outputs) {
          const idx = state.outputs.findIndex(x => x.id === o.id);
          if (idx >= 0) {
            state.outputs[idx].state = !!o.state;
            if (o.name) state.outputs[idx].name = String(o.name);
            state.outputs[idx].lastSeen = now;
          }
        }
        state.lastRefreshMs = now;
      }
    } catch (e) {
      state.error = `Refresh error: ${e.message || e}`;
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
      await httpJson(API.outputsSet, { method: "POST", body: { id, state: !!on } });
      // optimistycznie
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
      await httpJson(API.outputsSetAll, { method: "POST", body: { state: !!on } });
      for (const o of state.outputs) o.state = !!on;
      state.lastRefreshMs = Date.now();
    } catch (e) {
      state.error = `SetAll error: ${e.message || e}`;
    } finally {
      state.loading = false;
      render();
    }
  }

  // ===== Views =====
  function TopBar() {
    const badgeText = state.loading ? "busy..." : (state.error ? "error" : "ok");

    const navBtn = (route, label) => Btn(label, {
      variant: state.route === route ? "primary" : "ghost",
      onClick: () => { state.route = route; render(); },
    });

    return el("div", { class: "topbar" },
      el("div", { class: "topbarRow" },
        el("div", { class: "brand" },
          el("div", { class: "logoDot" }),
          el("div", null,
            el("div", { class: "brandTitle" }, "GROWBOX - control_v7g"),
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
    const info = el("div", { class: "kv" },
      "Start: ", el("b", null, "Home"), " | ",
      "Menu: ", el("b", null, "Outputs / Sounds / Cam"), " | ",
      "Status: ", el("b", null, state.loading ? "busy" : (state.error ? "error" : "ready"))
    );

    const body = el("div", null,
      info,
      state.error ? el("div", { class: "footerNote" }, state.error) : null,
      el("div", { class: "footerNote" },
        "Tip: w Outputs masz 16 wyjsc + OnAll/OffAll/Refresh. Endpointy ustawisz w control_v7g.js (sekcja API)."
      )
    );

    return Card("Strona glowna", null, body);
  }

  function OutputsView() {
    const right = el("div", { class: "kv" },
      "Last: ", el("b", null, state.lastRefreshMs ? new Date(state.lastRefreshMs).toLocaleTimeString() : "-")
    );

    const toolbar = el("div", { class: "outputsToolbar" },
      el("div", { class: "outputsToolbarLeft" },
        Btn("Refresh", { variant: "primary", onClick: refreshOutputs }),
        Btn("On all", { onClick: () => setAllOutputs(true) }),
        Btn("Off all", { variant: "danger", onClick: () => setAllOutputs(false) }),
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
        "Kontrolka ON/OFF to stan z /api/outputs/status (albo Twojego). Klikniecie wysyla /api/outputs/set."
      )
    );

    return Card("Outputs", right, body);
  }

  function OutputTile(o) {
    const pill = el("span", { class: `pill ${o.state ? "on" : "off"}` }, o.state ? "ON" : "OFF");
    const meta = el("div", { class: "outputMeta" },
      pill,
      el("span", { class: "pill" }, `id=${o.id}`)
    );

    const toggleBtn = o.state
      ? Btn("Off", { small: true, variant: "danger", onClick: () => setOutput(o.id, false) })
      : Btn("On", { small: true, variant: "primary", onClick: () => setOutput(o.id, true) });

    return el("div", { class: "outputTile" },
      el("div", { class: "outputLeft" },
        el("div", { class: "outputName" }, o.name),
        meta
      ),
      el("div", null, toggleBtn)
    );
  }

  function SoundsView() {
    const body = el("div", null,
      el("div", { class: "kv" }, "Tu dodamy przyciski dzwiekow (play/stop/volume)."),
      el("div", { class: "footerNote" }, "Na razie placeholder.")
    );
    return Card("Sounds", null, body);
  }

  function CamView() {
    const body = el("div", null,
      el("div", { class: "kv" }, "Tu dodamy podglad kamery (img/mjpeg/webrtc) + sterowanie."),
      el("div", { class: "footerNote" }, "Na razie placeholder.")
    );
    return Card("Cam", null, body);
  }

  // ===== Render =====
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

  // start
  render();
  // opcjonalnie: auto refresh przy wejsciu w outputs
  // refreshOutputs();
})();
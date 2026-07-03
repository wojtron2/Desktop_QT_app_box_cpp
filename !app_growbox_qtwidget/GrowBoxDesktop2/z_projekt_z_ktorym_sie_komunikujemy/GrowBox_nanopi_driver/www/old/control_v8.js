(() => {
  // Komentarze krotkie, po polsku, ascii

  const API = {
    status: "/api/status",
    outputOn: (id) => `/api/output/${id}/on`,
    outputOff: (id) => `/api/output/${id}/off`,
    outputsOnAll: "/api/outputs/on_all",
    outputsOffAll: "/api/outputs/off_all",
  };

  function el(tag, attrs, ...children) {
    const n = document.createElement(tag);
    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) {
        if (k === "class") n.className = v;
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
      for (const o of state.outputs) o.state = !!on; // szybki update UI
      state.lastRefreshMs = Date.now();
      // opcjonalnie dociagnij prawdziwy status:
      await refreshOutputs();
    } catch (e) {
      state.error = `SetAll error: ${e.message || e}`;
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
      },
    });

    return el("div", { class: "topbar" },
      el("div", { class: "topbarRow" },
        el("div", { class: "brand" },
          el("div", { class: "logoDot" }),
          el("div", null,
            el("div", { class: "brandTitle" }, "GROWBOX - control_v8"),
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
    return Card("Sounds", null, el("div", null,
      el("div", { class: "kv" }, "Placeholder."),
      el("div", { class: "footerNote" }, "Dopniemy potem endpointy dzwieku.")
    ));
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
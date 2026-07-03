const OUTPUT_COUNT = 16;

async function api(url) {
    const res = await fetch(url);
    return await res.json();
}

async function refresh() {

    const data = await api("/api/status");

    document.getElementById("globalStatus").innerText =
        "Global AUTO: " + (data.globalAutoMode ? "ON" : "OFF");

    const container = document.getElementById("outputs");
    container.innerHTML = "";

    data.outputs.forEach(o => {

        const div = document.createElement("div");
        div.style.border = "1px solid gray";
        div.style.margin = "5px";
        div.style.padding = "5px";

        div.innerHTML = `
            <h3>Output ${o.id}</h3>
            <div>State: ${o.state ? "ON" : "OFF"}</div>
            <div>Manual: ${o.manualMode ? "ON" : "OFF"}</div>
            <div>Auto ON: ${o.autoOnHour}</div>
            <div>Auto OFF: ${o.autoOffHour}</div>

            <button onclick="setOn(${o.id})">ON</button>
            <button onclick="setOff(${o.id})">OFF</button>

            <button onclick="setManual(${o.id}, true)">Manual ON</button>
            <button onclick="setManual(${o.id}, false)">Manual OFF</button>

            <br>

            <input type="number" id="on${o.id}" min="0" max="23">
            <button onclick="setAutoOn(${o.id})">Set ON</button>

            <input type="number" id="off${o.id}" min="0" max="23">
            <button onclick="setAutoOff(${o.id})">Set OFF</button>
        `;

        container.appendChild(div);
    });
}

function setOn(id) {
    api(`/api/output/${id}/on`).then(refresh);
}

function setOff(id) {
    api(`/api/output/${id}/off`).then(refresh);
}

function setManual(id, enabled) {
    const mode = enabled ? "on" : "off";
    api(`/api/output/${id}/manual/${mode}`).then(refresh);
}

function setAutoOn(id) {
    const hour = document.getElementById("on"+id).value;
    if (!hour) return;
    api(`/api/output/${id}/autoon?hour=${hour}`).then(refresh);
}

function setAutoOff(id) {
    const hour = document.getElementById("off"+id).value;
    if (!hour) return;
    api(`/api/output/${id}/autooff?hour=${hour}`).then(refresh);
}

function setGlobalAuto(enabled) {
    const mode = enabled ? "on" : "off";
    api(`/api/system/auto/${mode}`).then(refresh);
}

refresh();
setInterval(refresh, 5000);

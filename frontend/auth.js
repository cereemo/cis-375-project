const API_BASE = ""; //TODO

async function apiPost(path, body) {
  const res = await fetch(`${API_BASE}${path}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    credentials: "include",
    body: JSON.stringify(body),
  });

  const data = await res.json().catch(() => ({}));

  //success codes are 200 and 201
  if (res.status !== 200 && res.status !== 201) {
    throw {
      status: res.status,
      field: data.field || "client",
      message: data.error || "Client error",
      raw: data,
    };
  }

  return data;
}

function hideEl(el) { if (el) { el.style.display = "none"; el.textContent = ""; } }
function showEl(el, msg) { if (el) { el.style.display = "block"; el.textContent = msg; } }

function clearAuthErrors(map) {
  hideEl(map.topServerEl);
  hideEl(map.topClientEl);
  hideEl(map.emailEl);
  hideEl(map.passwordEl);
  hideEl(map.codeEl);
}

function showAuthError(map, field, message) {
  if (field === "server") return showEl(map.topServerEl, message);
  if (field === "client") return showEl(map.topClientEl, message);

  // form fields
  if (field === "email") return showEl(map.emailEl, message);
  if (field === "password") return showEl(map.passwordEl, message);
  if (field === "code") return showEl(map.codeEl, message);

  return showEl(map.topClientEl, message || "Client error");
}
// Storage key
const CART_KEY = "buythings_cart_v1";

function getCart() {
  try {
    const raw = localStorage.getItem(CART_KEY);
    const cart = raw ? JSON.parse(raw) : [];
    return Array.isArray(cart) ? cart : [];
  } catch {
    return [];
  }
}

function saveCart(cart) {
  localStorage.setItem(CART_KEY, JSON.stringify(cart));
}

function addToCart(productId, qty = 1) {
  const cart = getCart();
  const existing = cart.find((x) => x.id === productId);
  if (existing) existing.qty += qty;
  else cart.push({ id: productId, qty });
  saveCart(cart);
}

function removeFromCart(productId) {
  const cart = getCart().filter((x) => x.id !== productId);
  saveCart(cart);
}

function setQty(productId, qty) {
  const cart = getCart();
  const item = cart.find((x) => x.id === productId);
  if (!item) return;

  if (qty <= 0) {
    removeFromCart(productId);
    return;
  }

  item.qty = qty;
  saveCart(cart);
}

function getCartCount() {
  return getCart().reduce((sum, x) => sum + (x.qty || 0), 0);
}

function updateCartCountBadge() {
  const el = document.getElementById("cart-count");
  if (!el) return;
  const count = getCartCount();
  el.textContent = count > 0 ? String(count) : "";
}

function renderCartPage() {
  const container = document.getElementById("cart-items");
  const totalEl = document.getElementById("cart-total");
  const emptyEl = document.getElementById("cart-empty");

  if (!container) return;

  const cart = getCart();

  if (!Array.isArray(window.products)) {
    container.innerHTML = "<p>Products not loaded. Make sure products.js loads before cart.js.</p>";
    return;
  }

  const productMap = new Map(products.map((p) => [p.id, p]));

  if (cart.length === 0) {
    container.innerHTML = "";
    if (emptyEl) emptyEl.style.display = "block";
    if (totalEl) totalEl.textContent = "$0.00";
    return;
  } else {
    if (emptyEl) emptyEl.style.display = "none";
  }

  let total = 0;

  container.innerHTML = cart
    .map((item) => {
      const p = productMap.get(item.id);
      if (!p) return "";

      const line = p.price * item.qty;
      total += line;

      return `
        <article class="cart-item" data-id="${p.id}">
          <img class="cart-item-image" src="${p.image}" alt="${p.name}" />
          <div class="cart-item-info">
            <h3 class="cart-item-title">${p.name}</h3>
            <p class="cart-item-price">$${p.price.toFixed(2)}</p>

            <div class="cart-item-controls">
              <label>
                Qty:
                <input class="cart-qty" type="number" min="1" value="${item.qty}" />
              </label>
              <button class="btn-primary cart-remove" type="button">Remove</button>
            </div>
          </div>
        </article>
      `;
    })
    .join("");

  if (totalEl) totalEl.textContent = `$${total.toFixed(2)}`;

  container.querySelectorAll(".cart-item").forEach((row) => {
    const id = row.getAttribute("data-id");
    const qtyInput = row.querySelector(".cart-qty");
    const removeBtn = row.querySelector(".cart-remove");

    qtyInput.addEventListener("change", () => {
      const qty = parseInt(qtyInput.value, 10);
      setQty(id, Number.isFinite(qty) ? qty : 1);
      renderCartPage();
      updateCartCountBadge();
    });

    removeBtn.addEventListener("click", () => {
      removeFromCart(id);
      renderCartPage();
      updateCartCountBadge();
    });
  });
}

document.addEventListener("DOMContentLoaded", () => {
  updateCartCountBadge();
  renderCartPage();
});

function getQueryParam(name) {
  const params = new URLSearchParams(window.location.search);
  return (params.get(name) || "").trim();
}

function normalize(str) {
  return (str || "").toString().toLowerCase().trim();
}

function matchesQuery(product, q) {
  if (!q) return true;
  const haystack = [
    product.name,
    product.description,
    product.category,
    product.id,
  ]
    .map(normalize)
    .join(" ");

  return haystack.includes(q);
}

function setHeaderForSearch(qRaw, resultCount) {
  const titleEl = document.getElementById("products-title");
  const subtitleEl = document.getElementById("products-subtitle");

  // If the elements don't exist, don't crash — just skip.
  if (!titleEl || !subtitleEl) return;

  const q = normalize(qRaw);

  if (q) {
    titleEl.textContent = `Search results for “${qRaw}”`;

    if (resultCount === 0) {
      subtitleEl.textContent = "No matching products found.";
    } else {
      subtitleEl.textContent = `Showing ${resultCount} result${resultCount !== 1 ? "s" : ""}`;
    }
  } else {
    titleEl.textContent = "All Products";
    subtitleEl.textContent = "Browse all items";
  }
}

function renderProducts(list) {
  const grid = document.getElementById("products-grid");
  if (!grid) return;

  if (!list.length) {
    grid.innerHTML = "";
    return;
  }

  grid.innerHTML = list
    .map((p) => {
      return `
        <div class="product-card">
          <a href="product.html?id=${encodeURIComponent(p.id)}">
            <img src="${p.image}" alt="${p.name}" />
          </a>
          <h3>${p.name}</h3>
          <p>$${Number(p.price).toFixed(2)}</p>

          <button
            class="btn-primary add-to-cart-btn"
            data-product-id="${p.id}"
            type="button"
          >
            Add to Cart
          </button>
        </div>
      `;
    })
    .join("");

  grid.querySelectorAll(".add-to-cart-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      const id = btn.dataset.productId;
      if (!id) return;

      addToCart(id, 1);
      updateCartCountBadge();

      const old = btn.textContent;
      btn.textContent = "Added ✓";
      btn.disabled = true;
      setTimeout(() => {
        btn.textContent = old;
        btn.disabled = false;
      }, 800);
    });
  });
}

document.addEventListener("DOMContentLoaded", () => {
  if (!Array.isArray(window.products)) {
    console.error("products not loaded. Ensure products.js loads before products-list.js");
    return;
  }

  const qRaw = getQueryParam("q");
  const q = normalize(qRaw);

  const searchInput = document.querySelector('input[name="q"]');
  if (searchInput) searchInput.value = qRaw;

  const filtered = window.products.filter((p) => matchesQuery(p, q));

  setHeaderForSearch(qRaw, filtered.length);

  renderProducts(filtered);
});

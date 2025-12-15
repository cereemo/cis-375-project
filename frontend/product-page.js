function getQueryParam(name) {
  const params = new URLSearchParams(window.location.search);
  return params.get(name);
}

document.addEventListener("DOMContentLoaded", () => {
  const root = document.getElementById("product-root");

  const id = getQueryParam("id");
  const product = products.find((p) => p.id === id);

  if (!product) {
    root.innerHTML = "<p>Product not found.</p>";
    return;
  }

  root.innerHTML = `
    <div class="product-detail" style="display:flex; gap:2rem; align-items:flex-start;">
      <img src="${product.image}" alt="${product.name}" style="max-width:320px; width:100%; border-radius:12px;" />
      <div>
        <h1 style="margin-top:0;">${product.name}</h1>
        <p style="font-size:1.5rem; font-weight:700; margin:0.5rem 0;">
          $${product.price.toFixed(2)}
        </p>
        <p style="margin-bottom:1.5rem;">
          ${product.description}
        </p>

        <button id="add-to-cart-btn" class="btn-primary" style="padding: 0.9rem 2rem;">
          Add to Cart
        </button>
        <p id="added-msg" style="display:none; margin-top:0.75rem;">Added to cart ✅</p>
      </div>
    </div>
  `;

  const btn = document.getElementById("add-to-cart-btn");
  const msg = document.getElementById("added-msg");

  btn.addEventListener("click", () => {
    addToCart(product.id, 1);
    updateCartCountBadge();

    const oldText = btn.textContent;
    btn.textContent = "Added ✓"; //source for checkmark - https://www.namecheap.com/visual/font-generator/checkmarks/
    btn.disabled = true;

    setTimeout(() => {
        btn.textContent = oldText;
        btn.disabled = false;
    },800);
  });
});

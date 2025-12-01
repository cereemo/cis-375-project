//this file gets the listing info from products.js and loads it to product.html

function getQueryParam(name) {
    const params = new URLSearchParams(window.location.search);
    return params.get(name);
}

document.addEventListener("DOMContentLoaded", () => {
    const root = document.getElementById("product-root");

    const id = getQueryParam("id");
    const product = products.find(p => p.id === id);

    if (!product) {
        root.innerHTML = "<p>Product not found.</p>";
        return;
    }

    root.innerHTML = `
        <div class="product-detail-card" style="
            display: flex;
            gap: 2rem;
            background: var(--color-surface);
            box-shadow: var(--shadow-medium);
            border-radius: var(--radius-card);
            padding: var(--space-xl);
            border: 1px solid #f0dede;
        ">
            <img src="${product.image}" alt="${product.name}" class="product-detail-image" 
                style="width: 320px; border-radius: var(--radius-card); object-fit: cover;" />

            <div class="product-detail-info">
                <h1 style="margin-top:0;">${product.name}</h1>
                <p class="product-detail-price" style="font-size:1.5rem; font-weight:700; margin:0.5rem 0;">
                    $${product.price.toFixed(2)}
                </p>

                <p class="product-detail-description" style="margin-bottom:1.5rem;">
                    ${product.description}
                </p>

                <button class="btn-primary" style="padding: 0.9rem 2rem;">
                    Add to Cart
                </button>
            </div>
        </div>
    `;
});
// Renders the products from products.js into products.html

document.addEventListener("DOMContentLoaded", () => {
    const grid = document.getElementById("products-grid");

    if (!Array.isArray(products) || products.length === 0) {
        grid.innerHTML = "<p>No products available yet.</p>";
        return;
    }
//build the product page
    products.forEach((product) => {
        const card = document.createElement("article");
        card.className = "product-card";

        card.innerHTML = `
            <img 
                src="${product.image}" 
                alt="${product.name}" 
            />
            <h3>${product.name}</h3>
            <p>${product.description}</p>
            <p><strong>$${product.price.toFixed(2)}</strong></p>
            <button class="btn-primary" onclick="window.location.href='product.html?id=${encodeURIComponent(
                product.id
            )}'">
                View Details
            </button>
        `;

        grid.appendChild(card);
    });
});

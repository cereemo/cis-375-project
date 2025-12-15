document.addEventListener('DOMContentLoaded', async () => {
    const grid = document.getElementById('products-grid');
    if (!grid) return;
    
    const urlParams = new URLSearchParams(window.location.search);
    const searchQuery = urlParams.get('q');
    
    grid.innerHTML = '<p style="text-align: center; padding: 2rem;">Loading products...</p>';
    
    try {
        let products;
        
        if (searchQuery) {
            document.querySelector('.products-page h1').textContent = `Search results for "${searchQuery}"`;
            const response = await ProductsAPI.search(searchQuery);
            products = response.data || [];
        } else {
            const response = await ProductsAPI.getFeatured();
            products = response.products || [];
        }
        
        if (products.length === 0) {
            grid.innerHTML = '<p style="text-align: center; padding: 2rem; color: var(--color-muted);">No products found.</p>';
            return;
        }
        
        grid.innerHTML = '';
        
        products.forEach(product => {
            const card = document.createElement('article');
            card.className = 'product-card';
            
            const imgSrc = product.thumbnail 
                ? `${API_BASE}/uploads/${product.thumbnail}` 
                : 'img/stockphoto.webp';
            
            card.innerHTML = `
                <img 
                    src="${imgSrc}" 
                    alt="${product.name}"
                    onerror="this.src='img/stockphoto.webp'"
                />
                <h3>${product.name}</h3>
                <p>${product.snippet || product.description || ''}</p>
                <p><strong>$${product.price.toFixed(2)}</strong></p>
                <button class="btn-primary" onclick="window.location.href='product.html?id=${product.id}'">
                    View Details
                </button>
            `;
            
            grid.appendChild(card);
        });
    } catch (error) {
        console.error('Failed to load products:', error);
        grid.innerHTML = `
            <p style="text-align: center; padding: 2rem; color: var(--color-accent);">
                Failed to load products. Please try again later.
            </p>
        `;
    }
});

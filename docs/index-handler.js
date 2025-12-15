document.addEventListener('DOMContentLoaded', async () => {
    const featuredGrid = document.querySelector('.products-grid');
    
    if (featuredGrid) {
        try {
            const response = await ProductsAPI.getFeatured();
            const products = (response.products || []).slice(0, 4); // Show only 4 on home page
            
            featuredGrid.innerHTML = '';
            
            products.forEach(product => {
                const card = document.createElement('div');
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
                    <p>$${product.price.toFixed(2)}</p>
                    <button class="btn-primary" onclick="addToCartQuick(${product.id}, this)">
                        Add to Cart
                    </button>
                `;
                
                featuredGrid.appendChild(card);
            });
        } catch (error) {
            console.error('Failed to load featured products:', error);
            // Keep existing static products as fallback
        }
    }
});

// Quick add to cart from home page
async function addToCartQuick(productId, button) {
    if (!TokenManager.isLoggedIn()) {
        window.location.href = 'login.html?redirect=' + encodeURIComponent(window.location.href);
        return;
    }
    
    const originalText = button.textContent;
    button.disabled = true;
    button.textContent = 'Adding...';
    
    try {
        await CartAPI.addToCart(productId, 1);
        
        button.textContent = 'Added ✓';
        button.style.background = 'linear-gradient(135deg, #4caf50, #45a049)';
        
        // Update cart count in navbar
        updateCartCount();
        
        setTimeout(() => {
            button.disabled = false;
            button.textContent = originalText;
            button.style.background = '';
        }, 2000);
    } catch (error) {
        console.error('Failed to add to cart:', error);
        button.textContent = '✗ Failed';
        
        setTimeout(() => {
            button.disabled = false;
            button.textContent = originalText;
        }, 2000);
    }
}
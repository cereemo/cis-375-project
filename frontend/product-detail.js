function getQueryParam(name) {
    const params = new URLSearchParams(window.location.search);
    return params.get(name);
}

document.addEventListener('DOMContentLoaded', async () => {
    const root = document.getElementById('product-root');
    const productId = getQueryParam('id');
    
    if (!productId) {
        root.innerHTML = '<p>Product not found.</p>';
        return;
    }
    
    root.innerHTML = '<p style="text-align: center; padding: 2rem;">Loading product...</p>';
    
    try {
        const response = await ProductsAPI.getProduct(productId);
        const product = response.data;
        
        if (!product) {
            root.innerHTML = '<p>Product not found.</p>';
            return;
        }
        
        const images = product.images || [];
        const mainImage = images.length > 0 
            ? `${API_BASE}/uploads/${images[0]}` 
            : 'img/stockphoto.webp';
        
        let imageGallery = '';
        if (images.length > 1) {
            imageGallery = `
                <div style="display: flex; gap: 0.5rem; margin-top: 1rem; flex-wrap: wrap;">
                    ${images.map((img, idx) => `
                        <img 
                            src="${API_BASE}/uploads/${img}" 
                            alt="${product.name} ${idx + 1}"
                            style="width: 60px; height: 60px; object-fit: cover; border-radius: 0.5rem; cursor: pointer; border: 2px solid transparent;"
                            onmouseover="document.getElementById('main-product-image').src=this.src"
                            onerror="this.style.display='none'"
                        />
                    `).join('')}
                </div>
            `;
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
                <div>
                    <img 
                        id="main-product-image"
                        src="${mainImage}" 
                        alt="${product.name}" 
                        class="product-detail-image" 
                        style="width: 320px; border-radius: var(--radius-card); object-fit: cover;"
                        onerror="this.src='img/stockphoto.webp'"
                    />
                    ${imageGallery}
                </div>

                <div class="product-detail-info" style="flex: 1;">
                    <h1 style="margin-top:0;">${product.name}</h1>
                    <p class="product-detail-price" style="font-size:1.5rem; font-weight:700; margin:0.5rem 0;">
                        $${product.price.toFixed(2)}
                    </p>

                    <p class="product-detail-description" style="margin-bottom:1.5rem;">
                        ${product.description}
                    </p>

                    <div style="display: flex; gap: 1rem; align-items: center;">
                        <input 
                            type="number" 
                            id="quantity-input" 
                            min="1" 
                            value="1" 
                            style="width: 80px; padding: 0.75rem; border: 1px solid #f0dede; border-radius: 0.5rem; font-size: 1rem;"
                        />
                        <button 
                            id="add-to-cart-btn" 
                            class="btn-primary" 
                            style="padding: 0.9rem 2rem;"
                        >
                            Add to Cart
                        </button>
                    </div>
                    
                    <div id="cart-message" style="margin-top: 1rem;"></div>
                </div>
            </div>
        `;
        
        const addToCartBtn = document.getElementById('add-to-cart-btn');
        const quantityInput = document.getElementById('quantity-input');
        const cartMessage = document.getElementById('cart-message');
        
        addToCartBtn.addEventListener('click', async () => {
            if (!TokenManager.isLoggedIn()) {
                window.location.href = 'login.html?redirect=' + encodeURIComponent(window.location.href);
                return;
            }
            
            const quantity = parseInt(quantityInput.value) || 1;
            
            addToCartBtn.disabled = true;
            addToCartBtn.textContent = 'Adding...';
            
            try {
                await CartAPI.addToCart(productId, quantity);
                
                cartMessage.innerHTML = `
                    <div style="color: #4caf50; padding: 0.75rem; background: rgba(76,175,80,0.1); border-radius: 0.5rem;">
                        ✓ Added to cart!
                    </div>
                `;
                
                updateCartCount();
                
                setTimeout(() => {
                    cartMessage.innerHTML = '';
                }, 3000);
            } catch (error) {
                cartMessage.innerHTML = `
                    <div style="color: var(--color-accent); padding: 0.75rem; background: rgba(184,59,94,0.1); border-radius: 0.5rem;">
                        Failed to add to cart: ${error.message}
                    </div>
                `;
            } finally {
                addToCartBtn.disabled = false;
                addToCartBtn.textContent = 'Add to Cart';
            }
        });
        
    } catch (error) {
        console.error('Failed to load product:', error);
        root.innerHTML = `
            <p style="color: var(--color-accent);">
                Failed to load product. Please try again later.
            </p>
        `;
    }
});
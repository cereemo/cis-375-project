document.addEventListener('DOMContentLoaded', async () => {
    if (!TokenManager.isLoggedIn()) {
        window.location.href = 'login.html?redirect=cart.html';
        return;
    }
    
    const cartLayout = document.querySelector('.cart-layout');
    const checkoutBtn = document.querySelector('.cart-checkout-btn');
    
    cartLayout.innerHTML = '<p style="text-align: center; padding: 2rem;">Loading cart...</p>';
    
    try {
        const data = await CartAPI.getCart();
        const items = data.items || [];
        const total = data.cart_total || 0;
        
        if (items.length === 0) {
            cartLayout.innerHTML = `
                <div style="text-align: center; padding: 3rem;">
                    <h2 style="color: var(--color-muted);">Your cart is empty</h2>
                    <p style="margin: 1rem 0;">Start Shopping!</p>
                    <a href="products.html">
                        <button class="btn-primary" style="margin-top: 1rem;">
                            Browse Products
                        </button>
                    </a>
                </div>
            `;
            checkoutBtn.style.display = 'none';
            return;
        }
        
        cartLayout.innerHTML = '';
        
        items.forEach(item => {
            const cartItem = document.createElement('article');
            cartItem.className = 'cart-item';
            cartItem.dataset.productId = item.id;
            
            const imgSrc = item.thumbnail 
                ? `${API_BASE}/uploads/${item.thumbnail}` 
                : 'img/stockphoto.webp';
            
            cartItem.innerHTML = `
                <img
                    src="${imgSrc}"
                    alt="${item.name}"
                    class="cart-item-image"
                    onerror="this.src='img/stockphoto.webp'"
                />

                <div class="cart-item-info">
                    <h2 class="cart-item-title">${item.name}</h2>
                    <p class="cart-item-meta">$${item.price.toFixed(2)} each</p>

                    <div class="cart-item-bottom-row">
                        <div class="cart-qty">
                            <label>Qty</label>
                            <input 
                                type="number" 
                                min="1" 
                                value="${item.quantity}" 
                                data-product-id="${item.id}"
                                class="quantity-input"
                            />
                        </div>

                        <span class="cart-item-price">$${item.total.toFixed(2)}</span>

                        <button 
                            class="cart-remove" 
                            type="button"
                            data-product-id="${item.id}"
                        >
                            Remove
                        </button>
                    </div>
                </div>
            `;
            
            cartLayout.appendChild(cartItem);
        });
        
        checkoutBtn.innerHTML = `
            Proceed to Checkout <span style="margin-left: 0.5rem;">($${total.toFixed(2)})</span>
        `;
        
        document.querySelectorAll('.cart-remove').forEach(btn => {
            btn.addEventListener('click', async (e) => {
                const productId = e.target.dataset.productId;
                await removeCartItem(productId);
            });
        });
        
        document.querySelectorAll('.quantity-input').forEach(input => {
            let debounceTimer;
            input.addEventListener('change', async (e) => {
                clearTimeout(debounceTimer);
                const productId = e.target.dataset.productId;
                const newQuantity = parseInt(e.target.value);
                const oldQuantity = items.find(i => i.id == productId)?.quantity || 1;
                
                if (newQuantity < 1) {
                    e.target.value = 1;
                    return;
                }
                
                debounceTimer = setTimeout(async () => {
                    await updateQuantity(productId, newQuantity - oldQuantity);
                }, 500);
            });
        });
        
    } catch (error) {
        console.error('Failed to load cart:', error);
        cartLayout.innerHTML = `
            <p style="color: var(--color-accent); text-align: center; padding: 2rem;">
                Failed to load cart. Please try again later.
            </p>
        `;
    }
});

async function removeCartItem(productId) {
    try {
        await CartAPI.removeFromCart(productId);
        
        const cartItem = document.querySelector(`.cart-item[data-product-id="${productId}"]`);
        if (cartItem) {
            cartItem.style.opacity = '0';
            cartItem.style.transition = 'opacity 0.3s';
            setTimeout(() => {
                cartItem.remove();
                
                if (document.querySelectorAll('.cart-item').length === 0) {
                    location.reload();
                } else {
                    updateCartTotal();
                }
            }, 300);
        }
        
        updateCartCount();
    } catch (error) {
        console.error('Failed to remove item:', error);
        alert('Failed to remove item from cart');
    }
}

async function updateQuantity(productId, quantityChange) {
    try {
        await CartAPI.addToCart(productId, quantityChange);
        location.reload();
    } catch (error) {
        console.error('Failed to update quantity:', error);
        alert('Failed to update quantity');
    }
}

async function updateCartTotal() {
    try {
        const data = await CartAPI.getCart();
        const total = data.cart_total || 0;
        
        document.querySelector('.cart-checkout-btn').innerHTML = `
            Proceed to Checkout <span style="margin-left: 0.5rem;">($${total.toFixed(2)})</span>
        `;
    } catch (error) {
        console.error('Failed to update total:', error);
    }
}
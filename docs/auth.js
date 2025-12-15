const API_BASE = 'https://unbuffeted-ranunculaceous-hailey.ngrok-free.dev'; // change

// Token management
const TokenManager = {
    setTokens(accessToken, refreshToken) {
        localStorage.setItem('access_token', accessToken);
        localStorage.setItem('refresh_token', refreshToken);
    },
    
    getAccessToken() {
        return localStorage.getItem('access_token');
    },
    
    getRefreshToken() {
        return localStorage.getItem('refresh_token');
    },
    
    clearTokens() {
        localStorage.removeItem('access_token');
        localStorage.removeItem('refresh_token');
    },
    
    isLoggedIn() {
        return !!this.getAccessToken();
    }
};

// API Client with automatic token refresh
const ApiClient = {
    async request(endpoint, options = {}) {
        const url = `${API_BASE}${endpoint}`;
        const headers = {
            'Content-Type': 'application/json',
            ...options.headers
        };
        
        const token = TokenManager.getAccessToken();
        if (token) {
            headers['Authorization'] = `Bearer ${token}`;
        }
        
        try {
            let response = await fetch(url, {
                ...options,
                headers
            });
            
            // If unauthorized, try to refresh token
            if (response.status === 401 && TokenManager.getRefreshToken()) {
                const refreshed = await this.refreshToken();
                if (refreshed) {
                    // Retry original request with new token
                    headers['Authorization'] = `Bearer ${TokenManager.getAccessToken()}`;
                    response = await fetch(url, {
                        ...options,
                        headers
                    });
                } else {
                    TokenManager.clearTokens();
                    window.location.href = 'login.html';
                    return null;
                }
            }
            
            const data = await response.json();
            
            if (!response.ok) {
                throw new Error(data.error || 'Request failed');
            }
            
            return data;
        } catch (error) {
            console.error('API Error:', error);
            throw error;
        }
    },
    
    async refreshToken() {
        try {
            const response = await fetch(`${API_BASE}/api/refresh`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    refresh_token: TokenManager.getRefreshToken()
                })
            });
            
            if (response.ok) {
                const data = await response.json();
                TokenManager.setTokens(data.access_token, data.refresh_token);
                return true;
            }
            return false;
        } catch (error) {
            console.error('Token refresh failed:', error);
            return false;
        }
    }
};

// Auth API methods
const AuthAPI = {
    async requestCreationCode(email) {
        return await ApiClient.request('/api/account_creation_code', {
            method: 'POST',
            body: JSON.stringify({ email })
        });
    },
    
    async createAccount(email, password, code, token) {
        const response = await ApiClient.request('/api/create_account', {
            method: 'POST',
            body: JSON.stringify({ email, password, code, token })
        });
        
        if (response.access_token && response.refresh_token) {
            TokenManager.setTokens(response.access_token, response.refresh_token);
        }
        
        return response;
    },
    
    async login(email, password) {
        const response = await ApiClient.request('/api/login', {
            method: 'POST',
            body: JSON.stringify({ email, password })
        });
        
        if (response.access_token && response.refresh_token) {
            TokenManager.setTokens(response.access_token, response.refresh_token);
        }
        
        return response;
    },
    
    async logoutAll() {
        await ApiClient.request('/api/logoutall', {
            method: 'POST'
        });
        TokenManager.clearTokens();
    },
    
    logout() {
        TokenManager.clearTokens();
        window.location.href = 'login.html';
    }
};

// Products API - REQUIRED by products-api.js, product-detail.js, and index-handler.js
const ProductsAPI = {
    async getFeatured() {
        return await ApiClient.request('/api/products/featured');
    },
    
    async getProduct(id) {
        return await ApiClient.request(`/api/products/${id}`);
    },
    
    async search(query) {
        return await ApiClient.request(`/api/products/search?q=${encodeURIComponent(query)}`);
    }
};

// Cart API - REQUIRED by cart-handler.js, product-detail.js, and index-handler.js
const CartAPI = {
    async addToCart(productId, quantity = 1) {
        return await ApiClient.request('/api/cart', {
            method: 'POST',
            body: JSON.stringify({ 
                product_id: productId, 
                quantity: quantity 
            })
        });
    },
    
    async getCart() {
        return await ApiClient.request('/api/cart');
    },
    
    async removeFromCart(productId) {
        return await ApiClient.request(`/api/cart/${productId}`, {
            method: 'DELETE'
        });
    }
};

// Update cart count in navbar
async function updateCartCount() {
    if (!TokenManager.isLoggedIn()) {
        document.querySelectorAll('[href="cart.html"]').forEach(el => {
            el.textContent = 'Cart (0)';
        });
        return;
    }
    
    try {
        const data = await ApiClient.request('/api/cart');
        const itemCount = data.items ? data.items.length : 0;
        document.querySelectorAll('[href="cart.html"]').forEach(el => {
            el.textContent = `Cart (${itemCount})`;
        });
    } catch (error) {
        console.error('Failed to update cart count:', error);
    }
}

// Initialize auth state on page load
document.addEventListener('DOMContentLoaded', () => {
    // Update navbar based on auth state
    const accountLinks = document.querySelectorAll('[href="login.html"]');
    
    if (TokenManager.isLoggedIn()) {
        accountLinks.forEach(link => {
            link.textContent = 'Logout';
            link.href = '#';
            link.addEventListener('click', (e) => {
                e.preventDefault();
                AuthAPI.logout();
            });
        });
        
        updateCartCount();
    }
});
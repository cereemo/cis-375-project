document.addEventListener('DOMContentLoaded', () => {
    // Redirect if already logged in
    if (TokenManager.isLoggedIn()) {
        window.location.href = 'index.html';
        return;
    }
    
    const form = document.querySelector('.auth-form');
    const emailInput = form.querySelector('input[name="email"]');
    const passwordInput = form.querySelector('input[name="password"]');
    const submitButton = form.querySelector('button[type="submit"]');
    
    // Toggle password visibility
    const eyeButtons = form.querySelectorAll('.field-icon-btn');
    eyeButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const input = btn.closest('.field-input-wrapper').querySelector('input');
            input.type = input.type === 'password' ? 'text' : 'password';
        });
    });
    
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        const email = emailInput.value.trim().toLowerCase();
        const password = passwordInput.value;
        
        // Clear previous errors
        document.querySelectorAll('.error-message').forEach(el => el.remove());
        
        // Disable submit button
        submitButton.disabled = true;
        submitButton.textContent = 'Logging in...';
        
        try {
            await AuthAPI.login(email, password);
            
            // Success - redirect to home
            window.location.href = 'index.html';
        } catch (error) {
            // Show error message
            const errorDiv = document.createElement('div');
            errorDiv.className = 'error-message';
            errorDiv.style.cssText = 'color: var(--color-accent); margin: 1rem 0; padding: 0.75rem; background: rgba(184,59,94,0.1); border-radius: 0.5rem; font-size: 0.9rem;';
            errorDiv.textContent = error.message || 'Login failed. Please check your credentials.';
            
            form.insertBefore(errorDiv, submitButton);
            
            // Re-enable submit button
            submitButton.disabled = false;
            submitButton.textContent = 'Log in';
        }
    });
});
document.addEventListener('DOMContentLoaded', () => {
    if (TokenManager.isLoggedIn()) {
        window.location.href = 'index.html';
        return;
    }
    
    const form = document.querySelector('.auth-form');
    const emailInput = form.querySelector('input[name="email"]');
    const passwordInput = form.querySelector('input[name="password"]');
    const confirmPasswordInput = form.querySelector('input[name="confirm_password"]');
    const codeInput = form.querySelector('input[name="code"]');
    const sendCodeBtn = form.querySelector('.code-btn');
    const submitButton = form.querySelector('button[type="submit"]');
    
    let verificationToken = null;
    let codeSent = false;
    
    const eyeButtons = form.querySelectorAll('.field-icon-btn');
    eyeButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const input = btn.closest('.field-input-wrapper').querySelector('input');
            input.type = input.type === 'password' ? 'text' : 'password';
        });
    });
    
    sendCodeBtn.addEventListener('click', async () => {
        const email = emailInput.value.trim().toLowerCase();
        
        if (!email) {
            showError('Please enter an email address', emailInput);
            return;
        }
        
        sendCodeBtn.disabled = true;
        sendCodeBtn.textContent = 'Sending...';
        
        try {
            const response = await AuthAPI.requestCreationCode(email);
            verificationToken = response.token;
            codeSent = true;
            
            sendCodeBtn.textContent = 'Code sent!';
            sendCodeBtn.style.background = 'linear-gradient(90deg, rgba(76,175,80,0.2), rgba(76,175,80,0.1))';
            
            showSuccess('Verification code sent to your email!', sendCodeBtn);
            
            setTimeout(() => {
                sendCodeBtn.disabled = false;
                sendCodeBtn.textContent = 'Resend code';
                sendCodeBtn.style.background = '';
            }, 30000);
        } catch (error) {
            sendCodeBtn.disabled = false;
            sendCodeBtn.textContent = 'Send code';
            showError(error.message || 'Failed to send code', emailInput);
        }
    });
    
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        document.querySelectorAll('.error-message, .success-message').forEach(el => el.remove());
        
        const email = emailInput.value.trim().toLowerCase();
        const password = passwordInput.value;
        const confirmPassword = confirmPasswordInput.value;
        const code = codeInput.value.trim();
        
        if (!codeSent || !verificationToken) {
            showError('Please request a verification code first', codeInput);
            return;
        }
        
        if (password !== confirmPassword) {
            showError('Passwords do not match', confirmPasswordInput);
            return;
        }
        
        if (password.length < 8) {
            showError('Password must be at least 8 characters', passwordInput);
            return;
        }
        
        if (!code) {
            showError('Please enter the verification code', codeInput);
            return;
        }
        
        submitButton.disabled = true;
        submitButton.textContent = 'Creating account...';
        
        try {
            await AuthAPI.createAccount(email, password, code, verificationToken);
            window.location.href = 'index.html';
        } catch (error) {
            submitButton.disabled = false;
            submitButton.textContent = 'Sign up';
            showError(error.message || 'Account creation failed', submitButton);
        }
    });
    
    function showError(message, element) {
        const errorDiv = document.createElement('div');
        errorDiv.className = 'error-message';
        errorDiv.style.cssText = 'color: var(--color-accent); margin: 0.5rem 0; padding: 0.75rem; background: rgba(184,59,94,0.1); border-radius: 0.5rem; font-size: 0.9rem;';
        errorDiv.textContent = message;
        element.closest('.field, .auth-form').appendChild(errorDiv);
    }
    
    function showSuccess(message, element) {
        const successDiv = document.createElement('div');
        successDiv.className = 'success-message';
        successDiv.style.cssText = 'color: #4caf50; margin: 0.5rem 0; padding: 0.75rem; background: rgba(76,175,80,0.1); border-radius: 0.5rem; font-size: 0.9rem;';
        successDiv.textContent = message;
        element.closest('.field, .auth-form').appendChild(successDiv);
    }
});

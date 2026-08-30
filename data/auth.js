// Apply saved theme
(function initTheme() {
    const saved = localStorage.getItem('sgx_theme') || 'dark';
    document.documentElement.setAttribute('data-theme', saved);
})();

// ---- Login Page Logic ----
const loginForm = document.getElementById('loginForm');
if (loginForm) {
    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const user    = document.getElementById('username').value;
        const pass    = document.getElementById('password').value;
        const carWifi = document.getElementById('carWifi').value;
        const msg     = document.getElementById('loginMessage');
        const btn     = document.getElementById('loginBtn');

        btn.disabled    = true;
        btn.textContent = '⏳ AUTHENTICATING...';
        msg.className   = 'form-message';
        msg.textContent = '';

        try {
            const response = await fetch('http://localhost:8000/api/login', {
                method:  'POST',
                headers: { 'Content-Type': 'application/json' },
                body:    JSON.stringify({ username: user, password: pass, car_wifi: carWifi })
            });

            if (response.ok) {
                // Store username for profile page
                localStorage.setItem('sgx_username', user);
                msg.className   = 'form-message success';
                msg.textContent = `✅ Welcome, ${user}! Redirecting…`;
                setTimeout(() => { window.location.href = 'index.html'; }, 900);
            } else {
                const err = await response.json();
                msg.className   = 'form-message error';
                msg.textContent = err.detail || 'ACCESS DENIED. Invalid Credentials.';
                btn.disabled    = false;
                btn.innerHTML   = '<span class="btn-icon">⚡</span> AUTHENTICATE';
            }
        } catch (error) {
            console.error('Auth check failed:', error);
            msg.className   = 'form-message error';
            msg.textContent = 'ERROR: Cannot connect to Backend Server. Is it running?';
            btn.disabled    = false;
            btn.innerHTML   = '<span class="btn-icon">⚡</span> AUTHENTICATE';
        }
    });
}

// ---- Register Page Logic ----
const registerForm = document.getElementById('registerForm');
if (registerForm) {
    registerForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const user = document.getElementById('regUsername').value;
        const pass = document.getElementById('regPassword').value;
        const msg  = document.getElementById('registerMessage');
        const btn  = document.getElementById('registerBtn');

        btn.disabled    = true;
        btn.textContent = '⏳ CREATING...';
        msg.className   = 'form-message';
        msg.textContent = 'Creating account…';

        try {
            const response = await fetch('http://localhost:8000/api/register', {
                method:  'POST',
                headers: { 'Content-Type': 'application/json' },
                body:    JSON.stringify({ username: user, password: pass })
            });

            if (response.ok) {
                msg.className   = 'form-message success';
                msg.textContent = `✅ Account '${user}' created! Redirecting to login…`;
                setTimeout(() => {
                    window.location.href = `login.html?user=${encodeURIComponent(user)}`;
                }, 1800);
            } else {
                const err = await response.json();
                msg.className   = 'form-message error';
                msg.textContent = err.detail || 'Registration failed.';
                btn.disabled    = false;
                btn.innerHTML   = '<span class="btn-icon">✚</span> CREATE ACCOUNT';
            }
        } catch (error) {
            console.error('Register failed:', error);
            msg.className   = 'form-message error';
            msg.textContent = 'ERROR: Cannot connect to Backend Server. Is it running?';
            btn.disabled    = false;
            btn.innerHTML   = '<span class="btn-icon">✚</span> CREATE ACCOUNT';
        }
    });
}

// ---- Auto-fill username when redirected from register ----
const urlParams = new URLSearchParams(window.location.search);
const preUser   = urlParams.get('user');
if (preUser) {
    const usernameInput = document.getElementById('username');
    if (usernameInput) usernameInput.value = decodeURIComponent(preUser);
}
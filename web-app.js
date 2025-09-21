// AK Web Interface - Backend API Version
class AKWebApp {
    constructor() {
        this.currentProfile = 'default';
        this.profiles = new Map();
        this.keys = new Map();
        this.services = new Map();
        this.keysVisible = false;
        this.apiBaseUrl = 'http://127.0.0.1:8765/api';
        
        this.init();
    }

    async init() {
        try {
            await this.loadServices();
            await this.loadProfiles();
            await this.loadKeys();
            this.bindEvents();
            this.updateGlobalProfileSelector();
            this.loadData();
        } catch (error) {
            console.error('Failed to initialize:', error);
            this.showToast('Failed to connect to AK backend. Make sure the API server is running.', 'error');
        }
    }

    async loadServices() {
        try {
            const response = await fetch(`${this.apiBaseUrl}/services`);
            const data = await response.json();
            
            if (response.ok && Array.isArray(data)) {
                this.services.clear();
                data.forEach(service => {
                    this.services.set(service.name, service);
                });
            } else {
                throw new Error(data.error || 'Failed to load services');
            }
        } catch (error) {
            console.error('Failed to load services:', error);
            this.showToast('Failed to load services: ' + error.message, 'error');
        }
    }

    async loadProfiles() {
        try {
            const response = await fetch(`${this.apiBaseUrl}/profiles`);
            const data = await response.json();
            
            if (response.ok && Array.isArray(data)) {
                this.profiles.clear();
                data.forEach(profile => {
                    this.profiles.set(profile.name, {
                        name: profile.name,
                        keys: new Map(),
                        keyCount: profile.keyCount,
                        isDefault: profile.name === 'default' // Simple check for default profile
                    });
                });
                
                // Set current profile to default if it exists
                if (this.profiles.has('default')) {
                    this.currentProfile = 'default';
                }
            } else {
                throw new Error(data.error || 'Failed to load profiles');
            }
        } catch (error) {
            console.error('Failed to load profiles:', error);
            this.showToast('Failed to load profiles: ' + error.message, 'error');
        }
    }

    async loadKeys() {
        if (!this.currentProfile) return;
        
        try {
            const response = await fetch(`${this.apiBaseUrl}/profiles/${this.currentProfile}/keys`);
            const data = await response.json();
            
            if (response.ok && data.keys && Array.isArray(data.keys)) {
                const profile = this.profiles.get(this.currentProfile);
                if (profile) {
                    profile.keys.clear();
                    data.keys.forEach(keyData => {
                        // Detect service from key name if not provided
                        const detectedService = this.detectServiceFromKeyName(keyData.name);
                        profile.keys.set(keyData.name, {
                            value: keyData.value,
                            service: detectedService,
                            tested: keyData.tested || false
                        });
                    });
                }
                this.renderKeys();
            } else {
                throw new Error(data.error || 'Failed to load profile keys');
            }
        } catch (error) {
            console.error('Failed to load profile keys:', error);
            this.showToast('Failed to load keys: ' + error.message, 'error');
        }
    }

    bindEvents() {
        // Tab switching
        document.querySelectorAll('.tab-button').forEach(button => {
            button.addEventListener('click', (e) => this.switchTab(e.target.dataset.tab));
        });

        // Global profile selector
        document.getElementById('global-profile-selector').addEventListener('change', (e) => {
            this.setCurrentProfile(e.target.value);
        });

        // Key management events
        document.getElementById('add-key-btn').addEventListener('click', () => this.showAddKeyModal());
        document.getElementById('key-search').addEventListener('input', (e) => this.filterKeys(e.target.value));
        document.getElementById('toggle-visibility').addEventListener('click', () => this.toggleKeysVisibility());

        // Profile management events
        document.getElementById('create-profile-btn').addEventListener('click', () => this.showCreateProfileModal());

        // Service management events
        document.getElementById('add-service-btn').addEventListener('click', () => this.showAddServiceModal());

        // Refresh button
        document.getElementById('refresh-all').addEventListener('click', () => this.refreshAll());
    }

    loadData() {
        this.renderKeys();
        this.renderProfiles();
        this.renderServices();
        this.updateGlobalProfileSelector();
    }

    switchTab(tabName) {
        // Update tab buttons
        document.querySelectorAll('.tab-button').forEach(button => {
            button.classList.remove('active', 'border-primary-500', 'text-primary-600');
            button.classList.add('text-slate-500', 'hover:text-slate-700', 'border-transparent', 'hover:border-slate-300');
        });
        
        document.querySelector(`[data-tab="${tabName}"]`).classList.add('active', 'border-primary-500', 'text-primary-600');
        document.querySelector(`[data-tab="${tabName}"]`).classList.remove('text-slate-500', 'hover:text-slate-700', 'border-transparent', 'hover:border-slate-300');

        // Update tab content
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.remove('active');
        });
        document.getElementById(`${tabName}-tab`).classList.add('active');

        // Load data for the active tab
        switch(tabName) {
            case 'keys':
                this.loadKeys();
                break;
            case 'profiles':
                this.renderProfiles();
                break;
            case 'services':
                this.renderServices();
                break;
        }
    }

    async setCurrentProfile(profileName) {
        this.currentProfile = profileName;
        await this.loadKeys();
        this.updateGlobalProfileSelector();
        this.showToast(`Switched to profile: ${profileName}`, 'info');
    }

    updateGlobalProfileSelector() {
        const selector = document.getElementById('global-profile-selector');
        selector.innerHTML = '';
        
        this.profiles.forEach(profile => {
            const option = document.createElement('option');
            option.value = profile.name;
            option.textContent = profile.name + (profile.isDefault ? ' (default)' : '');
            option.selected = profile.name === this.currentProfile;
            selector.appendChild(option);
        });
    }

    renderKeys() {
        const keysList = document.getElementById('keys-list');
        const currentProfile = this.profiles.get(this.currentProfile);
        
        if (!currentProfile) {
            keysList.innerHTML = '<div class="text-center py-8 text-slate-500">No profile selected</div>';
            return;
        }

        if (currentProfile.keys.size === 0) {
            keysList.innerHTML = '<div class="text-center py-8 text-slate-500">No API keys found in this profile</div>';
            return;
        }

        let html = '';
        currentProfile.keys.forEach((keyData, keyName) => {
            const maskedValue = this.keysVisible ? keyData.value : this.maskValue(keyData.value);
            const serviceInfo = this.services.get(keyData.service) || { name: keyData.service };
            
            html += `
                <div class="bg-white rounded-lg border border-slate-200 p-4 hover:shadow-md transition-shadow">
                    <div class="flex items-center justify-between mb-2">
                        <div class="flex items-center space-x-3">
                            <span class="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium bg-primary-100 text-primary-800">
                                ${serviceInfo.name}
                            </span>
                            <h3 class="text-lg font-semibold text-slate-900">${keyName}</h3>
                        </div>
                        <div class="flex items-center space-x-2">
                            <button onclick="app.copyToClipboard('${keyName}', '${keyData.value}')" 
                                    class="text-slate-400 hover:text-slate-600 transition-colors" title="Copy to clipboard">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z"></path>
                                </svg>
                            </button>
                            <button onclick="app.editKey('${keyName}')" 
                                    class="text-slate-400 hover:text-blue-600 transition-colors" title="Edit key">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z"></path>
                                </svg>
                            </button>
                            <button onclick="app.deleteKey('${keyName}')" 
                                    class="text-slate-400 hover:text-red-600 transition-colors" title="Delete key">
                                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path>
                                </svg>
                            </button>
                        </div>
                    </div>
                    <div class="bg-slate-50 rounded-md p-3 font-mono text-sm text-slate-700">
                        ${maskedValue}
                    </div>
                    <div class="flex items-center justify-between mt-3 text-sm text-slate-500">
                        <span>Service: ${serviceInfo.name}</span>
                        <span class="flex items-center space-x-2">
                            <span class="w-2 h-2 rounded-full ${keyData.tested ? 'bg-green-500' : 'bg-gray-400'}"></span>
                            <span>${keyData.tested ? 'Tested' : 'Not tested'}</span>
                            <button onclick="app.testKey('${keyName}')" 
                                    class="ml-2 text-primary-600 hover:text-primary-800 font-medium">
                                Test
                            </button>
                        </span>
                    </div>
                </div>
            `;
        });

        keysList.innerHTML = html;
    }

    renderProfiles() {
        const profilesList = document.getElementById('profiles-list');
        let html = '';

        this.profiles.forEach(profile => {
            html += `
                <div class="bg-white rounded-lg border border-slate-200 p-6 hover:shadow-md transition-shadow">
                    <div class="flex items-center justify-between mb-4">
                        <div class="flex items-center space-x-3">
                            <h3 class="text-xl font-semibold text-slate-900">${profile.name}</h3>
                            ${profile.isDefault ? '<span class="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium bg-green-100 text-green-800">Default</span>' : ''}
                        </div>
                        <div class="flex items-center space-x-2">
                            <button onclick="app.setCurrentProfile('${profile.name}')" 
                                    class="px-3 py-1 text-sm font-medium text-primary-600 hover:text-primary-800 border border-primary-600 hover:border-primary-800 rounded-md transition-colors">
                                Select
                            </button>
                            ${profile.name !== 'default' ? `
                                <button onclick="app.deleteProfile('${profile.name}')" 
                                        class="px-3 py-1 text-sm font-medium text-red-600 hover:text-red-800 border border-red-600 hover:border-red-800 rounded-md transition-colors">
                                    Delete
                                </button>
                            ` : ''}
                        </div>
                    </div>
                    <div class="text-slate-600">
                        <p><strong>Keys:</strong> ${profile.keyCount || 0}</p>
                        <p><strong>Status:</strong> ${profile.name === this.currentProfile ? 'Active' : 'Inactive'}</p>
                    </div>
                </div>
            `;
        });

        profilesList.innerHTML = html;
    }

    renderServices() {
        const servicesList = document.getElementById('services-list');
        let html = '';

        this.services.forEach(service => {
            html += `
                <div class="bg-white rounded-lg border border-slate-200 p-6 hover:shadow-md transition-shadow">
                    <div class="flex items-center justify-between mb-4">
                        <div class="flex items-center space-x-3">
                            <h3 class="text-xl font-semibold text-slate-900">${service.name}</h3>
                            <span class="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium ${service.isBuiltIn ? 'bg-blue-100 text-blue-800' : 'bg-gray-100 text-gray-800'}">
                                ${service.isBuiltIn ? 'Built-in' : 'Custom'}
                            </span>
                        </div>
                        ${!service.isBuiltIn ? `
                            <div class="flex items-center space-x-2">
                                <button onclick="app.editService('${service.name}')" 
                                        class="px-3 py-1 text-sm font-medium text-blue-600 hover:text-blue-800 border border-blue-600 hover:border-blue-800 rounded-md transition-colors">
                                    Edit
                                </button>
                                <button onclick="app.deleteService('${service.name}')" 
                                        class="px-3 py-1 text-sm font-medium text-red-600 hover:text-red-800 border border-red-600 hover:border-red-800 rounded-md transition-colors">
                                    Delete
                                </button>
                            </div>
                        ` : ''}
                    </div>
                    <div class="text-slate-600 space-y-2">
                        <p><strong>API URL:</strong> ${service.apiUrl || 'Not specified'}</p>
                        <p><strong>Key Pattern:</strong> <code class="bg-slate-100 px-2 py-1 rounded text-sm">${service.keyPattern || 'Not specified'}</code></p>
                    </div>
                </div>
            `;
        });

        servicesList.innerHTML = html;
    }

    maskValue(value) {
        if (!value) return '(empty)';
        if (value.length <= 12) {
            return '*'.repeat(value.length);
        }
        return value.substring(0, 8) + '***' + value.substring(value.length - 4);
    }

    toggleKeysVisibility() {
        this.keysVisible = !this.keysVisible;
        const button = document.getElementById('toggle-visibility');
        button.innerHTML = this.keysVisible ? 
            '<svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13.875 18.825A10.05 10.05 0 0112 19c-4.478 0-8.268-2.943-9.543-7a9.97 9.97 0 011.563-3.029m5.858.908a3 3 0 114.243 4.243M9.878 9.878l4.242 4.242M9.878 9.878L8.464 8.464M9.878 9.878l4.242 4.242m-4.242-4.242L8.464 8.464m5.756 5.756L12.5 12.5m1.72 1.72L8.464 8.464"></path></svg> Hide Values' :
            '<svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"></path><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z"></path></svg> Show Values';
        this.renderKeys();
    }

    filterKeys(searchTerm) {
        const keyCards = document.querySelectorAll('#keys-list > div');
        keyCards.forEach(card => {
            const keyName = card.querySelector('h3').textContent.toLowerCase();
            const serviceName = card.querySelector('.bg-primary-100').textContent.toLowerCase();
            
            if (keyName.includes(searchTerm.toLowerCase()) || serviceName.includes(searchTerm.toLowerCase())) {
                card.style.display = 'block';
            } else {
                card.style.display = 'none';
            }
        });
    }

    async copyToClipboard(keyName, value) {
        try {
            await navigator.clipboard.writeText(value);
            this.showToast(`Copied ${keyName} to clipboard`, 'success');
        } catch (err) {
            console.error('Failed to copy to clipboard:', err);
            this.showToast('Failed to copy to clipboard', 'error');
        }
    }

    async addKey(keyName, keyValue, serviceName) {
        try {
            const response = await fetch(`${this.apiBaseUrl}/profiles/${this.currentProfile}/keys`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    name: keyName,
                    value: keyValue
                })
            });

            const data = await response.json();
            
            if (response.ok) {
                await this.loadKeys();
                await this.loadProfiles(); // Update key counts
                this.showToast(data.message || `Added key: ${keyName}`, 'success');
            } else {
                throw new Error(data.error || 'Failed to add key');
            }
        } catch (error) {
            console.error('Failed to add key:', error);
            this.showToast('Failed to add key: ' + error.message, 'error');
        }
    }

    async editKey(keyName) {
        const profile = this.profiles.get(this.currentProfile);
        const keyData = profile?.keys.get(keyName);
        
        if (!keyData) {
            this.showToast('Key not found', 'error');
            return;
        }

        this.showEditKeyModal(keyName, keyData.value, keyData.service);
    }

    async updateKey(keyName, keyValue) {
        try {
            const response = await fetch(`${this.apiBaseUrl}/profiles/${this.currentProfile}/keys/${keyName}`, {
                method: 'PUT',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    value: keyValue
                })
            });

            const data = await response.json();
            
            if (response.ok) {
                await this.loadKeys();
                this.showToast(data.message || `Updated key: ${keyName}`, 'success');
            } else {
                throw new Error(data.error || 'Failed to update key');
            }
        } catch (error) {
            console.error('Failed to update key:', error);
            this.showToast('Failed to update key: ' + error.message, 'error');
        }
    }

    async deleteKey(keyName) {
        if (!confirm(`Are you sure you want to delete the key "${keyName}"?`)) {
            return;
        }

        try {
            const response = await fetch(`${this.apiBaseUrl}/profiles/${this.currentProfile}/keys/${keyName}`, {
                method: 'DELETE'
            });

            const data = await response.json();
            
            if (response.ok) {
                await this.loadKeys();
                await this.loadProfiles(); // Update key counts
                this.showToast(data.message || `Deleted key: ${keyName}`, 'success');
            } else {
                throw new Error(data.error || 'Failed to delete key');
            }
        } catch (error) {
            console.error('Failed to delete key:', error);
            this.showToast('Failed to delete key: ' + error.message, 'error');
        }
    }

    async createProfile(profileName) {
        try {
            const response = await fetch(`${this.apiBaseUrl}/profiles`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    name: profileName
                })
            });

            const data = await response.json();
            
            if (response.ok) {
                await this.loadProfiles();
                this.renderProfiles();
                this.updateGlobalProfileSelector();
                this.showToast(data.message || `Created profile: ${profileName}`, 'success');
            } else {
                throw new Error(data.error || 'Failed to create profile');
            }
        } catch (error) {
            console.error('Failed to create profile:', error);
            this.showToast('Failed to create profile: ' + error.message, 'error');
        }
    }

    async deleteProfile(profileName) {
        if (profileName === 'default') {
            this.showToast('Cannot delete default profile', 'error');
            return;
        }

        if (!confirm(`Are you sure you want to delete the profile "${profileName}"?`)) {
            return;
        }

        try {
            const response = await fetch(`${this.apiBaseUrl}/profiles/${profileName}`, {
                method: 'DELETE'
            });

            const data = await response.json();
            
            if (response.ok) {
                await this.loadProfiles();
                this.renderProfiles();
                this.updateGlobalProfileSelector();
                
                // Switch to default profile if we deleted the current one
                if (this.currentProfile === profileName) {
                    this.setCurrentProfile('default');
                }
                
                this.showToast(`Deleted profile: ${profileName}`, 'success');
            } else {
                throw new Error(data.error || 'Failed to delete profile');
            }
        } catch (error) {
            console.error('Failed to delete profile:', error);
            this.showToast('Failed to delete profile: ' + error.message, 'error');
        }
    }

    testKey(keyName) {
        // Simulate API key testing
        const profile = this.profiles.get(this.currentProfile);
        const keyData = profile?.keys.get(keyName);
        
        if (keyData) {
            // Mark as tested (this is just UI simulation)
            keyData.tested = true;
            this.renderKeys();
            this.showToast(`Testing ${keyName}... (simulated)`, 'info');
        }
    }

    async refreshAll() {
        try {
            this.showToast('Refreshing data...', 'info');
            await this.loadServices();
            await this.loadProfiles();
            await this.loadKeys();
            this.loadData();
            this.showToast('Data refreshed successfully', 'success');
        } catch (error) {
            console.error('Failed to refresh:', error);
            this.showToast('Failed to refresh data: ' + error.message, 'error');
        }
    }

    // Modal methods
    showAddKeyModal() {
        const modal = document.getElementById('add-key-modal');
        if (modal) {
            modal.classList.remove('hidden');
            document.getElementById('add-key-name').focus();
        }
    }

    hideAddKeyModal() {
        const modal = document.getElementById('add-key-modal');
        if (modal) {
            modal.classList.add('hidden');
            // Clear form
            document.getElementById('add-key-name').value = '';
            document.getElementById('add-key-value').value = '';
            document.getElementById('add-key-service').value = '';
        }
    }

    showEditKeyModal(keyName, keyValue, serviceName) {
        const modal = document.getElementById('edit-key-modal');
        if (modal) {
            modal.classList.remove('hidden');
            document.getElementById('edit-key-name').textContent = keyName;
            document.getElementById('edit-key-value').value = keyValue;
            document.getElementById('edit-key-service').textContent = serviceName;
            document.getElementById('edit-key-value').focus();
        }
    }

    hideEditKeyModal() {
        const modal = document.getElementById('edit-key-modal');
        if (modal) {
            modal.classList.add('hidden');
        }
    }

    showCreateProfileModal() {
        const modal = document.getElementById('create-profile-modal');
        if (modal) {
            modal.classList.remove('hidden');
            document.getElementById('create-profile-name').focus();
        }
    }

    hideCreateProfileModal() {
        const modal = document.getElementById('create-profile-modal');
        if (modal) {
            modal.classList.add('hidden');
            document.getElementById('create-profile-name').value = '';
        }
    }

    showAddServiceModal() {
        const modal = document.getElementById('add-service-modal');
        if (modal) {
            modal.classList.remove('hidden');
            document.getElementById('add-service-name').focus();
        }
    }

    hideAddServiceModal() {
        const modal = document.getElementById('add-service-modal');
        if (modal) {
            modal.classList.add('hidden');
            // Clear form
            document.getElementById('add-service-name').value = '';
            document.getElementById('add-service-url').value = '';
            document.getElementById('add-service-pattern').value = '';
        }
    }

    // Service auto-detection
    detectServiceFromKeyName(keyName) {
        for (const [serviceName, service] of this.services.entries()) {
            if (service.keyPattern) {
                try {
                    const regex = new RegExp(service.keyPattern);
                    if (regex.test(keyName)) {
                        return serviceName;
                    }
                } catch (e) {
                    // Invalid regex pattern, skip
                }
            }
        }
        return 'Unknown';
    }

    // Toast notifications
    showToast(message, type = 'info') {
        const toast = document.createElement('div');
        const bgColor = {
            'success': 'bg-green-500',
            'error': 'bg-red-500',
            'warning': 'bg-yellow-500',
            'info': 'bg-blue-500'
        }[type] || 'bg-blue-500';

        toast.className = `fixed top-4 right-4 ${bgColor} text-white px-6 py-3 rounded-lg shadow-lg z-50 transform transition-transform duration-300 translate-x-full`;
        toast.textContent = message;

        document.body.appendChild(toast);

        // Slide in
        setTimeout(() => {
            toast.classList.remove('translate-x-full');
        }, 100);

        // Slide out and remove
        setTimeout(() => {
            toast.classList.add('translate-x-full');
            setTimeout(() => {
                document.body.removeChild(toast);
            }, 300);
        }, 3000);
    }
}

// Form submission handlers
document.addEventListener('DOMContentLoaded', function() {
    // Add Key Form
    document.getElementById('add-key-form').addEventListener('submit', function(e) {
        e.preventDefault();
        const keyName = document.getElementById('add-key-name').value.trim();
        const keyValue = document.getElementById('add-key-value').value.trim();
        const serviceName = document.getElementById('add-key-service').value.trim() || app.detectServiceFromKeyName(keyName);
        
        if (keyName && keyValue) {
            app.addKey(keyName, keyValue, serviceName);
            app.hideAddKeyModal();
        }
    });

    // Edit Key Form
    document.getElementById('edit-key-form').addEventListener('submit', function(e) {
        e.preventDefault();
        const keyName = document.getElementById('edit-key-name').textContent;
        const keyValue = document.getElementById('edit-key-value').value.trim();
        
        if (keyValue) {
            app.updateKey(keyName, keyValue);
            app.hideEditKeyModal();
        }
    });

    // Create Profile Form
    document.getElementById('create-profile-form').addEventListener('submit', function(e) {
        e.preventDefault();
        const profileName = document.getElementById('create-profile-name').value.trim();
        
        if (profileName) {
            app.createProfile(profileName);
            app.hideCreateProfileModal();
        }
    });

    // Modal close buttons
    document.querySelectorAll('[data-modal-close]').forEach(button => {
        button.addEventListener('click', function() {
            const modalId = this.getAttribute('data-modal-close');
            const modal = document.getElementById(modalId);
            if (modal) {
                modal.classList.add('hidden');
            }
        });
    });
});

// Initialize the application
const app = new AKWebApp();
#!/usr/bin/env python3
"""
AK Backend API Server
Provides REST API endpoints to access the same data files used by the AK CLI.
This allows the web interface to show real data instead of localStorage mock data.
"""

import os
import sys
import json
import base64
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from flask import Flask, jsonify, request
from flask_cors import CORS
import logging

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)
CORS(app)  # Enable CORS for web interface


class AKConfig:
    """Configuration class that mirrors the C++ Config struct"""

    def __init__(self):
        self.config_dir = self._get_config_dir()
        self.profiles_dir = os.path.join(self.config_dir, "profiles")
        self.persist_dir = os.path.join(self.config_dir, "persist")
        self.vault_path = os.path.join(self.config_dir, "keys.env.gpg")
        self.gpg_available = self._check_gpg_available()
        self.force_plain = os.getenv("AK_DISABLE_GPG", "").lower() in (
            "1",
            "true",
            "yes",
        )
        self.preset_passphrase = os.getenv("AK_PASSPHRASE", "")

        # Ensure directories exist
        os.makedirs(self.config_dir, exist_ok=True)
        os.makedirs(self.profiles_dir, exist_ok=True)
        os.makedirs(self.persist_dir, exist_ok=True)

    def _get_config_dir(self) -> str:
        """Get config directory following XDG Base Directory specification"""
        xdg_config = os.getenv("XDG_CONFIG_HOME")
        if xdg_config:
            return os.path.join(xdg_config, "ak")
        else:
            home = os.path.expanduser("~")
            return os.path.join(home, ".config", "ak")

    def _check_gpg_available(self) -> bool:
        """Check if GPG is available in the system"""
        try:
            result = subprocess.run(
                ["gpg", "--version"], capture_output=True, timeout=5
            )
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return False


class AKStorage:
    """Storage operations that mirror the C++ vault.cpp functions"""

    def __init__(self, config: AKConfig):
        self.config = config

    def profile_path(self, name: str) -> str:
        """Get path to profile file"""
        return os.path.join(self.config.profiles_dir, f"{name}.profile")

    def profile_keys_path(self, name: str) -> str:
        """Get path to profile keys file"""
        return os.path.join(self.config.profiles_dir, f"{name}.keys.gpg")

    def profile_keys_plain_path(self, name: str) -> str:
        """Get path to plain text profile keys file"""
        return os.path.join(self.config.profiles_dir, f"{name}.keys")

    def list_profiles(self) -> List[str]:
        """List all available profiles"""
        profiles = []
        if not os.path.exists(self.config.profiles_dir):
            return profiles

        for filename in os.listdir(self.config.profiles_dir):
            if filename.endswith(".profile"):
                profile_name = filename[:-8]  # Remove '.profile' extension
                profiles.append(profile_name)

        return sorted(profiles)

    def read_profile(self, name: str) -> List[str]:
        """Read profile key list"""
        profile_path = self.profile_path(name)
        if not os.path.exists(profile_path):
            return []

        try:
            with open(profile_path, "r") as f:
                lines = f.readlines()

            keys = []
            for line in lines:
                line = line.strip()
                if line:
                    keys.append(line)

            return keys
        except Exception as e:
            logger.error(f"Failed to read profile {name}: {e}")
            return []

    def write_profile(self, name: str, keys: List[str]) -> bool:
        """Write profile key list"""
        try:
            profile_path = self.profile_path(name)

            # Remove duplicates and sort
            unique_keys = sorted(list(set(keys)))

            with open(profile_path, "w") as f:
                for key in unique_keys:
                    f.write(f"{key}\n")

            return True
        except Exception as e:
            logger.error(f"Failed to write profile {name}: {e}")
            return False

    def decrypt_gpg_data(self, file_path: str) -> Optional[str]:
        """Decrypt GPG file and return content"""
        if not self.config.gpg_available or self.config.force_plain:
            return None

        try:
            cmd = ["gpg", "--quiet", "--decrypt", file_path]

            if self.config.preset_passphrase:
                # Create temporary passphrase file
                with tempfile.NamedTemporaryFile(
                    mode="w", delete=False, suffix=".pass"
                ) as f:
                    f.write(self.config.preset_passphrase)
                    pass_file = f.name

                try:
                    os.chmod(pass_file, 0o600)
                    cmd = [
                        "gpg",
                        "--batch",
                        "--yes",
                        "--quiet",
                        "--pinentry-mode",
                        "loopback",
                        "--passphrase-file",
                        pass_file,
                        "--decrypt",
                        file_path,
                    ]

                    result = subprocess.run(
                        cmd, capture_output=True, text=True, timeout=10
                    )
                    if result.returncode == 0:
                        return result.stdout
                finally:
                    os.unlink(pass_file)
            else:
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
                if result.returncode == 0:
                    return result.stdout

            return None
        except Exception as e:
            logger.error(f"Failed to decrypt {file_path}: {e}")
            return None

    def encrypt_gpg_data(self, data: str, file_path: str) -> bool:
        """Encrypt data and write to GPG file"""
        if not self.config.gpg_available or self.config.force_plain:
            return False

        try:
            # Write data to temporary file
            with tempfile.NamedTemporaryFile(
                mode="w", delete=False, suffix=".tmp"
            ) as f:
                f.write(data)
                tmp_file = f.name

            try:
                cmd = [
                    "gpg",
                    "--yes",
                    "-o",
                    file_path,
                    "--symmetric",
                    "--cipher-algo",
                    "AES256",
                    tmp_file,
                ]

                if self.config.preset_passphrase:
                    # Create temporary passphrase file
                    with tempfile.NamedTemporaryFile(
                        mode="w", delete=False, suffix=".pass"
                    ) as f:
                        f.write(self.config.preset_passphrase)
                        pass_file = f.name

                    try:
                        os.chmod(pass_file, 0o600)
                        cmd = [
                            "gpg",
                            "--batch",
                            "--yes",
                            "-o",
                            file_path,
                            "--pinentry-mode",
                            "loopback",
                            "--passphrase-file",
                            pass_file,
                            "--symmetric",
                            "--cipher-algo",
                            "AES256",
                            tmp_file,
                        ]

                        result = subprocess.run(cmd, capture_output=True, timeout=10)
                        return result.returncode == 0
                    finally:
                        os.unlink(pass_file)
                else:
                    result = subprocess.run(cmd, capture_output=True, timeout=10)
                    return result.returncode == 0
            finally:
                os.unlink(tmp_file)

        except Exception as e:
            logger.error(f"Failed to encrypt to {file_path}: {e}")
            return False

    def load_profile_keys(self, name: str) -> Dict[str, str]:
        """Load profile key-value pairs"""
        keys = {}

        # Try encrypted file first
        encrypted_path = self.profile_keys_path(name)
        plain_path = self.profile_keys_plain_path(name)

        data = None

        # Try to decrypt GPG file
        if os.path.exists(encrypted_path):
            data = self.decrypt_gpg_data(encrypted_path)

        # Fallback to plain text file
        if not data and os.path.exists(plain_path):
            try:
                with open(plain_path, "r") as f:
                    data = f.read()
            except Exception as e:
                logger.error(f"Failed to read plain keys for {name}: {e}")
                return keys

        if not data:
            return keys

        # Parse key=value lines
        for line in data.strip().split("\n"):
            line = line.strip()
            if not line or line.startswith("#"):
                continue

            if "=" in line:
                key, encoded_value = line.split("=", 1)
                try:
                    # Decode base64 value
                    value = base64.b64decode(encoded_value).decode("utf-8")
                    keys[key] = value
                except Exception as e:
                    logger.error(f"Failed to decode key {key}: {e}")

        return keys

    def save_profile_keys(self, name: str, keys: Dict[str, str]) -> bool:
        """Save profile key-value pairs"""
        try:
            # Prepare data in KEY=base64_value format
            lines = []
            for key, value in sorted(keys.items()):
                encoded_value = base64.b64encode(value.encode("utf-8")).decode("ascii")
                lines.append(f"{key}={encoded_value}")

            data = "\n".join(lines) + "\n" if lines else ""

            # Try to encrypt first
            encrypted_path = self.profile_keys_path(name)
            if self.encrypt_gpg_data(data, encrypted_path):
                return True

            # Fallback to plain text
            plain_path = self.profile_keys_plain_path(name)
            with open(plain_path, "w") as f:
                f.write(data)

            return True
        except Exception as e:
            logger.error(f"Failed to save keys for {name}: {e}")
            return False

    def ensure_default_profile(self):
        """Ensure default profile exists"""
        profiles = self.list_profiles()
        if "default" not in profiles:
            self.write_profile("default", [])


# Built-in services (mirrors the C++ services)
BUILTIN_SERVICES = {
    "OpenAI": {
        "name": "OpenAI",
        "isBuiltIn": True,
        "apiUrl": "https://api.openai.com/v1",
        "keyPattern": r"^OPENAI_.*",
    },
    "Anthropic": {
        "name": "Anthropic",
        "isBuiltIn": True,
        "apiUrl": "https://api.anthropic.com/v1",
        "keyPattern": r"^ANTHROPIC_.*",
    },
    "GitHub": {
        "name": "GitHub",
        "isBuiltIn": True,
        "apiUrl": "https://api.github.com",
        "keyPattern": r"^GITHUB_.*",
    },
    "GitLab": {
        "name": "GitLab",
        "isBuiltIn": True,
        "apiUrl": "https://gitlab.com/api/v4",
        "keyPattern": r"^GITLAB_.*",
    },
    "Google": {
        "name": "Google",
        "isBuiltIn": True,
        "apiUrl": "https://www.googleapis.com",
        "keyPattern": r"^GOOGLE_.*",
    },
    "AWS": {
        "name": "AWS",
        "isBuiltIn": True,
        "apiUrl": "https://aws.amazon.com",
        "keyPattern": r"^AWS_.*",
    },
    "Azure": {
        "name": "Azure",
        "isBuiltIn": True,
        "apiUrl": "https://management.azure.com",
        "keyPattern": r"^AZURE_.*",
    },
    "Stripe": {
        "name": "Stripe",
        "isBuiltIn": True,
        "apiUrl": "https://api.stripe.com/v1",
        "keyPattern": r"^STRIPE_.*",
    },
    "Twilio": {
        "name": "Twilio",
        "isBuiltIn": True,
        "apiUrl": "https://api.twilio.com",
        "keyPattern": r"^TWILIO_.*",
    },
    "SendGrid": {
        "name": "SendGrid",
        "isBuiltIn": True,
        "apiUrl": "https://api.sendgrid.com/v3",
        "keyPattern": r"^SENDGRID_.*",
    },
}

# Global instances
config = AKConfig()
storage = AKStorage(config)

# API Endpoints


@app.route("/api/health", methods=["GET"])
def health_check():
    """Health check endpoint"""
    return jsonify(
        {
            "status": "healthy",
            "config_dir": config.config_dir,
            "gpg_available": config.gpg_available,
        }
    )


@app.route("/api/profiles", methods=["GET"])
def get_profiles():
    """Get all profiles"""
    try:
        profiles = storage.list_profiles()
        profile_data = []

        for profile_name in profiles:
            keys = storage.load_profile_keys(profile_name)
            profile_data.append(
                {
                    "name": profile_name,
                    "keyCount": len(keys),
                    "isDefault": profile_name == "default",
                }
            )

        return jsonify({"profiles": profile_data})
    except Exception as e:
        logger.error(f"Failed to get profiles: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/api/profiles/<profile_name>/keys", methods=["GET"])
def get_profile_keys(profile_name):
    """Get all keys for a profile"""
    try:
        keys = storage.load_profile_keys(profile_name)
        key_list = []

        for key_name, value in keys.items():
            # Auto-detect service (simple pattern matching)
            service = "Unknown"
            for service_name, service_info in BUILTIN_SERVICES.items():
                import re

                if re.match(service_info["keyPattern"], key_name):
                    service = service_name
                    break

            key_list.append(
                {
                    "name": key_name,
                    "value": value,
                    "service": service,
                    "tested": False,  # We don't store test status
                }
            )

        return jsonify({"keys": key_list})
    except Exception as e:
        logger.error(f"Failed to get keys for profile {profile_name}: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/api/profiles/<profile_name>/keys", methods=["POST"])
def add_profile_key(profile_name):
    """Add a key to a profile"""
    try:
        data = request.get_json()
        if not data or "name" not in data or "value" not in data:
            return jsonify({"error": "Missing key name or value"}), 400

        key_name = data["name"]
        key_value = data["value"]

        # Load existing keys
        keys = storage.load_profile_keys(profile_name)
        keys[key_name] = key_value

        # Save keys
        if storage.save_profile_keys(profile_name, keys):
            # Update profile key list
            profile_keys = storage.read_profile(profile_name)
            if key_name not in profile_keys:
                profile_keys.append(key_name)
                storage.write_profile(profile_name, profile_keys)

            return jsonify({"message": "Key added successfully"})
        else:
            return jsonify({"error": "Failed to save key"}), 500

    except Exception as e:
        logger.error(f"Failed to add key to profile {profile_name}: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/api/profiles/<profile_name>/keys/<key_name>", methods=["PUT"])
def update_profile_key(profile_name, key_name):
    """Update a key in a profile"""
    try:
        data = request.get_json()
        if not data or "value" not in data:
            return jsonify({"error": "Missing key value"}), 400

        key_value = data["value"]

        # Load existing keys
        keys = storage.load_profile_keys(profile_name)
        keys[key_name] = key_value

        # Save keys
        if storage.save_profile_keys(profile_name, keys):
            return jsonify({"message": "Key updated successfully"})
        else:
            return jsonify({"error": "Failed to update key"}), 500

    except Exception as e:
        logger.error(f"Failed to update key {key_name} in profile {profile_name}: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/api/profiles/<profile_name>/keys/<key_name>", methods=["DELETE"])
def delete_profile_key(profile_name, key_name):
    """Delete a key from a profile"""
    try:
        # Load existing keys
        keys = storage.load_profile_keys(profile_name)
        if key_name in keys:
            del keys[key_name]

            # Save keys
            if storage.save_profile_keys(profile_name, keys):
                # Update profile key list
                profile_keys = storage.read_profile(profile_name)
                if key_name in profile_keys:
                    profile_keys.remove(key_name)
                    storage.write_profile(profile_name, profile_keys)

                return jsonify({"message": "Key deleted successfully"})
            else:
                return jsonify({"error": "Failed to save changes"}), 500
        else:
            return jsonify({"error": "Key not found"}), 404

    except Exception as e:
        logger.error(
            f"Failed to delete key {key_name} from profile {profile_name}: {e}"
        )
        return jsonify({"error": str(e)}), 500


@app.route("/api/profiles", methods=["POST"])
def create_profile():
    """Create a new profile"""
    try:
        data = request.get_json()
        if not data or "name" not in data:
            return jsonify({"error": "Missing profile name"}), 400

        profile_name = data["name"]

        # Check if profile already exists
        profiles = storage.list_profiles()
        if profile_name in profiles:
            return jsonify({"error": "Profile already exists"}), 409

        # Create empty profile
        if storage.write_profile(profile_name, []):
            return jsonify({"message": "Profile created successfully"})
        else:
            return jsonify({"error": "Failed to create profile"}), 500

    except Exception as e:
        logger.error(f"Failed to create profile: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/api/profiles/<profile_name>", methods=["DELETE"])
def delete_profile(profile_name):
    """Delete a profile"""
    try:
        if profile_name == "default":
            return jsonify({"error": "Cannot delete default profile"}), 400

        # Delete profile files
        profile_path = storage.profile_path(profile_name)
        encrypted_keys_path = storage.profile_keys_path(profile_name)
        plain_keys_path = storage.profile_keys_plain_path(profile_name)

        for path in [profile_path, encrypted_keys_path, plain_keys_path]:
            if os.path.exists(path):
                os.unlink(path)

        return jsonify({"message": "Profile deleted successfully"})

    except Exception as e:
        logger.error(f"Failed to delete profile {profile_name}: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/api/services", methods=["GET"])
def get_services():
    """Get all services"""
    try:
        services = list(BUILTIN_SERVICES.values())
        return jsonify({"services": services})
    except Exception as e:
        logger.error(f"Failed to get services: {e}")
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    # Ensure default profile exists
    storage.ensure_default_profile()

    print("🚀 AK Backend API Server starting...")
    print(f"📁 Config directory: {config.config_dir}")
    print(f"🔐 GPG available: {config.gpg_available}")
    print(f"🌐 Server will be available at: http://localhost:5000")

    app.run(host="0.0.0.0", port=5000, debug=True)

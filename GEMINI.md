# Gemini Code Assistant Context: `ak` Project

This document provides context for the `ak` project to the Gemini Code Assistant.

## Project Overview

`ak` is a command-line interface (CLI) tool for secure secret management, written in C++. It provides a vault-based key/value store with optional GPG encryption for secrets.

The main features include:
*   Setting, getting, listing, and removing secrets.
*   Profile management to group secrets.
*   Importing and exporting secrets from/to various formats (`.env`, `json`, `yaml`).
*   Utility functions like copying secrets to the clipboard, searching, and testing service connectivity.
*   Shell integration for auto-loading secrets into the environment.

The main source code is in `ak.cpp`, and the project is built using the `Makefile`.

## Building and Running

The project is built and managed using `make`. Here are the main commands:

*   **Build the project:**
    ```bash
    make
    ```
    This compiles the source code and creates the `ak` executable in the root directory.

*   **Install the application:**
    ```bash
    sudo make install
    ```
    This installs the `ak` binary to `/usr/local/bin/ak`.

*   **Run tests:**
    The project has a `test` command to check service connectivity. For example:
    ```bash
    ak test aws
    ```

*   **Clean the build:**
    ```bash
    make clean
    ```
    This removes the compiled binary and other temporary files.

*   **Create packages:**
    The `Makefile` also supports creating Debian and RPM packages:
    ```bash
    make package-deb
    make package-rpm
    ```

## Development Conventions

*   **Language:** The project is written in C++17.
*   **Build System:** `make` is used for building, installing, and packaging.
*   **Dependencies:** The tool has dependencies like `gpg`, `coreutils`, `bash`, and `curl`, which are listed in the packaging steps in the `Makefile`.
*   **Coding Style:** The code in `ak.cpp` follows a consistent style with clear comments and organization. New code should follow this style.
*   **Shell Integration:** The tool provides shell integration for `bash`, `zsh`, and `fish`. The installation is handled by `ak install-shell`.

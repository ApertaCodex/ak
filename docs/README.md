# AK Documentation

This directory contains comprehensive documentation for the AK API Key Manager.

## Files

- [`ak.1`](ak.1) - Manual page for the AK command-line tool
- [`MANUAL.md`](MANUAL.md) - Markdown version of the manual for web viewing

## Installing the Man Page

### Linux/macOS

After building AK, the man page can be installed system-wide:

```bash
# Copy to system man directory
sudo cp docs/ak.1 /usr/local/share/man/man1/
sudo mandb  # Update man database

# Or install to user directory
mkdir -p ~/.local/share/man/man1
cp docs/ak.1 ~/.local/share/man/man1/
```

### Viewing the Manual

Once installed, you can view the manual with:

```bash
man ak
```

Or view it directly from the source:

```bash
man ./docs/ak.1
```

## Documentation Sections

The AK manual covers:

- **Commands** - All available commands with detailed descriptions
- **Options** - Command-line flags and parameters
- **Supported Services** - Built-in API service integrations
- **Configuration** - Config files and environment variables
- **Examples** - Common usage patterns
- **Security** - Encryption and safety considerations
- **Troubleshooting** - Exit codes and error handling

## Contributing to Documentation

When updating the manual:

1. Edit the `ak.1` man page using proper groff syntax
2. Update the corresponding `MANUAL.md` with the same information
3. Test the man page: `man ./docs/ak.1`
4. Ensure examples are current and working

## Formatting Guidelines

- Use `.B` for bold text (commands, options)
- Use `.I` for italic text (arguments, placeholders)
- Use `.TP` for definition lists
- Include practical examples for each command
- Keep descriptions concise but complete
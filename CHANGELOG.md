# Changelog

## v1.1

### Fixed
- Asset export now creates a real ZIP archive with the selected game assets.
- Asset import no longer depends on Explorer's ZIP shell integration and now extracts directly, including on Windows XP.
- Release metadata and installer version updated for the 1.1 build.

### Added
- Versioned release notes for 1.1.

## v1.0

### Added
- LAN discovery and direct file transfer between PCs.
- Game folder sharing with local and LAN entries in one library.
- Steam-style library view with banners, icons, and launch button.
- Per-game launch executable selection.
- Assets support for banners, icons, and logos.
- Chat with formatting, emoji, and friend/PC presence.
- Persistent settings and LAN sharing preferences.
- Windows installer and portable zip distribution.

### Notes
- Designed to stay lightweight and usable on older Windows setups where supported.
- Public releases should use environment variables for API keys rather than hardcoding credentials.

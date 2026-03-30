@../CLAUDE.md

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Waterius** — open-source firmware for ESP8266/Attiny85-based IoT water meter counter. The device connects to household water meters via pulse sensors, counts pulses, and periodically sends readings to the Waterius cloud backend over Wi-Fi.

## Repository Structure

- `ESP8266/` — main ESP8266 firmware (Arduino/PlatformIO)
- `Attiny85/` — low-power counter firmware for Attiny85 co-processor
- `Board/` — PCB design files
- `Server/` — legacy server-side components
- `tests/` — test suite
- `files/` — supporting files and assets

## Important Rules

- **CRITICAL: Never commit to master from another branch.** Check current branch before every commit (`git branch --show-current`). Only commit to the current working branch.
- **NEVER use `git stash`.** Create a temporary commit instead.
- **NEVER use merge commits. ALWAYS rebase to preserve linear history.**
- **MANDATORY BEFORE EVERY `git push`: ALWAYS rebase onto fresh master.**

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-C6 Arduino sketch that drives a 32x8 LED matrix (chain of 4 MAX7219 chips) using the MD_Parola library, with automatic brightness control via a photoresistor. Two display modes: scrolling text and static display (selected at compile time via `#define`).

## Build (Arduino IDE)

1. **Board:** Install the ESP32 board package in Arduino IDE, then select "ESP32C6 Dev Module" (or your specific ESP32-C6 board).
2. **Libraries:** Install via Library Manager:
   - `MD_MAX72XX` by MajicDesigns
   - `MD_Parola` by MajicDesigns
3. **Build & upload:**
   - Open `MAX7219demo.ino` in Arduino IDE
   - Select the correct board and port (e.g., `/dev/ttyUSB0`)
   - Click Upload (or use `Sketch → Upload`)
4. **Serial monitor:** 115200 baud

No test framework is configured.

## Architecture

**`MAX7219demo.ino`** — Single-file Arduino sketch. MD_Parola handles SPI communication, text rendering, scrolling animation, and framebuffer management for the 4-chip MAX7219 chain. Brightness is controlled by reading a photoresistor via `analogRead()` and mapping it to one of 16 hardware intensity levels (`setIntensity(0-15)`).

## Hardware Pin Assignments (ESP32-C6)

- GPIO 0: DIN (SPI data)
- GPIO 2: CS (chip select)
- GPIO 4: CLK (SPI clock)
- GPIO 5: Photoresistor ADC input

Uses software SPI via MD_Parola to allow arbitrary GPIO selection.

## Key Design Decisions

- Uses MD_Parola's built-in `setIntensity(0-15)` for brightness control instead of the previous ISR-based PWM shutdown toggling. Simpler and more reliable.
- Brightness mapping is inverted: high ADC reading (dark room) maps to low intensity; low ADC reading (bright room) maps to high intensity.
- Display mode (scrolling vs. static) is selected at compile time by uncommenting the appropriate `#define` (`MODE_SCROLLING` or `MODE_STATIC`).
- Hardware type is set to `FC16_HW` — change to `GENERIC_HW`, `ICSTATION_HW`, or `PAROLA_HW` if your module wiring differs.

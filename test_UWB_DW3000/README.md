# 📡 UWB DW3000 Raw Ranging Characterization — Session 1 (B001 - B030)

Dokumentasi eksperimen dan karakterisasi pengukuran jarak mentah (*raw ranging*) berbasis teknologi **Ultra-Wideband (UWB)** menggunakan modul transceiver **Decawave DW3000** dan mikrokontroler **ESP32**. 

Pengujian ini bertujuan untuk menguji keandalan transmisi paket (*Packet Success Rate*), variabilitas jitter/standar deviasi, dan mendeteksi offset/bias sistematis (*antenna delay mismatch*) sebelum dilakukan proses kalibrasi lanjutan.

---

## 📋 Daftar Isi
- [1. Spesifikasi Hardware & Software](#1-spesifikasi-hardware--software)
- [2. Konfigurasi Radio DW3000 & Timing TWR](#2-konfigurasi-radio-dw3000--timing-twr)
- [3. Metodologi & Prosedur Pengujian](#3-metodologi--prosedur-pengujian)
- [4. Rekapitulasi Data Hasil Pengujian (Session 1)](#4-rekapitulasi-data-hasil-pengujian-session-1)
- [5. Analisis Hasil & Temuan Kunci](#5-analisis-hasil--temuan-kunci)
- [6. Panduan Menjalankan Pengujian](#6-panduan-menjalankan-pengujian)
- [7. Struktur Direktori & Kamus Data CSV](#7-struktur-direktori--kamus-data-csv)
- [8. Langkah Selanjutnya (Next Steps)](#8-langkah-selanjutnya-next-steps)

---

## 1. Spesifikasi Hardware & Software

### Hardware
* **Modul UWB:** Makerfabs ESP32 UWB DW3000 (Transceiver Decawave/Qorvo DW3000 terintegrasi dengan ESP32-WROOM-32)
* **Jumlah Perangkat:** 3 Unit (`DW01`, `DW02`, `DW03`)
* **Pinout SPI & Kendali:**
  * `PIN_RST` : GPIO 27
  * `PIN_IRQ` : GPIO 34
  * `PIN_SS` (CS) : GPIO 4
  * `SCK` / `MISO` / `MOSI` : SPI Default ESP32 (GPIO 18 / 19 / 23)
* **Instrumen Validasi Ground Truth:** Laser Distance Meter (akurasi milimeter)

### Software & Environment
* **Firmware:** Arduino C++ (Library DW3000 Decawave)
* **IDE Firmware:** Arduino IDE 2.x
* **Logger & Automation:** Python 3.x (Visual Studio Code)
  * Library utama: `pyserial`, `csv`, `datetime`
* **OS:** Windows (Serial Port `COM6`, Baud Rate: 115200 bps)

---

## 2. Konfigurasi Radio DW3000 & Timing TWR

Eksperimen ini menggunakan protokol **SS-TWR (Single-Sided Two-Way Ranging)** tanpa algoritma filtering, averaging, atau kalibrasi software untuk mendapatkan data fisik murni (*raw measurement*).

| Parameter | Nilai / Konfigurasi | Keterangan |
|---|---|---|
| **RF Channel** | Channel 5 (6489.6 MHz) | Center Frequency pita UWB standar |
| **Data Rate** | 6.8 Mbps (`DWT_BR_6M8`) | Kecepatan transfer paket RF |
| **Pulse Repetition Freq (PRF)** | 64 MHz | Default PRF DW3000 |
| **Preamble Length** | 128 simbol (`DWT_PLEN_128`) | Ukuran preamble transmisi |
| **PAC Size** | 8 (`DWT_PAC8`) | Preamble Acquisition Chunk |
| **TX/RX Preamble Code** | 9 | Standar Channel 5 |
| **SFD Mode** | Non-standard 8-symbol (`1`) | Start of Frame Delimiter |
| **TX/RX Antenna Delay** | 16385 (Default uncalibrated) | Nilai dasar untuk baseline karakterisasi |
| **Poll TX to Resp RX Delay** | 600 µs | Delay receiver window pada Initiator |
| **Resp RX Timeout** | 400 µs | Jendela waktu tunggu respon Initiator |
| **Poll RX to Resp TX Delay** | 900 µs | Waktu proses respon pada Responder |
| **Ranging Frequency** | 1 Hz (Interval 1000 ms) | Frekuensi request pengukuran jarak |

---

## 3. Metodologi & Prosedur Pengujian

Pengujian dilakukan dengan skema **Round-Robin Pair Testing** pada kondisi Line-of-Sight (LOS):

```mermaid
flowchart LR
    DW01["DW01 (Initiator)"] -->|"B001 - B010"| DW02["DW02 (Responder)"]
    DW02 -->|"B011 - B020"| DW03["DW03 (Responder)"]
    DW03 -->|"B021 - B030"| DW01["DW01 (Responder)"]

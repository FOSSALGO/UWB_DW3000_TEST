import serial
import csv
import os
import time
from datetime import datetime


# ============================================================
# CONFIGURATION
# ============================================================

SERIAL_PORT = "COM6"
BAUD_RATE = 115200

SESSION = "S01"
BLOCK = "B001"

INITIATOR = "DW01"
RESPONDER = "DW02"

# Nominal distance / titik pengujian
NOMINAL_DISTANCE_M = 0.5

# Hasil pengukuran aktual dengan laser
GROUND_TRUTH_M = 0.500

# Total attempts = DATA + FAIL
TARGET_ATTEMPTS = 100

# Warm-up setelah serial port dibuka
WARMUP_SECONDS = 120


# ============================================================
# CREATE OUTPUT FILE
# ============================================================

timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

filename = (
    f"{SESSION}_{BLOCK}_"
    f"{INITIATOR}_{RESPONDER}_"
    f"D{int(NOMINAL_DISTANCE_M * 100):03d}cm_"
    f"{timestamp}.csv"
)

output_dir = "data"

os.makedirs(output_dir, exist_ok=True)

filepath = os.path.join(
    output_dir,
    filename
)


# ============================================================
# OPEN SERIAL PORT
# ============================================================

print()
print("========================================")
print("      UWB DW3000 RAW DATA LOGGER")
print("========================================")
print()

print(f"Session      : {SESSION}")
print(f"Block        : {BLOCK}")
print(f"Port         : {SERIAL_PORT}")
print(f"Baud rate    : {BAUD_RATE}")
print(f"Initiator    : {INITIATOR}")
print(f"Responder    : {RESPONDER}")
print(f"Nominal dist.: {NOMINAL_DISTANCE_M:.3f} m")
print(f"Ground truth : {GROUND_TRUTH_M:.3f} m")
print(f"Target       : {TARGET_ATTEMPTS} attempts")

print()
print(f"Output file:")
print(filepath)
print()


ser = serial.Serial(
    port=SERIAL_PORT,
    baudrate=BAUD_RATE,
    timeout=2
)


# ============================================================
# WARM-UP
# ============================================================

print(
    f"Warm-up {WARMUP_SECONDS} seconds..."
)

time.sleep(WARMUP_SECONDS)

# Buang data yang masuk selama warm-up
ser.reset_input_buffer()

print("Warm-up finished.")
print()
print("Starting data acquisition...")
print()


# ============================================================
# CSV FILE
# ============================================================

csv_file = open(
    filepath,
    mode="w",
    newline="",
    encoding="utf-8"
)

writer = csv.writer(csv_file)


# ============================================================
# CSV HEADER
# ============================================================

writer.writerow([
    "timestamp",
    "session",
    "block",
    "type",
    "block_attempt_idx",
    "firmware_attempt_idx",
    "firmware_valid_idx",
    "elapsed_ms",
    "frame_seq",
    "status",
    "distance_m",
    "clock_offset_ratio",
    "status_reg",
    "nominal_distance_m",
    "ground_truth_m",
    "initiator",
    "responder"
])

csv_file.flush()


# ============================================================
# COUNTERS
# ============================================================

total_attempts = 0
valid_samples = 0
fail_samples = 0


# ============================================================
# MAIN LOOP
# ============================================================

try:

    while total_attempts < TARGET_ATTEMPTS:

        line = ser.readline().decode(
            "utf-8",
            errors="ignore"
        ).strip()


        # ----------------------------------------------------
        # Empty line
        # ----------------------------------------------------

        if not line:
            continue


        # ----------------------------------------------------
        # Ignore ESP32 startup messages
        # ----------------------------------------------------

        if line.startswith("#"):

            print(line)

            continue


        # ----------------------------------------------------
        # Ignore firmware CSV header
        # ----------------------------------------------------

        if line.startswith("type,"):

            continue


        # ----------------------------------------------------
        # PROCESS DATA
        # ----------------------------------------------------

        if line.startswith("DATA,"):

            parts = line.split(",")

            try:

                firmware_attempt_idx = parts[1]
                firmware_valid_idx = parts[2]
                elapsed_ms = parts[3]
                frame_seq = parts[4]
                status = parts[5]
                distance_m = parts[6]
                clock_offset_ratio = parts[7]
                status_reg = parts[8]


                # Every DATA = one ranging attempt
                total_attempts += 1
                valid_samples += 1


                current_time = datetime.now().isoformat(
                    timespec="milliseconds"
                )


                writer.writerow([
                    current_time,
                    SESSION,
                    BLOCK,
                    "DATA",
                    total_attempts,
                    firmware_attempt_idx,
                    firmware_valid_idx,
                    elapsed_ms,
                    frame_seq,
                    status,
                    distance_m,
                    clock_offset_ratio,
                    status_reg,
                    NOMINAL_DISTANCE_M,
                    GROUND_TRUTH_M,
                    INITIATOR,
                    RESPONDER
                ])

                csv_file.flush()


                print(
                    f"[{total_attempts:03d}/"
                    f"{TARGET_ATTEMPTS}] "
                    f"DATA | "
                    f"distance = "
                    f"{float(distance_m):.4f} m"
                )


            except (IndexError, ValueError) as e:

                print(
                    "Invalid DATA line:",
                    line
                )

                print(
                    "Error:",
                    e
                )


        # ----------------------------------------------------
        # PROCESS FAIL
        # ----------------------------------------------------

        elif line.startswith("FAIL,"):

            parts = line.split(",")

            try:

                firmware_attempt_idx = parts[1]
                firmware_valid_idx = parts[2]
                elapsed_ms = parts[3]
                frame_seq = parts[4]
                status = parts[5]
                distance_m = parts[6]
                clock_offset_ratio = parts[7]
                status_reg = parts[8]


                # FAIL juga dihitung sebagai attempt
                total_attempts += 1
                fail_samples += 1


                current_time = datetime.now().isoformat(
                    timespec="milliseconds"
                )


                writer.writerow([
                    current_time,
                    SESSION,
                    BLOCK,
                    "FAIL",
                    total_attempts,
                    firmware_attempt_idx,
                    firmware_valid_idx,
                    elapsed_ms,
                    frame_seq,
                    status,
                    distance_m,
                    clock_offset_ratio,
                    status_reg,
                    NOMINAL_DISTANCE_M,
                    GROUND_TRUTH_M,
                    INITIATOR,
                    RESPONDER
                ])

                csv_file.flush()


                print(
                    f"[{total_attempts:03d}/"
                    f"{TARGET_ATTEMPTS}] "
                    f"FAIL | "
                    f"status = {status}"
                )


            except (IndexError, ValueError) as e:

                print(
                    "Invalid FAIL line:",
                    line
                )

                print(
                    "Error:",
                    e
                )


        # ----------------------------------------------------
        # Other serial messages
        # ----------------------------------------------------

        else:

            print(line)


# ============================================================
# MANUAL STOP
# ============================================================

except KeyboardInterrupt:

    print()
    print(
        "Logging stopped by user."
    )


# ============================================================
# FINISH
# ============================================================

finally:

    csv_file.close()

    ser.close()


    if total_attempts > 0:

        valid_rate = (
            valid_samples /
            total_attempts *
            100
        )

    else:

        valid_rate = 0


    print()
    print("========================================")
    print("Logging finished.")
    print("========================================")

    print(
        f"Total attempts : "
        f"{total_attempts}"
    )

    print(
        f"Valid samples  : "
        f"{valid_samples}"
    )

    print(
        f"Failed attempts: "
        f"{fail_samples}"
    )

    print(
        f"Valid rate     : "
        f"{valid_rate:.2f}%"
    )

    print(
        f"File           : "
        f"{filepath}"
    )

    print("========================================")

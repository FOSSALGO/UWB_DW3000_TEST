import csv
import serial
from datetime import datetime

# ============================================================
# EXPERIMENT CONFIGURATION
# ============================================================

SERIAL_PORT = "COM5"       # Ganti sesuai COM ESP32 Anda
BAUD_RATE = 115200

PAIR_ID = "P01"

INITIATOR = "DW01"
RESPONDER = "DW02"

GROUND_TRUTH_M = 2.000

REPLICATE = 1

TARGET_VALID_SAMPLES = 500


# ============================================================
# CREATE FILE NAME
# ============================================================

distance_code = f"D{int(GROUND_TRUTH_M * 100):03d}"

filename = (
    f"S1_{PAIR_ID}_{distance_code}_"
    f"{INITIATOR}I_{RESPONDER}R_"
    f"R{REPLICATE}.csv"
)


# ============================================================
# CSV HEADER
# ============================================================

header = [
    "type",
    "attempt_idx",
    "valid_idx",
    "elapsed_ms",
    "frame_seq",
    "status",
    "distance_m",
    "clock_offset_ratio",
    "status_reg",
    "ground_truth_m",
    "pair_id",
    "initiator",
    "responder",
    "host_timestamp"
]


# ============================================================
# OPEN SERIAL PORT
# ============================================================

print("==============================================")
print("DW3000 UWB EXPERIMENT LOGGER")
print("==============================================")
print(f"Port          : {SERIAL_PORT}")
print(f"Pair          : {PAIR_ID}")
print(f"Initiator     : {INITIATOR}")
print(f"Responder     : {RESPONDER}")
print(f"Ground truth  : {GROUND_TRUTH_M:.3f} m")
print(f"Target valid  : {TARGET_VALID_SAMPLES}")
print(f"Output file   : {filename}")
print("==============================================")

ser = serial.Serial(
    SERIAL_PORT,
    BAUD_RATE,
    timeout=1
)

valid_count = 0


# ============================================================
# START LOGGING
# ============================================================

with open(filename, "w", newline="") as csvfile:

    writer = csv.writer(csvfile)

    writer.writerow(header)

    print("\nLogging started...\n")

    while valid_count < TARGET_VALID_SAMPLES:

        line = ser.readline().decode(
            "utf-8",
            errors="ignore"
        ).strip()

        if not line:
            continue


        # Show exactly what comes from ESP32
        print(line)


        # Ignore comments / startup information
        if line.startswith("#"):
            continue


        # Ignore ESP32 CSV header
        if line.startswith("type,"):
            continue


        # Only process DATA or FAIL
        if not (
            line.startswith("DATA,")
            or line.startswith("FAIL,")
        ):
            continue


        fields = line.split(",")


        # We expect 9 fields from ESP32
        if len(fields) != 9:
            print("WARNING: Invalid field count")
            continue


        host_timestamp = datetime.now().isoformat(
            timespec="milliseconds"
        )


        row = fields + [
            f"{GROUND_TRUTH_M:.3f}",
            PAIR_ID,
            INITIATOR,
            RESPONDER,
            host_timestamp
        ]


        writer.writerow(row)

        # Immediately save data to disk
        csvfile.flush()


        if fields[0] == "DATA":

            try:
                valid_count = int(fields[2])

            except ValueError:
                pass


        print(
            f"Progress: "
            f"{valid_count}/{TARGET_VALID_SAMPLES}"
        )


# ============================================================
# FINISHED
# ============================================================

ser.close()

print("\n==============================================")
print("EXPERIMENT COMPLETED")
print("==============================================")
print(f"Valid samples : {valid_count}")
print(f"Saved to      : {filename}")
print("==============================================")

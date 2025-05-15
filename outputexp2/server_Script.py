#!/usr/bin/env python3
import subprocess
import re
import time
import os

# Configuration
NUM_RUNS = 3
OUTPUT_FILE = "./output/run601hour.txt"

# Parameters to extract and average (updated with new fields)
PARAMS = {
    "Throughput Conn1": r"Throughput Conn1 = ([\d.]+) Mbps",
    "Throughput Conn2": r"Throughput Conn2 = ([\d.]+) Mbps",
    "Socket 1 before Socket 2": r"Socket 1 before Socket 2 = (\d+) times",
    "Socket 2 before Socket 1": r"Socket 2 before Socket 1 = (\d+) times",
    "Socket 2 and Socket 1": r"Socket 2 and Socket 1 = (\d+) times",
    "Conn1 bytes": r"Conn1 bytes = (\d+)",
    "Conn2 bytes": r"Conn2 bytes = (\d+)",
    "Conn1 packets": r"Conn1 packets = (\d+)",  # New
    "Conn2 packets": r"Conn2 packets = (\d+)",  # New
    "Conn1 duration": r"Conn1 duration = ([\d.]+) sec",  # New
    "Conn2 duration": r"Conn2 duration = ([\d.]+) sec",  # New
    "First Socket Count": r"First Socket Count = (\d+)",
    "Second Socket Count": r"Second Socket Count = (\d+)",
    "Total Time": r"Total Time = ([\d.]+) sec",
    "Ratio (Socket 1 before Socket 2 / Total)": r"Ratio \(Socket 1 before Socket 2 / Total\) = ([\d.]+)",
    "Ratio (Socket 2 before Socket 1 / Total)": r"Ratio \(Socket 2 before Socket 1 / Total\) = ([\d.]+)",
    "Throughput": r"Throughput = ([\d.]+) Mbps"
}

# Initialize output file
os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
open(OUTPUT_FILE, "w").close()

# Dictionaries to store sums and counts for averaging
param_sums = {param: 0.0 for param in PARAMS}
param_counts = {param: 0 for param in PARAMS}

def extract_params(output, run):
    """Extract parameters from server output and update sums/counts."""
    # Append raw output to run.txt
    with open(OUTPUT_FILE, "a") as f:
        f.write(f"Run {run} Output:\n{output}\n------------------------\n")

    # Extract parameters
    for param, pattern in PARAMS.items():
        match = re.search(pattern, output)
        if match:
            try:
                value = float(match.group(1))
                param_sums[param] += value
                param_counts[param] += 1
            except ValueError:
                print(f"Warning: Non-numeric value for {param} in run {run}")

def run_server(run):
    """Run server for one experiment."""
    print(f"Running server for experiment {run}...")
    temp_file = "server_temp_output.txt"

    try:
        # Start server and capture output
        with open(temp_file, "w") as f:
            server_process = subprocess.Popen(["./server"], stdout=f, stderr=subprocess.STDOUT)
            server_process.wait(timeout=7200)  # Adjusted timeout to 60 seconds

        # Read server output
        with open(temp_file, "r") as f:
            output = f.read()

        # Check if output contains expected data
        if "Throughput Conn1" not in output:
            print(f"Error: Incomplete or no output from server in run {run}")
            return False

        # Extract parameters
        extract_params(output, run)
        return True

    except subprocess.TimeoutExpired:
        print(f"Error: Server timed out in run {run}")
        server_process.kill()
        return False
    except FileNotFoundError:
        print("Error: ./server executable not found")
        return False
    except Exception as e:
        print(f"Error in run {run}: {e}")
        return False
    finally:
        # Clean up temp file
        if os.path.exists(temp_file):
            os.remove(temp_file)

# Run experiments
successful_runs = 0
for i in range(1, NUM_RUNS + 1):
    if run_server(i):
        successful_runs += 1
    # Delay to ensure client is ready for the next run
    time.sleep(2)  # Kept at 5 seconds for stability

# Append averages to run.txt
with open(OUTPUT_FILE, "a") as f:
    f.write(f"\nSummary - Average Parameter Values Across {successful_runs} Successful Runs:\n")
    for param in PARAMS:
        count = param_counts[param]
        if count > 0:
            avg = param_sums[param] / count
            f.write(f"{param} = {avg:.6f}\n")
        else:
            f.write(f"{param} = No valid data\n")

print(f"Experiments complete. {successful_runs}/{NUM_RUNS} runs successful. All data saved in {OUTPUT_FILE}.")

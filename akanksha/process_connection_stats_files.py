import re
import numpy as np
import matplotlib.pyplot as plt
import os

def parse_file(file_path):
    """
    Parse a single file to extract connection range summaries and the overall summary.
    Returns a list of dictionaries for connection ranges and a dictionary for the overall summary.
    """
    connection_summaries = []
    overall_summary = {}
    current_summary = None
    in_overall = False

    try:
        with open(file_path, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: File {file_path} not found.")
        return [], {}

    for line in lines:
        line = line.strip()
        # Detect start of a connection range summary
        if re.match(r"Summary for Connections \d+ to \d+:", line):
            if current_summary is not None:
                connection_summaries.append(current_summary)
            current_summary = {}
            in_overall = False
        # Detect start of overall summary
        elif line.startswith("Overall Statistics (Averaged):"):
            if current_summary is not None:
                connection_summaries.append(current_summary)
            current_summary = None
            in_overall = True
        # Parse metric lines
        elif line and ':' in line and (current_summary is not None or in_overall):
            key, value = map(str.strip, line.split(':', 1))
            # Clean key to make it consistent
            key = key.replace(' ', '_').lower()
            # Convert value to float if possible, otherwise keep as string
            try:
                value = float(value.split()[0])  # Extract number, ignore units
            except ValueError:
                value = value
            if in_overall:
                overall_summary[key] = value
            else:
                current_summary[key] = value

    # Append the last connection summary if exists
    if current_summary is not None:
        connection_summaries.append(current_summary)

    return connection_summaries, overall_summary

def average_metrics(summaries):
    """
    Compute the average of metrics across a list of summaries.
    Returns a dictionary with averaged metrics.
    """
    if not summaries:
        return {}
    avg_stats = {}
    keys = summaries[0].keys()
    for key in keys:
        values = [s[key] for s in summaries if isinstance(s[key], (int, float))]
        if values:
            avg_stats[key] = np.mean(values)
    return avg_stats

def main(file_paths):
    """
    Process multiple files, compute averages, bytes ratio, and CDF.
    """
    all_connection_summaries = []
    all_overall_summaries = []

    # Parse all files
    for file_path in file_paths:
        conn_summaries, overall_summary = parse_file(file_path)
        if conn_summaries:
            all_connection_summaries.append(conn_summaries)
        if overall_summary:
            all_overall_summaries.append(overall_summary)

    # Check if we have valid data
    if not all_connection_summaries or not all_overall_summaries:
        print("Error: No valid data parsed from files.")
        return

    # Average connection range statistics per file
    avg_stats_per_file = []
    for conn_summaries in all_connection_summaries:
        avg_stats = average_metrics(conn_summaries)
        if avg_stats:
            avg_stats_per_file.append(avg_stats)

    # Average connection range statistics across all files
    avg_stats_all = average_metrics(avg_stats_per_file)

    # Average overall summaries across all files
    avg_overall = average_metrics(all_overall_summaries)

    # Calculate bytes ratio
    bytes_ratio = None
    if 'avg_persistent_bytes' in avg_stats_all and 'avg_non_persistent_bytes' in avg_stats_all:
        if avg_stats_all['avg_non_persistent_bytes'] != 0:
            bytes_ratio = avg_stats_all['avg_persistent_bytes'] / avg_stats_all['avg_non_persistent_bytes']

    # Collect ratios for CDF
    persistent_ratios = []
    non_persistent_ratios = []
    for conn_summaries in all_connection_summaries:
        for summary in conn_summaries:
            if 'avg_persistent_ratio' in summary:
                persistent_ratios.append(summary['avg_persistent_ratio'])
            if 'avg_non_persistent_ratio' in summary:
                non_persistent_ratios.append(summary['avg_non_persistent_ratio'])

    # Generate CDF
    if persistent_ratios and non_persistent_ratios:
        persistent_ratios_sorted = np.sort(persistent_ratios)
        non_persistent_ratios_sorted = np.sort(non_persistent_ratios)
        cdf_persistent = np.arange(1, len(persistent_ratios_sorted) + 1) / len(persistent_ratios_sorted)
        cdf_non_persistent = np.arange(1, len(non_persistent_ratios_sorted) + 1) / len(non_persistent_ratios_sorted)

        # Plot CDF
        plt.figure(figsize=(10, 6))
        plt.plot(persistent_ratios_sorted, cdf_persistent, label='Persistent Ratio CDF', marker='o')
        plt.plot(non_persistent_ratios_sorted, cdf_non_persistent, label='Non-Persistent Ratio CDF', marker='s')
        plt.title('CDF of Persistent and Non-Persistent Ratios Across All Files')
        plt.xlabel('Ratio')
        plt.ylabel('Cumulative Probability')
        plt.legend()
        plt.grid(True)
        plt.savefig('ratio_cdf.png')
        plt.close()

    # Print results
    print("Averaged Statistics Across All Connection Ranges and Files:")
    for key, value in avg_stats_all.items():
        print(f"{key}: {value:.2f}")
    if bytes_ratio is not None:
        print(f"\nAverage Persistent Bytes / Average Non-Persistent Bytes Ratio: {bytes_ratio:.2f}")

    print("\nAveraged Overall Statistics Across All Files:")
    for key, value in avg_overall.items():
        print(f"{key}: {value:.2f}")

    # Save results to a file
    with open('averaged_connection_stats.txt', 'w') as f:
        f.write("Averaged Statistics Across All Connection Ranges and Files:\n")
        for key, value in avg_stats_all.items():
            f.write(f"{key}: {value:.2f}\n")
        if bytes_ratio is not None:
            f.write(f"\nAverage Persistent Bytes / Average Non-Persistent Bytes Ratio: {bytes_ratio:.2f}\n")
        f.write("\nAveraged Overall Statistics Across All Files:\n")
        for key, value in avg_overall.items():
            f.write(f"{key}: {value:.2f}\n")

if __name__ == "__main__":
    # Example file paths (replace with actual file paths)
    file_paths = [
        'stats_10000_1.log',
        'stats_10000_2.log',
        'stats_10000_3.log'  # Comment out this line for 2 files
    ]
    main(file_paths)
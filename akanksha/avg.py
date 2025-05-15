import re
import numpy as np
import matplotlib.pyplot as plt

def parse_connection_block(text):
    # Extract all desired fields except 'Connection 2 duration'
    fields = {
        'Avg Non-persistent count': [],
        'Avg Persistent count': [],
        'Avg Total when both present': [],
        'Avg Only persistent count': [],
        'Avg Only non-persistent count': [],
        'Avg Persistent bytes read': [],
        'Avg Non-persistent bytes read': [],
        'Avg Persistent ratio': [],
        'Avg Non-persistent ratio': [],
        'Avg Throughput Persistent': [],
        'Avg Throughput Non-Persistent': [],
        'Avg Persistent packets': [],
        'Avg Non-Persistent packets': [],
    }

    for field in fields.keys():
        matches = re.findall(rf'{re.escape(field)}:\s*([0-9\.]+)', text)
        fields[field] = list(map(float, matches))

    return fields

def parse_overall_statistics(text):
    persistent_duration = re.search(r'Total persistent duration:\s*([0-9\.]+)', text)
    nonpersistent_duration = re.search(r'Total non-persistent duration:\s*([0-9\.]+)', text)

    persistent_duration = float(persistent_duration.group(1)) if persistent_duration else 0.0
    nonpersistent_duration = float(nonpersistent_duration.group(1)) if nonpersistent_duration else 0.0

    return persistent_duration, nonpersistent_duration

def load_file(filename):
    with open(filename, 'r') as f:
        data = f.read()

    connection_part = data.split('Overall Statistics (Averaged):')[0]
    overall_statistics_part = data.split('Overall Statistics (Averaged):')[1]

    conn_fields = parse_connection_block(connection_part)
    pers_dur, nonpers_dur = parse_overall_statistics(overall_statistics_part)

    return conn_fields, pers_dur, nonpers_dur

def merge_fields(fields_list):
    merged = {}
    for fields in fields_list:
        for key in fields.keys():
            if key not in merged:
                merged[key] = fields[key]
            else:
                merged[key] += fields[key]
    return merged

def calculate_averages(merged_fields):
    avg = {}
    for key, values in merged_fields.items():
        avg[key] = np.mean(values)
    return avg

def plot_cdf(values, label, color):
    sorted_vals = np.sort(values)
    cdf = np.arange(1, len(sorted_vals)+1) / len(sorted_vals)
    plt.plot(sorted_vals, cdf, label=label, color=color)

def main(*files):
    # Load data from all files
    all_conn_fields = []
    all_pers_durations = []
    all_nonpers_durations = []

    for file in files:
        fields, pers_dur, nonpers_dur = load_file(file)
        all_conn_fields.append(fields)
        all_pers_durations.append(pers_dur)
        all_nonpers_durations.append(nonpers_dur)

    # Merge fields from all files
    merged_fields = merge_fields(all_conn_fields)

    # Compute averages
    avg_fields = calculate_averages(merged_fields)
    avg_persistent_duration = np.mean(all_pers_durations)
    avg_nonpersistent_duration = np.mean(all_nonpers_durations)

    # Print results
    print("\nSummary of Averages:\n")
    for key, val in avg_fields.items():
        print(f"{key}: {val:.2f}")
    print(f"\nTotal Persistent Duration (averaged from Overall Stats): {avg_persistent_duration:.6f} sec")
    print(f"Total Non-Persistent Duration (averaged from Overall Stats): {avg_nonpersistent_duration:.6f} sec")

    # Plot CDF graphs
    plt.figure(figsize=(8, 6))
    plot_cdf(merged_fields['Avg Persistent ratio'], 'Persistent Ratio', 'blue')
    plot_cdf(merged_fields['Avg Non-persistent ratio'], 'Non-Persistent Ratio', 'green')
    plt.xlabel('Ratio')
    plt.ylabel('CDF')
    plt.title('CDF of Persistent and Non-Persistent Ratios')
    plt.grid(True)
    plt.legend()
    plt.show()

if __name__ == "__main__":
    # Example usage for 3 files (you can add more files as needed)
    file1 = "stats_75000_1.log"  # Replace with your actual file names
    # file2 = "stats_100000_2.log"
    # file3 = "stats_100000_3.log"  # Add more files as needed
    main(file1)  # Pass as many files as required

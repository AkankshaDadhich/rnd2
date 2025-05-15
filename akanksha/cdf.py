import numpy as np
import matplotlib.pyplot as plt

def parse_file(filename):
    """
    Parse a file and return a list of dictionaries containing the summary data for each group,
    along with lists of ratios.
    """
    data = []
    current_group = {}
    persistent_ratios = []
    non_persistent_ratios = []
    in_summary = False
    
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: File {filename} not found.")
        return [], [], []

    for line in lines:
        line = line.strip()
        if not line:
            continue
        # Start of a new connection group summary
        if line.startswith("Summary for Connections"):
            if current_group:  # Save the previous group
                data.append(current_group)
            current_group = {}
            in_summary = True
        # End parsing if we reach the Overall Statistics section
        elif line.startswith("Overall Statistics"):
            in_summary = False
            if current_group:  # Save the last group before exiting
                data.append(current_group)
            break
        elif in_summary:
            # Parse key-value pairs (e.g., "Avg Non-persistent count: 256040.94")
            if ":" not in line:
                continue
            parts = line.split(":")
            if len(parts) < 2:
                continue
            key = parts[0].strip().replace("Avg ", "").replace(" ", "_").lower()
            # Extract the numerical value, handle units like 'sec' or 'Mbps'
            value_part = parts[1].strip().split()[0]
            try:
                value = float(value_part)
                current_group[key] = value
                if key == "persistent_ratio":
                    persistent_ratios.append(value)
                elif key == "non-persistent_ratio":
                    non_persistent_ratios.append(value)
            except ValueError:
                continue
    
    # Append the last group if we haven't already
    if current_group and in_summary:
        data.append(current_group)
    
    print(f"Parsed {len(data)} groups from {filename}")
    return data, persistent_ratios, non_persistent_ratios

def compute_overall_averages_and_ratios(files):
    """
    Compute overall averages across all connections and collect all ratios from multiple files.
    """
    all_data = []
    all_persistent_ratios = []
    all_non_persistent_ratios = []
    
    for file in files:
        file_data, persistent_ratios, non_persistent_ratios = parse_file(file)
        all_data.extend(file_data)
        all_persistent_ratios.extend(persistent_ratios)
        all_non_persistent_ratios.extend(non_persistent_ratios)
    
    if not all_data:
        print("No data to process.")
        return None, [], []

    # Compute overall averages
    overall_avg = {
        "non-persistent_count": np.mean([d["non-persistent_count"] for d in all_data]),
        "persistent_count": np.mean([d["persistent_count"] for d in all_data]),
        "total_when_both_present": np.mean([d["total_when_both_present"] for d in all_data]),
        "only_persistent_count": np.mean([d["only_persistent_count"] for d in all_data]),
        "only_non-persistent_count": np.mean([d["only_non-persistent_count"] for d in all_data]),
        "persistent_bytes_read": np.mean([d["persistent_bytes_read"] for d in all_data]),
        "non-persistent_bytes_read": np.mean([d["non-persistent_bytes_read"] for d in all_data]),
        "persistent_ratio": np.mean([d["persistent_ratio"] for d in all_data]),
        "non-persistent_ratio": np.mean([d["non-persistent_ratio"] for d in all_data]),
        "connection_2_duration": np.mean([d["connection_2_duration"] for d in all_data]),
        "throughput_persistent": np.mean([d["throughput_persistent"] for d in all_data]),
        "throughput_non-persistent": np.mean([d["throughput_non-persistent"] for d in all_data]),
        "persistent_packets": np.mean([d["persistent_packets"] for d in all_data]),
        "non-persistent_packets": np.mean([d["non-persistent_packets"] for d in all_data])
    }

    return overall_avg, all_persistent_ratios, all_non_persistent_ratios

def plot_cdf(all_persistent_ratios, all_non_persistent_ratios):
    """
    Generate and plot a single CDF graph for Persistent and Non-Persistent ratios.
    """
    # Sort ratios for CDF
    persistent_ratios_sorted = np.sort(all_persistent_ratios)
    non_persistent_ratios_sorted = np.sort(all_non_persistent_ratios)

    # Compute cumulative probabilities
    cdf_persistent = np.arange(1, len(persistent_ratios_sorted) + 1) / len(persistent_ratios_sorted)
    cdf_non_persistent = np.arange(1, len(non_persistent_ratios_sorted) + 1) / len(non_persistent_ratios_sorted)

    # Plot CDFs on a single graph
    plt.figure(figsize=(10, 6))
    plt.step(persistent_ratios_sorted, cdf_persistent, label='Persistent Ratio', color='blue')
    # plt.step(non_persistent_ratios_sorted, cdf_non_persistent, label='Non-Persistent Ratio', color='green')
    plt.title('CDF of Persistent before Non-Persistent Ratio (10k Packets per Connection)')
    plt.xlabel('Ratio')
    plt.ylabel('Cumulative Probability')
    plt.legend()
    plt.grid(True)
    plt.xlim(0.48, 0.55)  # Focus on the range of interest
    plt.savefig('ratio_cdf_all_files1.png')
    plt.close()

def main():
    """
    Main function to process files, compute averages, and plot CDFs.
    """
    #files = ["stats_100000_5.log", "stats_100000_4.log", "stats_100000_3.log", "stats_100000_2.log", "stats_100000_1.log", "stats_100000_1.log"]
    #files = ["stats_10000_2.log", "stats_10000_3.log", "stats_10000_4.log", "stats_10000_7.log", "stats_10000_6.log", "stats_10000_8.log","stats_10000_9.log"]
    files = [ "stats_10000_2.log", "stats_10000_7.log", "stats_10000_6.log", "stats_10000_8.log","stats_10000_9.log"]
    
    overall_avg, all_persistent_ratios, all_non_persistent_ratios = compute_overall_averages_and_ratios(files)
    
    if overall_avg:
        print("Overall Averages Across All Connections:")
        for key, value in overall_avg.items():
            print(f"{key.replace('_', ' ').title()}: {value:.2f}")
        
        print(f"Number of Persistent Ratio samples: {len(all_persistent_ratios)}")
        print(f"Number of Non-Persistent Ratio samples: {len(all_non_persistent_ratios)}")
        
        plot_cdf(all_persistent_ratios, all_non_persistent_ratios)
        print(f"CDF plot saved as 'ratio_cdf_all_files.png'")

if __name__ == "__main__":
    main()

        # file1 = "stats_10000_1.log"  # Replace with your actual file names
    # file7 = "stats_10000_2.log"
    # file8 = "stats_10000_3.log"
    # file2 = "stats_10000_4.log"  # Replace with your actual file names
    # file3 = "stats_10000_7.log"
    # file4 = "stats_10000_6.log"
    # file5 = "stats_10000_8.log"
    # file6 = "stats_10000_9.log"
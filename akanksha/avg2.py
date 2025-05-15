import re
import numpy as np

# Fields to calculate the average for in "Overall Statistics (Averaged)"
fields_to_average = [
    'Average events when both were present',
    'Average Persistent count',
    'Average Non-Persistent count',
    'Average Persistent Ratio',
    'Average Non-Persistent Ratio',
    'Rejected connections',
    'Average Only Persistent count',
    'Average Only Non Persistent count',
    'Average persistent bytes per connection',
    'Average non-persistent bytes per connection',
    'Average persistent packets per connection',
    'Average non-persistent packets per connection',
    'Total persistent duration',
    'Total non-persistent duration',
    'Average Throughput Persistent',
    'Average Throughput Non-Persistent'
]

def parse_overall_statistics(text):
    # Extract all desired fields
    fields = {field: [] for field in fields_to_average}

    for field in fields.keys():
        matches = re.findall(rf'{re.escape(field)}:\s*([0-9\.]+)', text)
        fields[field] = list(map(float, matches))

    return fields

def load_file(filename):
    with open(filename, 'r') as f:
        data = f.read()

    overall_statistics_part = data.split('Overall Statistics (Averaged):')[1]
    return parse_overall_statistics(overall_statistics_part)

def merge_fields(fields_list):
    merged = {field: [] for field in fields_to_average}
    for fields in fields_list:
        for key in fields.keys():
            merged[key] += fields[key]
    return merged

def calculate_averages(merged_fields):
    avg = {}
    for key, values in merged_fields.items():
        avg[key] = np.mean(values)
    return avg

def main(*files):
    # Load data from all files
    all_fields = []

    for file in files:
        fields = load_file(file)
        all_fields.append(fields)

    # Merge fields from all files
    merged_fields = merge_fields(all_fields)

    # Compute averages
    avg_fields = calculate_averages(merged_fields)

    # Print results for selected fields
    print("\nSummary of Averages for Overall Statistics (Averaged):\n")
    for key in fields_to_average:
        print(f"{key}: {avg_fields.get(key, 'N/A'):.5f}")

if __name__ == "__main__":
    # file1 = "stats_1000_1.log"  # Replace with your actual file names
    # file2 = "stats_1000_2.log"  # Replace with your actual file names
    # file3 = "stats_1000_3.log"
    # file4 = "stats_1000_4.log"
    # file5 = "stats_1000_5.log"
    # file6 = "stats_1000_6.log"

    # # Example usage for multiple files
    # file1 = "stats_2500_4.log"  # Replace with your actual file names
    # file2 = "stats_2500_5.log"  # Replace with your actual file names
    # file3 = "stats_2500_6.log"
    # file4 = "stats_2500_7.log"
    # file5 = "stats_2500_8.log"
    # file6 = "stats_2500_6.log"


    # file1 = "stats_7500_3.log"  # Replace with your actual file names
    # file2 = "stats_7500_4.log"  # Replace with your actual file names
    # file3 = "stats_7500_5.log"
    # file4 = "stats_7500_6.log"
    # file5 = "stats_7500_8.log"
    # file6 = "stats_7500_9.log"

    # file1 = "stats_10000_1.log"  # Replace with your actual file names
    # file7 = "stats_10000_2.log"
    # file8 = "stats_10000_3.log"
    # file2 = "stats_10000_4.log"  # Replace with your actual file names
    # file3 = "stats_10000_7.log"
    # file4 = "stats_10000_6.log"
    # file5 = "stats_10000_8.log"
    # file6 = "stats_10000_9.log"

    # file1 = "stats_25000_7.log"  # Replace with your actual file names
    # file2 = "stats_25000_4.log"  # Replace with your actual file names
    # file3 = "stats_25000_5.log"
    # file4 = "stats_25000_6.log"
    # file5 = "stats_25000_8.log"
    # file6 = "stats_25000_9.log"

    # file1 = "stats_50000_2.log"  # Replace with your actual file names
    # file2 = "stats_50000_4.log"  # Replace with your actual file names
    # file3 = "stats_50000_5.log"
    # file4 = "stats_50000_3.log"
    # file5 = "stats_50000_7.log"
    # file6 = "stats_50000_2.log"

    # file1 = "stats_75000_5.log"  # Replace with your actual file names
    # file2 = "stats_75000_6.log"  # Replace with your actual file names
    # file3 = "stats_75000_7.log"
    # file4 = "stats_75000_8.log"
    # file5 = "stats_75000_9.log"
    # file6 = "stats_75000_6.log"

    # file1 = "stats_1000000_1.log"  # Replace with your actual file names
    # file2 = "stats_1000000_2.log"  # Replace with your actual file names
    # file3 = "stats_1000000_3.log"
    # file4 = "stats_1000000_4.log"
    # file5 = "stats_1000000_1.log"
    # file6 = "stats_1000000_2.log"

    # file1 = "stats_5000_7.log"  # Replace with your actual file names
    # file2 = "stats_5000_4.log"  # Replace with your actual file names
    # file3 = "stats_5000_5.log"
    # file4 = "stats_5000_6.log"
    # file5 = "stats_5000_8.log"
    # file6 = "stats_5000_9.log"


    # file1 = "stats_100000_1.log"  # Replace with your actual file names
    # file2 = "stats_100000_2.log"  # Replace with your actual file names
    # file3 = "stats_100000_3.log"
    # file4 = "stats_100000_4.log"
    # file5 = "stats_100000_5.log"
    # file6 = "stats_100000_6.log"

    file1 = "stats_750000_1.log"  # Replace with your actual file names
    file2 = "stats_750000_2.log"  # Replace with your actual file names
    file3 = "stats_750000_3.log"
    file4 = "stats_750000_2.log"
    file5 = "stats_750000_2.log"
    file6 = "stats_750000_1.log"


    main(file1, file2, file3, file4 , file5, file6)  # Pass as many files as required

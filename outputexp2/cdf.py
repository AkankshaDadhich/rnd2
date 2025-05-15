import re
import numpy as np
import matplotlib.pyplot as plt

def parse_ratios(file_path):
    """
    Parse the input file to extract ratios for each run.
    Returns lists of ratios for Socket 1 before Socket 2 and Socket 2 before Socket 1.
    """
    socket1_ratios = []
    socket2_ratios = []

    try:
        with open(file_path, 'r') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: File {file_path} not found.")
        return [], []

    # Split the content into runs
    runs = content.split('------------------------')
    
    # Regex patterns for the ratios
    socket1_ratio_pattern = r"Ratio \(Socket 1 before Socket 2 / Total\) = (\d+\.\d+)"
    socket2_ratio_pattern = r"Ratio \(Socket 2 before Socket 1 / Total\) = (\d+\.\d+)"

    for run in runs:
        # Extract Socket 1 ratio
        socket1_match = re.search(socket1_ratio_pattern, run)
        if socket1_match:
            socket1_ratios.append(float(socket1_match.group(1)))
        
        # Extract Socket 2 ratio
        socket2_match = re.search(socket2_ratio_pattern, run)
        if socket2_match:
            socket2_ratios.append(float(socket2_match.group(1)))

    return socket1_ratios, socket2_ratios

def plot_cdf(socket1_ratios, socket2_ratios):
    """
    Generate and plot CDFs for the given ratios.
    """
    if not socket1_ratios or not socket2_ratios:
        print("Error: No ratios found to plot.")
        return

    # Sort ratios for CDF
    socket1_ratios_sorted = np.sort(socket1_ratios)
    socket2_ratios_sorted = np.sort(socket2_ratios)

    # Compute cumulative probabilities
    n1 = len(socket1_ratios_sorted)
    n2 = len(socket2_ratios_sorted)
    cdf_socket1 = np.arange(1, n1 + 1) / n1
    cdf_socket2 = np.arange(1, n2 + 1) / n2

    # Plot CDF
    plt.figure(figsize=(10, 6))
    plt.plot(socket1_ratios_sorted, cdf_socket1, label='Socket 1 before Socket 2 / Total', marker='o')
    #plt.plot(socket2_ratios_sorted, cdf_socket2, label='Socket 2 before Socket 1 / Total', marker='s')
    plt.title('CDF of Socket Processing Ratio')
    plt.xlabel('Ratio')
    plt.ylabel('Cumulative Probability')
    plt.legend()
    plt.grid(True)
    plt.savefig('ratio_cdf.png')
    plt.close()

def main(file_path):
    """
    Main function to parse ratios and generate CDF plot.
    """
    socket1_ratios, socket2_ratios = parse_ratios(file_path)
    plot_cdf(socket1_ratios, socket2_ratios)
    print(f"Processed {len(socket1_ratios)} runs. CDF plot saved as 'ratio_cdf.png'.")

if __name__ == "__main__":
    # Replace with the path to your input file
    file_path = '240bytes10min.txt'
    main(file_path)
import numpy as np
import matplotlib.pyplot as plt

def parse_ratios(file_path):
    """
    Parse the input file to extract bytes1/bytes2, Socket 1 before Socket 2, and Socket 2 before Socket 1 ratios.
    Returns lists of ratios for each category.
    """
    bytes_ratios = []
    socket1_ratios = []
    socket2_ratios = []

    try:
        with open(file_path, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: File {file_path} not found.")
        return [], [], []

    current_category = None
    for line in lines:
        line = line.strip()
        if line.startswith('bytes1/bytes2'):
            current_category = 'bytes'
            ratios = line.split()[1:]  # Skip the label
            bytes_ratios.extend(float(r) for r in ratios)
        elif line.startswith('Socket 1 before Socket 2'):
            current_category = 'socket1'
            ratios = line.split()[4:]  # Skip the label
            socket1_ratios.extend(float(r) for r in ratios)
        elif line.startswith('Socket 2 before Socket 1'):
            current_category = 'socket2'
            ratios = line.split()[4:]  # Skip the label
            socket2_ratios.extend(float(r) for r in ratios)

    return bytes_ratios, socket1_ratios, socket2_ratios

def plot_cdf(bytes_ratios, socket1_ratios, socket2_ratios):
    """
    Generate and plot CDFs for bytes1/bytes2 and Socket 1 before Socket 2 ratios.
    Optionally includes Socket 2 before Socket 1.
    """
    if not bytes_ratios or not socket1_ratios:
        print("Error: No ratios found to plot.")
        return

    # Sort ratios for CDF
    bytes_ratios_sorted = np.sort(bytes_ratios)
    socket1_ratios_sorted = np.sort(socket1_ratios)
    socket2_ratios_sorted = np.sort(socket2_ratios) if socket2_ratios else []

    # Compute cumulative probabilities
    n_bytes = len(bytes_ratios_sorted)
    n_socket1 = len(socket1_ratios_sorted)
    n_socket2 = len(socket2_ratios_sorted) if socket2_ratios else 0
    cdf_bytes = np.arange(1, n_bytes + 1) / n_bytes
    cdf_socket1 = np.arange(1, n_socket1 + 1) / n_socket1
    cdf_socket2 = np.arange(1, n_socket2 + 1) / n_socket2 if socket2_ratios else []

    # Plot CDFs
    plt.figure(figsize=(12, 8))
    
    # Subplot 1: bytes1/bytes2
    plt.subplot(2, 1, 1)
    plt.plot(bytes_ratios_sorted, cdf_bytes, label='bytes1/bytes2', marker='o', color='blue')
    plt.title('CDF of bytes1/bytes2 Ratio')
    plt.xlabel('Ratio (bytes1/bytes2)')
    plt.ylabel('Cumulative Probability')
    plt.legend()
    plt.grid(True)

    # Subplot 2: Socket 1 before Socket 2 (and optionally Socket 2 before Socket 1)
    plt.subplot(2, 1, 2)
    plt.plot(socket1_ratios_sorted, cdf_socket1, label='Socket 1 before Socket 2 / Total', marker='o', color='green')
    if socket2_ratios:
        plt.plot(socket2_ratios_sorted, cdf_socket2, label='Socket 2 before Socket 1 / Total', marker='s', color='orange')
    plt.title('CDF of Socket Processing Ratios')
    plt.xlabel('Ratio')
    plt.ylabel('Cumulative Probability')
    plt.legend()
    plt.grid(True)

    plt.tight_layout()
    plt.savefig('ratio_cdf2.png')
    plt.close()

def main(file_path):
    """
    Main function to parse ratios and generate CDF plots.
    """
    bytes_ratios, socket1_ratios, socket2_ratios = parse_ratios(file_path)
    plot_cdf(bytes_ratios, socket1_ratios, socket2_ratios)
    print(f"Processed {len(bytes_ratios)} runs. CDF plot saved as 'ratio_cdf.png'.")

if __name__ == "__main__":
    # Replace with the path to your input file
    file_path = 'ratio.txt'
    main(file_path)
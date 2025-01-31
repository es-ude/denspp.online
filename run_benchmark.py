import subprocess
import atexit
import time
import sys, yaml, os, select
import re
import statistics
import matplotlib.pyplot as plt

def get_microseconds(process):
    try:
        line = process.stdout.readline().decode('utf-8').strip()
        
        match = re.search(r'computed in: (\d+)us', line)
        match2 = re.search(r'Spikes Processed: (\d+)', line)

        if match2:
            spikes_processed = int(match2.group(1))
            print(f"Spikes Processed: {spikes_processed}")
        if match:
            microseconds = int(match.group(1))
            print(f"Extracted microseconds: {microseconds}")
            return microseconds
        else:
            return None
        
    except Exception as e:
        print(f"Error reading output: {e}")
        return None

def print_log(process):
    fds = [process.stdout.fileno(), process.stderr.fileno()]
    readable, _, _ = select.select(fds, [], [], 0.1)
    for fd in readable:
        if fd == process.stdout.fileno():
            output = os.read(fd, 1024).decode('utf-8').strip()
            if output:
                print(output)
        elif fd == process.stderr.fileno():
            error = os.read(fd, 1024).decode('utf-8').strip()
            if error:
                print("Standard Error:")
                print(error)

def do_benchmark(sampling_rate, n_channel):
    config_path = "config/benchmark.yaml"
    with open(config_path, 'r') as file:
        config = yaml.safe_load(file)
    
    config['sampling_rate'] = sampling_rate
    config['n_channel'] = n_channel

    with open(config_path, 'w') as file:
        yaml.dump(config, file)

    # start the processing:
    print("Starting Processing...")
    processing_program = subprocess.Popen(['./processing/cmake-build-debug/processing'] + [config_path], 
									   	    stdout=subprocess.PIPE, 
									   	    stderr=subprocess.PIPE)
    time.sleep(2)

    # start the simulation:
    print("Starting Simulation...")
    sim_program = subprocess.Popen(['./sim/cmake-build-debug/sim'] + [config_path], 
								        stdout=subprocess.PIPE, 
								        stderr=subprocess.PIPE)
    # wait for 5 seconds to stabilize
    print("Waiting for stabilization...")
    
    for i in range(10):
        print_log(processing_program)
        time.sleep(1)

    # record 20 seconds of the processing output
    ms_list = []
    print("Record numbers...")
    while len(ms_list) < 20:
        ms = get_microseconds(processing_program)
        if ms is not None:
            ms_list.append(ms)
        time.sleep(0.5)
    
    # kill the processes
    processing_program.kill()
    processing_program.wait()
    processing_program.stdout.close()
    processing_program.stderr.close()
    
    sim_program.kill()
    sim_program.wait()
    sim_program.stdout.close()
    sim_program.stderr.close()
    print("Processes killed")
    
    # if the output is always below 1 second, return True, else False
    if statistics.mean(ms_list) < 1010000:
        print(f"Realtime Factor achieved: {ms_list}")
        return True
    else:
        print(f"Realtime Factor not achieved: {ms_list}")
        return False

if __name__ == "__main__":
    print("Start of Benchmark")
    results = []
    
    prev_high = 1500 
    # iterate over the frequency range from 10,000 Hz to 100,000 Hz
    for sampling_rate in range(10000, 100001, 10000):
        print(f"Testing Frequency: {sampling_rate} Hz")
        
        low, high = 1, prev_high  # Initial range for the number of channels
        best_channel_count = 0
        
        while low <= high:
            mid = (low + high) // 2
            print(f"Testing {mid} channels at {sampling_rate} Hz")
            
            if do_benchmark(sampling_rate, mid):
                best_channel_count = mid
                low = mid + 1
            else:
                # If real-time factor is not achieved, reduce channels
                high = mid - 1
        
        print(f"Maximum channels for {sampling_rate} Hz: {best_channel_count}")
        results.append((best_channel_count, sampling_rate))
        prev_high = best_channel_count
        
        # Write the result to a file
        with open("benchmark_results.txt", "a") as file:
            file.write(f"{best_channel_count} channels at {sampling_rate} Hz\n")

    print("Benchmark concluded. View the results in benchmark_results.txt")
    print("Plotting results: ")
    
    # Prepare data for plotting
    channels, frequencies = zip(*results)
    
    # Plot the data
    plt.plot(frequencies, channels, marker='o', linestyle='-', color='b')
    plt.xlabel('Sampling Rate (hz)')
    plt.ylabel('Number of Channels')
    plt.title('Channel Count vs Frequency')
    plt.grid(True)
    plt.show()

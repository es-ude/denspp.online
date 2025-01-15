import subprocess, atexit
import time
import sys, yaml, os , select
import re
import statistics
import matplotlib.pyplot as plt

def get_microseconds(process):
    try:
        # Read the next line of the output
        line = process.stdout.readline().decode('utf-8').strip()
        
        # Regex to extract the microseconds part from the output
        match = re.search(r'computed in: (\d+)us', line)
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
    readable, _, _ = select.select(fds, [], [], 0.1)  # Non-blocking wait	
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
    # configure the config file 
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

    # record 10 seconds of the processing output
    ms_list = []
    print("Record numbers...")
    while len(ms_list) < 10:
        ms = get_microseconds(processing_program)
        if ms is not None:
            ms_list.append(ms)
        time.sleep(0.5)
    
    # kill the processes
    #processing_program.terminate() 
    processing_program.kill()
    processing_program.wait()
    
    processing_program.stdout.close()
    processing_program.stderr.close()
    
    #sim_program.terminate()
    sim_program.kill()
    sim_program.wait()
    sim_program.stdout.close()
    sim_program.stderr.close()
    print("Processes killed")
    # if the output is alway below 1 sec, return True, else False
    if statistics.mean(ms_list) < 1000000:
        print(f"Realtime Factor achieved: {ms_list}")
        return True
    else:
        print(f"Realtime Factor not achieved: {ms_list}")
        return False
        

if __name__ == "__main__":
    print("Start of Benchmark")
    results = []
    # iterate over the frequency range from 10000 - 100000 hz
    for idx,sampling_rate in enumerate(range(10000, 100001, 10000)):
        running = True
        n_channel = int(360/(idx+1))

        # increase channel count until realtime factor cant be achieved
        while(running):

            print(f"____________NEW BENCHMARK: _______________\nNumber of Channels: {n_channel}, Frequency: {sampling_rate}")
            print("___________________________________________")
            running = do_benchmark(sampling_rate, n_channel)
            if running:
                n_channel += 5
            else:
                print(f"Realtime Factor not achieved for {n_channel} channels, {sampling_rate} Hz")
                results.append((n_channel-5, sampling_rate-10000))
                # write the result to a file
                with open("benchmark_results.txt", "a") as file:
                    file.write(f"No RTF: {n_channel} channels, {sampling_rate} Hz\n")
                break
    print("Benchmark concluded, view the results in benchmark_results.txt")
    print("Plotting results: ")
    channels, frequencies = zip(*results)
    
    # Plot the data
    plt.plot(frequencies, channels, marker='o', linestyle='-', color='b')
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Number of Channels')
    plt.title('Channel Count vs Frequency')
    plt.grid(True)
    plt.show()
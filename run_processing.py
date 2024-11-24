import subprocess, atexit
import time
import sys, yaml, os , select


def print_log(processes):
	print("Printing Logs:")
	for process in processes:
		fds = [process.stdout.fileno(), process.stderr.fileno()]
		readable, _, _ = select.select(fds, [], [], 0.1)  # Non-blocking wait
		
		for fd in readable:
			if fd == process.stdout.fileno():
				output = os.read(fd, 1024).decode('utf-8').strip()
				if output:
					print("Standard Output:")
					print(output)
			elif fd == process.stderr.fileno():
				error = os.read(fd, 1024).decode('utf-8').strip()
				if error:
					print("Standard Error:")
					print(error)
	print("\n")


if __name__ == "__main__":

	config_path = str(sys.argv[1:]).strip("[]").strip("''")
	if not config_path:
		print("No config file specified. Using default.yaml")
		config_path = "config/default.yaml"

	with open(config_path, 'r') as file:
		config = yaml.safe_load(file)
		n_channel = config["n_channel"]	
		s_rate = config["sampling_rate"]
		do_plot = config["do_plot"]
		use_hw = config["use_hw"]
		use_layout = config["use_layout"]
		mapping_path = config["mapping_path"]
		


	print("Starting Processing with the following configuration:")
	for key, value in config.items():
		print(f"{key}: {value}")

	args = [config_path]  # adjust for the processing dir
	
	# 1. Start processing of Data -> holds until LSL Stream is connected (either sim or hw)
	processing_program = subprocess.Popen(['./processing/cmake-build-debug/processing'] + args, 
									   	    stdout=subprocess.PIPE, 
									   	    stderr=subprocess.PIPE)
	processes = [processing_program]
	print("Started LSL Stream Processing! Waiting for Stream...")
	time.sleep(2)

	
	# (2. Start the simulation depending of whether the processing is in hw or sim)
	if not use_hw:
		sim_program = subprocess.Popen(['./sim/cmake-build-debug/sim'] + args, 
								        stdout=subprocess.PIPE, 
								        stderr=subprocess.PIPE)
		print("Started LsLStream Simulation...")
		processes.append(sim_program)
	else:
		# explorepy push2lsl type shit
		print("Started LsLStream using Hardware...")
	


	# (3. Start the Plotting programm)
	if do_plot:
		plot_program = subprocess.Popen(['./plotting/build-Qt_LSL_Plotting-Desktop-Debug/Qt_LSL_Plotting'] + [mapping_path],
									stdout=subprocess.PIPE, 
									stderr=subprocess.PIPE)
		print("Started Data visualizer...")
		processes.append(plot_program)
	try:
		while(True):
			time.sleep(1)
			print_log(processes)

	except KeyboardInterrupt:
		print("Script interrupted")
		processing_program.terminate() 
		processing_program.kill()
		if not use_hw:
			sim_program.terminate()
			sim_program.kill()
		if do_plot:
			plot_program.terminate()
			plot_program.kill()



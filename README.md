# Online Processing Framework for Neural Signal Processing
This repository allows to perform online processing of neural data streams from a sensor system or simulation via LSL.

## Hardware Interfaces
Updates are coming

## Project Structure
- config: includes configuration files for the pipeline
- data: standard path for recorded and example datasets
- lib: shared library for the modules
- plotting: visualizer for LSL streams and Spike Waveforms
- processing: pipeline for online processing of neural data streams
- sim: simulate lsl streams from pre recorded datasets
## Installation

1. Install liblsl from source: 
```
git clone https://github.com/sccn/liblsl.git
```
```
mkdir build && cd build
```
```
cmake .. && make && sudo make install
```
2. Install libxdf from source:
```
git clone https://github.com/xdf-modules/libxdf.git 
```
```
mkdir build && cd build
```
```
cmake .. && make && sudo make install
```

3. install xml, matio, yaml packages:
```
sudo apt install libpugixml-dev
sudo apt install libmatio-dev
sudo apt install libyaml-cpp-dev
```


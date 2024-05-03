# SHIF-DRAM: meme-DRAM with a twist (or a shift?)

[![Academic Code](https://img.shields.io/badge/Origin-Academic%20Code-C1ACA0.svg?style=flat)]() [![Language Badge](https://img.shields.io/badge/Made%20with-C/C++-blue.svg)](https://isocpp.org/std/the-standard) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) [![contributions welcome](https://img.shields.io/badge/Contributions-welcome-lightgray.svg?style=flat)]() [![Preprint: arXiv](https://img.shields.io/badge/cs.AR-2402.19080-b31b1b?logo=arxiv&logoColor=red)](https://arxiv.org/pdf/2402.19080.pdf) 

SHIF-DRAM is based on MIMDRAM. MIMDRAM is a hardware/software co-designed processing-using-DRAM (PUD) system that introduces new mechanisms to allocate and control only the necessary resources for a given PUD operation. The key idea of MIMDRAM is to leverage fine-grained DRAM (i.e., the ability to independently access smaller segments of a large DRAM row) for PUD computation. MIMDRAM exploits this key idea to enable a multiple-instruction multiple-data (MIMD) execution model in each DRAM subarray (and SIMD execution within each DRAM row segment).

## List of changed files from MIMDRAM to SHIF-DRAM
```
.
+-- gem5/
|   +-- src/
|       +-- arch/
|           +-- x86/
|               +-- isa/
|                   +-- microops/
|                       +-- rowbit.isa
|                   +-- /decoder/
|                       +-- two_byte_opcodes.isa
|        +-- mem/
|            +-- request.hh
|            +-- abstract_mem.cc
|            +-- dram_ctrl.cc
```

## Setting up SHIF-DRAM
### Repository Structure and Installation
We point out next to the repository structure and some important folders and files.

```
.
+-- README.md
+-- gem5/
|   +-- build_opts/
|   +-- configs/
|   +-- configs/
|   +-- ext/
|   +-- protoc3/
|   +-- src/
|   +-- system/
|   +-- tests/
|   +-- util/
+-- microworkloads/
```

### Step 0: Prerequisites
Our framework requires all [gem5 dependencies](https://pages.cs.wisc.edu/~david/courses/cs752/Spring2015/gem5-tutorial/part1/building.html) and was tested using Ubuntu 14.04. Not sure why we went back to the stone age.

* To install gem5 dependencies:
```
sudo apt install build-essential git m4 scons zlib1g zlib1g-dev \
                 libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
                 python-dev python swig
```
* You may need to include the following line into gem5/SConstruct at line 555:
```
main.Append(CCFLAGS=['-DPROTOBUF_INLINE_NOT_IN_HEADERS=0'])
```

### Step 1: Installing the Simulator
To install the simulator:
```
cd gem5
scons build/X86/gem5.opt -j8
cd ..
```

### Step 2: Compiling the Workloads
To compile a workload, you need to link the source code with the **../gem5/util/m5** folder, as follows (we use the bitweave-buddy.c file as an example):
```
cd microworkloads/
gcc -static -std=c99 -O3 -msse2 -I ../gem5/util/m5 m5op_x86.S rowop.S** sad_plus.c -o sad_plus.exe
cd .. 
```

You can compile all workloads at once by using the compile.sh script
```
cd microworkloads/
./compile.sh
cd ..
```

### Step 3: Running a Simulation
After building gem5 and the workloads, you can execute a simulation as follows:
```
cd gem5/
./build/X86/gem5.opt configs/example/se.py --cpu-type=detailed --caches --l2cache --mem-type=DDR4_2400_x64 --mem-size=8192MB -c ABSOLUTE_PATH/sad_plus.exe -o "10 1"
```
Note that you *must* specify the *absolute path* (ABSOLUTE_PATH) to the folder containing the compiled workloads.

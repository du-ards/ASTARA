# ASTARA

ASTARA C++ dynamic simulation library for the primary loop of a pressurized-water reactor (PWR), 
with extensions for small modular reactors (SMRs) using helical-coil once-through 
steam generators. Also, the models are generalized enough to support other designs as
well. 

The over all design followed an OOP design and should be easily customizable. 


![CI](https://github.com/du-ards/ASTARA/actions/workflows/workflow.yml/badge.svg?branch=main)


# Dependencies

For the thermal fluid property lookup ASTARA relies on a third party lib called 
[CoolProp](https://github.com/coolprop/coolprop). The code is tested gcc-13.3.0 
version in Ubuntu 24.04 LTS and Debian version 13.0. For the build system it relies entirely 
on CMake (VERSION 3.15 or later ). For Windows support please 
reach out to any of the maintainers. 

# Setup

If you have a linux machine then you are already ready to go. 
Just copy the commands and run one by one.

```
git clone https://github.com/du-ards/ASTARA.git
cd ASTARA/externals
git submodule update --init --recursive CoolProp
cd ../

mkdir -p build 
cd build 
cd cmake .. && make -j $nproc
```
# Adding as a CMake project

We recommend that add this repo as a submodule and then add the repositiory as a subdirectory 
in your cmake build system. 

## References
The majority of the models were adapted from [Naghedolfeizi et al](https://trace.tennessee.edu/handle/20.500.14382/38656) and [Samet E Arda et al](https://www.researchgate.net/publication/301705924_Nonlinear_dynamic_modeling_and_simulation_of_a_passively_cooled_small_modular_reactor).


### Maintainer 
Ali Mahdi
Saad Islam
Ebny Walid Ahammed



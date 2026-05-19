FROM ubuntu:24.04



RUN apt-get update --yes
RUN apt-get install --yes \
        git \
        make \
        bison \
        cmake \
        g++ \
        mpich \
        python3 \
        curl \
        nano


RUN git clone --recurse https://github.com/du-ards/ASTARA.git ASTARA
RUN mkdir /ASTARA/build
RUN cd /ASTARA/build && cmake ..  &&    make -j${nproc}

FROM ubuntu:latest

RUN apt-get update && \
    apt-get install -y --no-install-recommends gcc-12 g++-12 libeigen3-dev libssl-dev git && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 --slave /usr/bin/g++ g++ /usr/bin/g++-12 && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 100 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 100 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /

RUN git clone --branch NN --depth 1 https://github.com/chart21/hpmpc && \
    cd hpmpc && \
    git clone --depth 1 https://github.com/chart21/flexNN SimpleNN

ENTRYPOINT ["/bin/bash"]
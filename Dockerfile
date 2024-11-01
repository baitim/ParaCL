FROM ubuntu:latest
WORKDIR /app
COPY . .

RUN apt-get -y update
RUN apt-get -y upgrade
RUN apt-get -y install g++
RUN apt-get -y install cmake
RUN apt-get -y install git
RUN apt-get -y install flex
RUN apt-get -y install bison

RUN git submodule update --init --recursive

RUN cmake . -B build -DCMAKE_BUILD_TYPE=Debug; cmake --build build
RUN ctest --test-dir build --rerun-failed --output-on-failure

RUN rm -rf build

RUN cmake . -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build
RUN ctest --test-dir build --rerun-failed --output-on-failure

ENTRYPOINT ["/app"]
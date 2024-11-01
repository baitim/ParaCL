FROM ubuntu:latest
WORKDIR /app
COPY . .
RUN apt-get -y update
RUN apt-get -y upgrade
RUN apt-get -y install g++
RUN apt-get -y install cmake
RUN apt-get -y install flex
RUN apt-get -y install bison

RUN cmake . -B build -DCMAKE_BUILD_TYPE=Debug; cmake --build build
ENTRYPOINT ["/app"]
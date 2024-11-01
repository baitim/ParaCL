FROM ubuntu:latest
WORKDIR /app
COPY . .
RUN apt-get -y update
RUN apt-get -y upgrade
RUN apt-get -y install cmake
RUN apt-get -y install flex
RUN apt-get -y install bison

RUN cmake . -B build -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake; cmake --build build
ENTRYPOINT ["/app"]
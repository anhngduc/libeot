## Setup the environment
* Optional: put eot font files to seeds folder

* Install dependencies [(ref)](https://github.com/google/fuzzing/blob/master/tutorial/libFuzzerTutorial.md): 

```shell
# Install git and get this tutorial
sudo apt-get --yes install git
git clone https://github.com/google/fuzzing.git fuzzing

# Get fuzzer-test-suite
git clone https://github.com/google/fuzzer-test-suite.git FTS

./fuzzing/tutorial/libFuzzer/install-deps.sh  # Get deps
./fuzzing/tutorial/libFuzzer/install-clang.sh # Get fresh clang binaries
```

## BUILD and run fuzz test
```shell
libeot$ mkdir build && cd build

libeot/build$ rm -rf ./* 
libeot/build$ cmake -DCMAKE_BUILD_TYPE=Debug ..
libeot/build$ make && mkdir MY_CORPUS
libeot/build$ ./fuzz_eot2ttf MY_CORPUS/ ../seeds/
```

## Example

Try comment out-of-bound check from this commit [
Check out-of-bound access](https://github.com/umanwizard/libeot/commit/591171bc49a31b7f8fb86cc33cf30ee11bad5038)
and build and run test:
```shell
libeot/build$ make
libeot/build$ ./fuzz_eot2ttf ../src/tests/crashes/crash-out-of-bound
```
=> SUMMARY: AddressSanitizer: SEGV (libeot/build/liblibeot.so+0x9807) in MTX_LZCOMP_UnPackMemory

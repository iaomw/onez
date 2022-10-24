export CMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake

echo $CMAKE_TOOLCHAIN_FILE

cmake -B build #-DCMAKE_EXPORT_COMPILE_COMMANDS=1

export MAKEFLAGS=-j$(nproc)

cmake --build build ${MAKEFLAGS}

#ln -s ./build/compile_commands.json ./

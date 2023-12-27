if [ -d "./build" ]; then
    rm -rf ./build
    echo "the build directory is deleted successfully."
fi

# for dir in $(find ./src -type d); do
#     if [[ $dir == *"shaders"* ]]; then
#         cd "$dir"
#         for file in * ".vert"; do
#             glslangValidator -V "$file" -o "${file%.*}.spv"
#         done
#         for file in * ".frag"; do
#             glslangValidator -V "$file" -o "${file%.*}.spv"
#         done
#         cd ..
#     fi
# done
# echo "the shaders is compiled."

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 
cmake --build build

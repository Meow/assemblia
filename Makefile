.PHONY: all build test clean

# nproc is GNU-only; fall back to sysctl on macOS/BSD.
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

all: configure build

configure:
	cmake -DCMAKE_BUILD_TYPE=Debug -B build -S .

build:
	cmake --build build --config Debug -j $(NPROC)

release:
	cmake -DCMAKE_BUILD_TYPE=Release -B build -S .
	cmake --build build --config Release -j $(NPROC)

dist:
	cmake -DCMAKE_BUILD_TYPE=Release -B build -S .
	cmake --build build --config Release -j $(NPROC)
	rm -rf dist
	mkdir dist
	cp build/engine/miniflow dist/miniflow
	cp -r assets/ dist/assets
	cp -r levels/ dist/levels
	cp -r lua/ dist/lua

exec:
	./build/engine/miniflow

run: configure build exec

lint:
	find ./src -type f -name '*.c' -exec clang-format -i {} \;
	find ./src -type f -name '*.h' -exec clang-format -i {} \;
	find ./src -type f -name '*.cpp' -exec clang-format -i {} \;
	find ./src -type f -name '*.hpp' -exec clang-format -i {} \;

analyze:
	find ./src -type f -name '*.c' -exec clang -Xanalyzer,--exclude,./vendor -I./src -I./vendor/glfw/include -I./vendor/stb -I./vendor/glad/include -I./vendor/vulkan-headers -I./vendor/luajit/src -I./vendor/simdjson -Wno-unused-command-line-argument --analyze -Xanalyzer -analyzer-output=text {} \;
	find ./src -type f -name '*.cpp' -exec clang -Xanalyzer,--exclude,./vendor -I./src -I./vendor/glfw/include -I./vendor/stb -I./vendor/glad/include -I./vendor/vulkan-headers -I./vendor/luajit/src -I./vendor/simdjson -Wno-unused-command-line-argument --analyze -Xanalyzer -analyzer-output=text {} \;

shaders:
	rm -f ./src/engine/render/shaders/*.spv
	find ./src/engine/render/shaders -type f -name '*.frag' -exec glslc -fshader-stage=fragment -o {}.spv {} \;
	find ./src/engine/render/shaders -type f -name '*.vert' -exec glslc -fshader-stage=vertex -o {}.spv {} \;
	find ./src/engine/render/shaders -type f -name '*.spv' -exec spirv-val {} \;
	ruby ./src/engine/render/shaders/gen_baked_shaders.rb ./src/engine/render/shaders/

clean:
	rm -rf build/
	rm -rf dist/

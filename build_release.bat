cmake -DCMAKE_BUILD_TYPE=Release -B build -S .
cmake --build build --config Release -j 16
rmdir /Q /S dist
mkdir dist
mkdir dist\assets
mkdir dist\levels
mkdir dist\lua
mkdir dist\shaders
copy /y build\engine\Release\miniflow.exe dist
xcopy /Y /S /E assets dist\assets
xcopy /Y /S /E levels dist\levels
xcopy /Y /S /E lua dist\lua
xcopy /Y /S /E shaders dist\shaders

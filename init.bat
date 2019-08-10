md build
cd build
REM cmake .. -G "Visual Studio 16" -DCMAKE_PREFIX_PATH="E:\DEVELOP\libx86\usr\local" -DPERL_LIBRARY="D:\Strawberry\c\lib" -DPerlEmbed_ARCHNAME="MSWin32-x64-multi-thread"
REM cmake .. -G "Visual Studio 16" -DCMAKE_PREFIX_PATH="E:\DEVELOP\libx86\usr\local" -DPERL_LIBRARY="D:\Strawberry\c\lib"
cmake .. -G "Visual Studio 16" -DCMAKE_PREFIX_PATH="E:\DEVELOP\libWin64\usr\local\usr\local" -DPERL_LIBRARY="D:\Strawberry\c\lib" -DBOOST_INCLUDEDIR="E:\DEVELOP\libWin64\usr\local\usr\local\include\boost-1_70\boost"




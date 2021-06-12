mkdir build install output
cmake -B build -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=install
cmake --build build --config %BUILD_TYPE%
cmake --install build --config %BUILD_TYPE%
7z a -tzip output\zrp-windows-%BUILD_TYPE%-%GITHUB_SHA:~0,7%.zip .\install\bin\*


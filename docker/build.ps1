# Build the Amiga binaries in Docker (bebbo gcc 6.5, the image AmiXcom uses).
#   .\docker\build.ps1          # -> build\amiga\simtest
#   .\docker\build.ps1 -Clean   # fresh CMake tree
param([switch]$Clean)

# Not 'Stop': PowerShell 5.1 turns docker's progress on stderr into errors.
$ErrorActionPreference = 'Continue'
$root  = (Resolve-Path "$PSScriptRoot\..").Path
$image = 'sacredbanana/amiga-compiler:m68k-amigaos'

$sh = 'export PATH=/opt/amiga/bin:$PATH; cd /src && ' +
      $(if ($Clean) { 'rm -rf /build/* && ' } else { '' }) +
      'cmake -S port -B /build -DCMAKE_TOOLCHAIN_FILE=/src/port/cmake/amiga-m68k.cmake ' +
      '-DCMAKE_BUILD_TYPE= >/dev/null && cmake --build /build -j $(nproc) && ' +
      'mkdir -p build/amiga && for f in simtest datacheck Settlers Settlers-hg modelist loadtest loadtest-fm mapgentest lantest; do cp /build/$f build/amiga/ && m68k-amigaos-strip -o build/amiga/$f.s build/amiga/$f; done && ' +
      'ls -l build/amiga'

# settlers-build volume holds the objects on the Linux filesystem (faster than
# compiling into a Windows bind mount).
docker run --rm -v "${root}:/src" -v "settlers-build:/build" $image sh -c $sh
exit $LASTEXITCODE

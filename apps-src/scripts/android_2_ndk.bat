@echo off
if not exist "%SDL_sdl%\libs\armeabi-v7a\libSDL2.so" goto exit

@echo on

set DST=%SCRIPTS%\..\linker\android\lib\armeabi-v7a\.
copy %SDL_sdl%\libs\armeabi-v7a\libSDL2.so %SDL_sdl_so_path%\.
copy %SDL_image%\libs\armeabi-v7a\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\armeabi-v7a\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\armeabi-v7a\libSDL2_ttf.so %DST%
copy %libvpx%\libs\armeabi-v7a\libvpx.so %DST%


@echo off

:exit

rem ABI_LEVEL: 21 hasn't arch-arm64
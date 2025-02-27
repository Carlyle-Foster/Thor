@echo off
cl.exe /nologo /I src /std:c++20 /GR- unity.cpp /link Winmm.lib /out:thor.exe
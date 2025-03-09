#!lua
local output = "./build/" .. _ACTION

solution "retrorun_solution"
   configurations { "Debug", "Release" }


project "retrorun"
   location (output)
   kind "ConsoleApp"
   language "C++"
   includedirs { "src/.", "." }
   files { "src/*.h", "src/*.cpp" }
   buildoptions { "-std=c++11 -Wall -D__GBM__" }
   linkoptions { "-Wl,-rpath=. -L../../. -lgo2 -ldl -lEGL -lGLESv2 -lrga" }
   
   configuration "Debug"
      flags { "Symbols" }
      defines { "DEBUG" }

   configuration "Release"
      flags { "Optimize" }
      defines { "NDEBUG" }

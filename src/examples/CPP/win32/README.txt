To build you need:
- compiled boost library: boost::thread (www.boost.org)
- compiled php embed2 sapi

You will probably need to change the paths for header files and libraries (boost and php) in Make.bat and possibly alter some compile/link switches

After successful compilation you need to either copy the missing .dll files to the directory of the test executable or place them in the windows system dir.
find_library(LOG4CXX_LIBRARIES log4cxx)
 
if(NOT LOG4CXX_LIBRARIES)
# Set a fallback default
	if(MINGW)
		set(LOG4CXX_INCLUDE_DIRS /opt/mingw/usr/i686pcmingw32/include)
		set(LOG4CXX_LIBRARIES log4cxx aprutil1 expat iconv apr1 rpcrt4 shell32 ws2_32 advapi32 kernel32 msvcrt)
	elseif(MSVC)
		set(LOG4CXX_INCLUDE_DIRS C:/opt/3rdparty/include)
		set(LOG4CXX_LIBRARIES C:/opt/3rdparty/lib/log4cxx.lib)
	else()
		set(LOG4CXX_INCLUDE_DIRS /usr/include)
	endif()
	if(MINGW)
		set(LOG4CXX_INCLUDE_DIRS /opt/mingw/usr/i686pcmingw32/include)
		set(LOG4CXX_LIBRARIES log4cxx aprutil1 expat iconv apr1 rpcrt4 shell32 ws2_32 advapi32 kernel32 msvcrt)
	elseif(MSVC)
		set(LOG4CXX_INCLUDE_DIRS C:/opt/3rdparty/include)
		set(LOG4CXX_LIBRARIES C:/opt/3rdparty/lib/log4cxx.lib)
	else()
		set(LOG4CXX_INCLUDE_DIRS /usr/include)
	endif()
endif()
 
 set(LOG4CXX_FOUND TRUE)
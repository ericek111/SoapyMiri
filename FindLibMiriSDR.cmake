
if(NOT LIBMIRISDR_FOUND)

	find_package(PkgConfig)
	pkg_check_modules (LIBMIRISDR_PKG libmirisdr)

	find_path(LIBMIRISDR_INCLUDE_DIRS NAMES mirisdr.h
		PATHS
		${LIBMIRISDR_PKG_INCLUDE_DIRS}
		/usr/include
		/usr/local/include
	)

	find_library(LIBMIRISDR_LIBRARIES NAMES mirisdr
		PATHS
		${LIBMIRISDR_PKG_LIBRARY_DIRS}
		/usr/lib
		/usr/local/lib
	)

	if (LIBMIRISDR_INCLUDE_DIRS AND LIBMIRISDR_LIBRARIES)
		set(LIBMIRISDR_FOUND TRUE CACHE INTERNAL "libmirisdr found")
		message(STATUS "Found libmirisdr: ${LIBMIRISDR_INCLUDE_DIRS}, ${LIBMIRISDR_LIBRARIES}")
	else ()
		set(LIBMIRISDR_FOUND FALSE CACHE INTERNAL "libmirisdr found")
		message(STATUS "libmirisdr not found.")
	endif ()

	mark_as_advanced(LIBMIRISDR_LIBRARIES LIBMIRISDR_INCLUDE_DIRS)

endif(NOT LIBMIRISDR_FOUND)

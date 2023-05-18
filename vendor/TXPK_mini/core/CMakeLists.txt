### paths ###
set(INCLUDE_DIR include)
set(SOURCE_DIR src)

set(CORE_INCLUDE_DIR			${INCLUDE_DIR}/TXPK/Core)
set(MATH_INCLUDE_DIR			${INCLUDE_DIR}/TXPK/Math)
set(PACKERS_INCLUDE_DIR			${INCLUDE_DIR}/TXPK/Packers)

set(CORE_SOURCE_DIR				${SOURCE_DIR}/Core)
set(MATH_SOURCE_DIR				${SOURCE_DIR}/Math)
set(PACKERS_SOURCE_DIR			${SOURCE_DIR}/Packers)


### headers ###
set(CORE_HEADERS
	${CORE_INCLUDE_DIR}/Common.hpp
	${CORE_INCLUDE_DIR}/Color.hpp
	${CORE_INCLUDE_DIR}/Bin.hpp
	${CORE_INCLUDE_DIR}/FitResult.hpp
	${CORE_INCLUDE_DIR}/Rectangle.hpp
	${CORE_INCLUDE_DIR}/Margin.hpp
	${CORE_INCLUDE_DIR}/SizeConstraintType.hpp
	${CORE_INCLUDE_DIR}/IPacker.hpp
	${CORE_INCLUDE_DIR}/IDataExporter.hpp
	${CORE_INCLUDE_DIR}/IKeyConverter.hpp
	${CORE_INCLUDE_DIR}/Comparison.hpp
	${CORE_INCLUDE_DIR}/Comparison.inl
	${CORE_INCLUDE_DIR}/VectorExtension.hpp
	${CORE_INCLUDE_DIR}/VectorExtension.inl
	${CORE_INCLUDE_DIR}/RectangleMerger.hpp
	${CORE_INCLUDE_DIR}/StringExtension.hpp
	${CORE_INCLUDE_DIR}/ServiceLocator.hpp
)

set(MATH_HEADERS
	${MATH_INCLUDE_DIR}/MathExtension.hpp
)

set(PACKERS_HEADERS
	${PACKERS_INCLUDE_DIR}/SameSizePacker.hpp
	${PACKERS_INCLUDE_DIR}/BlackspawnPacker.hpp
)

set(HEADERS
	${CORE_HEADERS}
	${MATH_HEADERS}
	${PACKERS_HEADERS}
)


### inlines ###
set(CORE_INLINES
	${CORE_INCLUDE_DIR}/Comparison.inl
	${CORE_INCLUDE_DIR}/Color.inl
	${CORE_INCLUDE_DIR}/Rectangle.inl
	${CORE_INCLUDE_DIR}/Texture.inl
	${CORE_INCLUDE_DIR}/VectorExtension.inl
	${CORE_INCLUDE_DIR}/StringExtension.inl
	${CORE_INCLUDE_DIR}/ServiceLocator.inl
)

set(INLINES
	${CORE_INLINES}
)


### sources ###
set(CORE_SOURCES
	${CORE_SOURCE_DIR}/Rectangle.cpp
	${CORE_SOURCE_DIR}/Margin.cpp
	${CORE_SOURCE_DIR}/Bin.cpp
	${CORE_SOURCE_DIR}/Texture.cpp
	${CORE_SOURCE_DIR}/RectangleMerger.cpp
	${CORE_SOURCE_DIR}/ServiceLocator.cpp
	${CORE_SOURCE_DIR}/StringExtension.cpp
)


set(MATH_SOURCES
	${MATH_SOURCE_DIR}/MathExtension.cpp
)

set(PACKERS_SOURCES
	${PACKERS_SOURCE_DIR}/SameSizePacker.cpp
	${PACKERS_SOURCE_DIR}/BlackspawnPacker.cpp
)

set(SOURCES
	${CORE_SOURCES}
	${MATH_SOURCES}
	${PACKERS_SOURCES}
)

add_library(${ENGINE_NAME} ${HEADERS} ${INLINES} ${SOURCES})
add_library(TXPK::Core-static ALIAS ${ENGINE_NAME})
target_include_directories(${ENGINE_NAME} PUBLIC ${INCLUDE_DIR})

## OpenIL
find_package(DevIL REQUIRED)

target_link_libraries(${ENGINE_NAME} DevIL::IL)

# target_link_libraries(${ENGINE_NAME} IL)
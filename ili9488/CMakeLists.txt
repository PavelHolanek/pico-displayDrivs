add_library(ili9488
	ili9488.c
)

target_include_directories(ili9488 PUBLIC
	.
)

target_link_libraries(ili9488 pico_stdlib hardware_spi hardware_dma)
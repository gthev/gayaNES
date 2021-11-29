emul_core:
	g++ -o emul_core emul_test.cpp nes_loaders/ines.cpp emulation_manager.cpp emulation_debug_cli.cpp ppu_render/ppu_render.cpp ppu_render/draw_tile.cpp cpu.cpp mem.cpp ppu_mem.cpp input_devices/device.cpp input_devices/nesjoypad.cpp sdl_utils.cpp mappers/mapper_resolve.cpp mappers/mapper2.cpp -lSDL2

emul_core_debug:
	g++ -g -o emul_core emul_test.cpp nes_loaders/ines.cpp emulation_manager.cpp emulation_debug_cli.cpp ppu_render/ppu_render.cpp ppu_render/draw_tile.cpp cpu.cpp mem.cpp ppu_mem.cpp input_devices/device.cpp input_devices/nesjoypad.cpp sdl_utils.cpp mappers/mapper_resolve.cpp mappers/mapper2.cpp -lSDL2
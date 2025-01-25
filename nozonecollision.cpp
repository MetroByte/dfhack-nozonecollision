/*
Copyright (c) 2025 Metrobyte.ru

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//TODO: Linux/Mac, find addresses, instructions. Make functions to search and replace memory without superuser, safely and effectively
#ifndef LINUX_BUILD //no linux for now

#pragma once

#include "PluginManager.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "df/building_civzonest.h"

DFHACK_PLUGIN("nozonecollision")
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

using namespace DFHack;

DFhackCExport command_result plugin_enable(color_ostream& out, bool enable);
DFhackCExport command_result plugin_init(color_ostream& out, std::vector <PluginCommand>& commands);


static size_t old_zone_address, new_zone_address;

static const std::vector<BYTE> instruction_for_new_zone = { 0x83, 0x4A, 0x24, 0x04 };
static const std::vector<BYTE> nop_for_new_zone = { 0x90, 0x90, 0x90, 0x90 };

static const std::vector<BYTE> instruction_for_old_zone = { 0x41, 0x83, 0x4C, 0x24, 0x24, 0x04 };
static const std::vector<BYTE> nop_for_old_zone = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

static const SIZE_T start_offset = 0x7FF600000000;
/* nothing for linux right now
#ifdef LINUX_BUILD
bool FindInstructionInMemory(const std::vector<uint8_t>& instructionToFind, size_t offset, size_t& foundAddress) {
	const size_t pageSize = sysconf(_SC_PAGESIZE);
	size_t bytesRead;

	// Map the entire process memory
	void* startAddress = mmap(nullptr, offset + 1, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (startAddress == MAP_FAILED) {
		std::cerr << "Failed to mmap process memory" << std::endl;
		return false;
	}

	for (size_t address = offset; address < offset + 1; address += pageSize) {
		void* mappedAddress = mmap(startAddress, pageSize, PROT_READ, MAP_PRIVATE | MAP_FIXED, -1, address);
		if (mappedAddress == MAP_FAILED) {
			std::cerr << "Failed to mmap page at address " << std::hex << address << std::endl;
			continue;
		}

		std::vector<uint8_t> buffer(pageSize);
		memcpy(buffer.data(), mappedAddress, pageSize);

		for (size_t i = 0; i <= pageSize - instructionToFind.size(); ++i) {
			if (std::equal(buffer.begin() + i, buffer.begin() + i + instructionToFind.size(), instructionToFind.begin())) {
				// Store the address of the found instruction
				foundAddress = address + i;
				// Clean up and unmap
				munmap(startAddress, offset + 1);
				return true;
			}
		}

		// Unmap the current page
		munmap(mappedAddress, pageSize);
	}

	// Clean up and unmap
	munmap(startAddress, offset + 1);
	return false;
}

#endif
*/
static bool FindInstructionInMemory(HANDLE hProcess, const std::vector<BYTE>& instructionToFind, SIZE_T offset, SIZE_T& foundAddress) {
	MEMORY_BASIC_INFORMATION mbi;
	SIZE_T bytesRead;

	LPVOID startAddress = (LPVOID)offset;

	for (LPVOID address = startAddress; VirtualQueryEx(hProcess, address, &mbi, sizeof(mbi)); address = (LPVOID)((BYTE*)address + mbi.RegionSize)) {
		if (mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS) {
			std::vector<BYTE> buffer(mbi.RegionSize);
			if (ReadProcessMemory(hProcess, address, buffer.data(), buffer.size(), &bytesRead) && bytesRead == buffer.size()) {
				for (SIZE_T i = 0; i <= bytesRead - instructionToFind.size(); ++i) {
					if (std::equal(buffer.begin() + i, buffer.begin() + i + instructionToFind.size(), instructionToFind.begin())) {
						// Store the address of the found instruction
						foundAddress = (SIZE_T)((BYTE*)address + i);
						return true;
					}
				}
			}
		}
	}
	return false;
}

static bool ReplaceInstructionInMemory(HANDLE hProcess, SIZE_T address, const std::vector<BYTE>& newInstruction) {
	SIZE_T bytesWritten;
	if (WriteProcessMemory(hProcess, (LPVOID)address, newInstruction.data(), newInstruction.size(), &bytesWritten) && bytesWritten == newInstruction.size()) {
		return true;
	}
	else {
		std::cerr << "NoZoneCollision: Failed to write to memory." << std::endl;
		return false;
	}
}

static bool PatchInstruction(color_ostream& out, HANDLE hProcess, const std::vector<BYTE>& instructionToFind, const std::vector<BYTE>& nopInstruction, SIZE_T start_offset, size_t& address) {
	if (address != 0 || FindInstructionInMemory(hProcess, instructionToFind, start_offset, address)) {
		if (ReplaceInstructionInMemory(hProcess, address, nopInstruction)) {
			out.print("NoZoneCollision: patched zone instruction at %p\n", address);
		}
		else {
			out.printerr("NoZoneCollision: error patching zone instruction\n");
			return false;
		}
	}
	return true;
}

static void search_and_replace(color_ostream& out, bool enable) {

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, GetCurrentProcessId());
	if (hProcess == NULL) {
		out.printerr("NoZoneCollision: Failed to open DF process for writing.\n");
		return;
	}

	if (!PatchInstruction(out, hProcess, instruction_for_new_zone, (enable ? nop_for_new_zone : instruction_for_new_zone), start_offset, new_zone_address)) {
		CloseHandle(hProcess);
		return;
	}

	if (!PatchInstruction(out, hProcess, instruction_for_old_zone, (enable ? nop_for_old_zone : instruction_for_old_zone), start_offset, old_zone_address)) {
		CloseHandle(hProcess);
		return;
	}
	out.color(enable ? COLOR_LIGHTCYAN : COLOR_LIGHTMAGENTA);
	out.print(enable ? "NoZoneCollision: patched successfully.\n" : "NoZoneCollision: patch reverted. Zones will get overlapping flags as usual.\n");
	out.reset_color();
	CloseHandle(hProcess);
}

DFhackCExport command_result plugin_enable(color_ostream& out, bool enable) {
	if (enable != is_enabled) {
		is_enabled = enable;
		search_and_replace(out, is_enabled);
		auto& zones = df::building_civzonest::get_vector();
		if (!zones.empty()) {
			for (auto zone : zones) {
				zone->flags.bits.room_collision = false;
			}
		}
	}
	return CR_OK;
}

command_result remove_collision(color_ostream& out, std::vector<std::string>& parameters) {
	if (std::find(parameters.begin(), parameters.end(), "apply") != parameters.end()) {
		auto& zones = df::building_civzonest::get_vector();
		if (!zones.empty()) {
			for (auto zone : zones) {
				zone->flags.bits.room_collision = false;
			}
		}
		out.print("NoZoneCollision: collisions removed\n");
	}
	else {
		out.print("No zone collision plugin.\n\nUsage:\n\nenable nozonecollision - remove collision set instructions\nnozonecollision apply - remove collision flags from all zones in a current game.\n");
	}
	return CR_OK;
}

DFhackCExport command_result plugin_init(color_ostream& out, std::vector <PluginCommand>& commands)
{
	commands.push_back(PluginCommand("nozonecollision", "Removes zone collision/overlap instructions", &remove_collision, "nozonecollision apply - remove overlap flags\nenable nozonecollision - patch game instructions responsible for zone overlap flags set\ndisable nozonecollision - revert the patch back"));
	return CR_OK;
}

DFhackCExport command_result plugin_shutdown(color_ostream& out, std::vector <PluginCommand>& commands)
{
	return CR_OK;
}

#endif // !LINUX_BUILD
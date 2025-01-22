# No zone overlap plugin for DFHack

This plugin searches and replaces two game assembly instructions responsible for assigning zone overlap/collision flags, with code that does nothing. Now you can place overlapping "rooms" without losing their value like in old classic DF.

# How to use

* Place `nozonecollision.plug.dll` into `hack\plugins` folder

* Enable plugin: `enable nozonecollision`

* If you want to revert it back, disable the plugin: `disable nozonecollision`

* `nozonecollision apply` will remove collision/overlap flags from any zone in the current game. This command will not patch the game. 

* You can add `enable nozonecollision` to the autolaunch file, located at:  `dfhack-config\init\dfhack.init` to enable it automatically on every launch.

## What does it do

1. Searches for instructions in the game memory, starting with offset `0x7FF600000000`. If failed, nothing will be done next.

2. Replaces instructions with NOP bytes `0x90`
   
   First instruction is `or dword ptr [rdx + 0x24], 4`, this is for newly created zones (zones getting placed above existing ones).
   
   Second instruction `or dword ptr [r12 + 0x24], 4` is for already created zones (bottom ones)

## Building

1. Place the source into `plugins\external` folder.

2. Add the following into `plugins\external\CMakeLists.txt` file:

```cmake
dfhack_plugin(nozonecollision nozonecollision.cpp)
```

3. Build as you usually build DFHack plugins: with `.bat` file or with Visual Studio DFHack solution.

# Important notes and considerations

* This plugin was made for Windows OS *(Linux is TODO)*;

* Works with DF 0.50.15. Searching for an address could future-proof address changes but newer versions can have another instructions for zone collision flags;

* Tested with Itch.io and Classic versions, not tested with Steam version;

* **If you did not enabled it again while playing a previous patched save, overlapping zones will get collision flags, enable this plugin to remove overlap flags;**

* Searching for addresses will lock the game for a while;

* Could crash the game;

* Could trigger protection software since it is modifying an executable memory.


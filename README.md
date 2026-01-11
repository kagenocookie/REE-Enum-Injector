## REE Enum Injector
REFramework plugin for injecting custom enum entries for RE Engine games. Since a lot of the games have hardcoded object IDs, this can be necessary for injecting custom content.

### Installation
- Download the latest release
- Place the .dll into your game's reframework/plugins folder.

### Config support
The plugin automatically scans the `reframework/data/injected_enums` folder for .txt files containing custom enum entries. A .txt file can contain any number of enums. The syntax is `@app.EnumClassname` to set the enum for the following assign statements and `Label 12345` pairs, one per line, of a label and value that should be added to the enum.

### Lua API

The plugin adds a global `content_injector` object with the following methods:
```lua
content_injector.add_enum_entry(enum: string|TypeDefinition, label: string, value: integer)
```
This adds a single entry to an enum.

Example: `content_injector.add_enum_entry('app.ItemIDEnum', 'It038887', 38887)`

```lua
content_injector.add_enum_entries(enum: string|TypeDefinition, entries: table<string,integer>)
```
This adds all the entries from the label-value table as new values to the target enum. Example:
```lua
content_injector.add_enum_entries('app.ItemIDEnum', {
    Label1 = 1234,
    Label2 = 4567,
})
```

To verify functionality ingame:
```lua

sdk.find_type_definition('System.Enum'):get_method('GetNames'):call(nil, sdk.find_type_definition('app.TopsStyle'):get_runtime_type()):add_ref()
```

### Building
Requires cmake and a valid windows c++ compiler

```sh
cmake -S . -B build
cmake --build build
```
Then copy the build/Debug/content_injector.dll into the game's reframework/plugins folder.

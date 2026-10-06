## REE Enum Injector
REFramework plugin for injecting custom enum entries for RE Engine games. Since a lot of the games have hardcoded object IDs, this can be necessary for injecting custom content.

Note that because the plugin works as a method hook and not a direct TDB edit, REF's ingame object explorer won't show the custom entries, and will instead assume it to be a "flag" type enum and show multiple unrelated values instead. This does not affect what the game code sees and is safe to ignore.

Depending on the specific case, the hook could trigger too late for the game to use it in the initial data setup, so it might not work out of the box for those cases. It should however work for anything after the initial setup. Some games might also have hardcoded mapping of enum values to other enums or enum values, or a separate static array of values, those can't be automatically covered by this plugin either because they're game specific.

### Requirements
Requires REFramework with Plugin version >= 1.14.0 (**Nightly build 01165 or above**), or whichever stable release newer than v1.5.9.1 when it happens.

### Installation
- Download the latest release
- Place the .dll into your game's reframework/plugins folder.

### Config support
The plugin automatically scans the `reframework/data/injected_enums` folder for .txt files containing custom enum entries. A .txt file can contain any number of enums. The syntax is `@app.EnumClassname` to set the enum for the following assign statements and `Label 12345` pairs, one per line, of a label and value that should be added to the enum. Example:
```
@app.EnumClassname
Label 12345
Label2 5678

@app.Enum2
NewLabel 42
```

### Lua API

The plugin adds a global `content_injector` object with the following methods:
```lua
-- Add a single entry to an enum
content_injector.add_enum_entry(enum: string|TypeDefinition, label: string, value: integer)

-- Example:
content_injector.add_enum_entry('app.ItemIDEnum', 'It038887', 38887)`
```

```lua
-- Add all the entries from the label-value table as new values to the target enum.
content_injector.add_enum_entries(enum: string|TypeDefinition, entries: table<string,integer>)

-- Example:
content_injector.add_enum_entries('app.ItemIDEnum', {
    Label1 = 1234,
    Label2 = 4567,
})
```

To verify functionality ingame:
```lua

sdk.find_type_definition('System.Enum'):get_method('GetNames'):call(nil, sdk.find_type_definition('app.TopsStyle'):get_runtime_type()):add_ref()
sdk.find_type_definition('System.Enum'):get_method('GetValues'):call(nil, sdk.find_type_definition('offline.gamemastering.Map.ID'):get_runtime_type())[326].value__
```

### Building
Requires cmake and a valid windows c++ compiler

```sh
# The debug build enables extra logging for troubleshooting
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
Then copy the build/Debug/content_injector.dll into the game's reframework/plugins folder.

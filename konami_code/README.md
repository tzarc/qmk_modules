This is adds Konami Code support to your build.

Add the following to the list of modules in your `keymap.json` to enable this module:

```json
{
    "modules": ["tzarc/konami_code"]
}
```

In your keymap, add a Konami Code handler:

```c
void konami_code_handler(void) {
    dprintf("Konami code entered!\n");
}
```

Build and flash your firmware. You can then do <kbd>Up</kbd>, <kbd>Up</kbd>, <kbd>Down</kbd>, <kbd>Down</kbd>, <kbd>Left</kbd>, <kbd>Right</kbd>, <kbd>Left</kbd>, <kbd>Right</kbd>, <kbd>B</kbd>, <kbd>A</kbd>, <kbd>Enter</kbd> to trigger the handler.

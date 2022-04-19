# A custom status bar

## Features
- dwm-ipc support
- easy custom block creation in C++
- quite a few built-in blocks

## Things to do
- [ ] Right aligned blocks

    Possible approaches:
    - Calling draw() twice with a FakeDraw then determining the width.
    - Calling draw() with a BufDraw that stores the draw instructions and only executes them after exiting from draw() with an offset. This could also be used to cache draw operations.
    - Adding a width() method where a block would need to precalculate it's width. (An error could be thrown if the width from draw() does not match the one from width() or the draw() method could be made void)

## Building
```command
$ ./build.sh
<a lot of stuff>
$ ls build/
... bar ...
```

As shown above the resulting executable will be placed in `build/bar`.

## Custom blocks
Create a new header file in `src/blocks` containing a class for your block.
If you created a C++ file to acompany the header then remember to add it to `CMakeLists.txt`.

```cpp
#include <cstddef>
#include <chrono>

// For map_range, hsl_to_rgb and rgb_to_long
#include "../util.hh"
// For EventLoop::duration
#include "../loop.hh"
// For the Block base class
#include "../block.hh"

// For 1s 100ms etc.
using namespace std::literals::chrono;

// A custom block is a C++ class deriving from Block.
class MyBlock : public Block {
    // Block state can be stored as data members.
    size_t _some_counter;
    size_t _hue = 0;
    Draw::color_t _color;

public:
    // Configuration should be passed via the constructor.
    MyBlock(size_t initial_value = 0)
        : _some_counter(initial_value), _color(0xFF0000) {}
    ~MyBlock() {}

    void late_init() override {
        // Initialisation should be handled either in the constructor or here if encountering static initialisation order issues.
        // In the future this may not be necessary but for this to be changed configuration will have to be handled a bit differently.
    }

    // The delta parameter here is usually unused but there if you need it.
    // If you want to make animations you should use animate() instead as draw() will only be called when necessary.
    size_t draw(Draw& draw, EventLoop::duration /* delta */) override {
        // -- Drawing code here --
        // Example drawing code:
        std::string str = "Counter: ";
        str += std::to_string(_some_counter);
        return draw.text(0, draw.vcenter(), str, _color);
    };
    
    void update(EventLoop::duration delta) override {
        // -- State update code here --
        // Things like extracting cpu usage information, or other information gathering should happen here.
        // This method will be called every update_interval().
        _some_counter += delta / 1s;
    }
    EventLoop::duration update_interval() override {
        return 1s;
    }
    
    void animate(EventLoop::duration delta) override {
        // -- Animation code here --
        // This method should also update data members similiar to update(), it's provided so that animations could be implemented independent of more resource intensive information gathering.
        // Similiar to update() it will be called every animate_interval().
        hue = (hue + 1) % 360;
        _color = rgb_to_long(hsl_to_rgb(map_range(hue, 0, 360, 0, 1), 1., .5));
    }
    EventLoop::duration animate_interval() override {
        return 50ms;
    }
}
```

After making your custom block you need to add an #include directive to `config.hh` and add it to the `blocks` array.

```cpp
// ...
#include "blocks/myblock.hh"
// ...

namespace config {
// ...

std::unique_ptr<Block> blocks = {
    // ...
    std::make_unique<MyBlock>(42),
};

// ...
}
```

Documentation for the `Draw` class will be provided here in the future.
For now you can look in `draw.hh`.

## Contributing
~~don't~~

Follow the code style, submit a PR.
Note that custom blocks may or may not be accepted.
Complex integrations with other programs like pipewire or pulseaudio should just be left to `ScriptBlock`s.
An extension system may be implemented to allow such integrations in C++, although it is unlikely I will bother with this.

Don't refactor the whole codebase and expect me to merge it though, I want to keep working on this.

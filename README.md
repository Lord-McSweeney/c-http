# c-http
minimal http browser in c

Missing features:
- HTTP keep-alive support (currently ignored)
- Asynchronous HTTP requests (currently everything blocks)
- All JavaScript
- Forms
- Relative query parameter resolving (i.e. links leading to "?param=value")
- Tables
- UTF-8
- Many HTML elements
    - &lt;ruby>
    - &lt;sub>
    - &lt;sup>
    - ...
- Correct rendering of block/inline layout (current impl is "good enough")
- Many CSS styles
    - position
    - top/left/right/bottom
    - width/height
    - background-color
    - border
    - margin/padding

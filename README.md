# ledwalld

A small C++ daemon for a Raspberry Pi that drives an RGB LED matrix wall. Other processes send it commands over a Unix domain socket, and it shows scrolling text or animated GIFs on the panels.

## Hardware assumptions

The panel setup is currently hard-coded in `led.cc`:

- **4 panels** on a single chain (`chain_length = 4`), connected through an **Adafruit RGB Matrix HAT** with the PWM mod (`adafruit-hat-pwm`).
- **U-arrangement wiring**: the chain runs along the top row (two panels), turns around, and comes back along the bottom row. The library's `U-mapper` pixel mapper folds this into one 2×2 display, so a chain of four 32×32 panels becomes a 64×64 canvas.

If your wall is laid out differently, change `chain_length`, `hardware_mapping` and `pixel_mapper_config` in `Led::Led()`.

## How it works

- **`ledwalld.cc`**: the main loop. It accepts a client on `/var/run/led.sock`, reads requests, and dispatches them to the LED controller.
- **`communicator.*`**: the socket server (see [Protocol](#protocol)).
- **`led.*`**: drives the matrix through [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix). Each action runs in its own thread and replaces whatever is currently showing.
- **`models/request.proto`**: the request format.

## Protocol

This is **not gRPC**. Clients connect to the Unix socket `/var/run/led.sock` and send framed messages:

```
[4-byte length, native byte order (little-endian on a Pi)] [serialized ledwalld.Request]
```

One connection can carry any number of messages. When the client disconnects, the daemon waits for the next one.

| Action    | Payload            | Effect                                                  |
|-----------|--------------------|---------------------------------------------------------|
| `TEXT`    | `textRequest.text` | Scrolls the text in a continuously shifting color      |
| `GIF`     | `gifRequest.gif`   | Decodes the image/GIF with GraphicsMagick and plays it |
| `CANCEL`  | none               | Stops the current action and clears the matrix         |
| `PICTURE` | none               | Not implemented yet                                     |
| `VIDEO`   | none               | Not implemented yet                                     |

Every request also carries a `sender` string, which is used for logging.

### Sending a command from the shell

Encode the request with `protoc`, then frame and send it with Python:

```sh
send() {
  protoc --encode=ledwalld.Request models/request.proto | sudo python3 -c '
import socket, struct, sys
data = sys.stdin.buffer.read()
s = socket.socket(socket.AF_UNIX)
s.connect("/var/run/led.sock")
s.sendall(struct.pack("=i", len(data)) + data)'
}

echo 'action: TEXT  sender: "cli"  textRequest { text: "Hello world" }' | send
echo 'action: CANCEL  sender: "cli"' | send
```

## Building

Tested on Debian bookworm and trixie (arm64) against the current upstream `rpi-rgb-led-matrix`:

```sh
sudo apt install build-essential pkg-config protobuf-compiler libprotobuf-dev \
                 libgraphicsmagick++1-dev libboost-date-time-dev
git submodule update --init
make
sudo ./ledwalld
```

`make` generates the protobuf sources from `models/request.proto`, so they always match the installed protobuf version. Run the binary from the repository root so it can find `fonts/8x13.bdf`. It needs root to access the GPIO pins.

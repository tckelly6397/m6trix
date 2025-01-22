# M6trix LED Cube - 6x6x6

Driver for my custom 6x6x6 LED cube. Runs on ESP32-S3 WROOM 1.

## How to Use
### Future (Ideally):
- Connect to the web server and select desired settings.

### Manually:
- Click the button to alternate animations.
- Long hold the button to activate debug mode.

## Programming
1. Open the project and run:
   ```
   source $HOME/esp/esp-idf/export.sh
   ```
   This sets the necessary environment variables.

2. Build and upload the code to the connected ESP32 by running:
   ```
   idf.py build
   ```

## Folder contents

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md
```

## Features
- **Animations**: Predefined LED animations to display on the cube.
- **Debug Mode**: Provides detailed output for troubleshooting and fine-tuning animations.
- **Web Control (Future Feature)**: A web interface for customizing settings and animations.

## Cube Settings

## Cube Animation List

## Cube blueprint images

## Cube downloads

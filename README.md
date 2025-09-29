# Rive JSON to RIV Converter

Production-ready converter for transforming JSON scene descriptions into binary Rive `.riv` files.

## 🎯 Features

### ✅ Fully Implemented (Production Ready)

**Shapes (7 types):**
- Rectangle
- Ellipse/Circle
- Triangle
- Polygon (customizable points)
- Star (with inner radius)
- Image (with asset references)
- ClippingShape (masking)

**Paint System:**
- Fill & Stroke
- SolidColor
- RadialGradient (multi-stop)
- LinearGradient (multi-stop)
- Dash effects (DashPath + Dash segments)

**Animation:**
- LinearAnimation with keyframes
- Properties: y-position, scaleX/Y, opacity
- Cubic interpolation (smooth easing)
- Multi-property animations

**Transform:**
- Position (x, y)
- Rotation
- Scale (X, Y, uniform)
- Opacity

### 🚧 Skeleton (Ready for Extension)

**Text Rendering:**
- Text object with position/size
- TextStyle basics
- Note: Full rendering requires TextRun + font assets

**State Machines:**
- StateMachine container
- Input placeholders
- Note: Full implementation needs States, Transitions, Listeners

**Constraints:**
- Schema ready
- Note: Requires bone system integration

### ⏳ Not Yet Implemented

- TrimPath (stroke/fill trimming)
- Feather (shadow/glow effects)
- Bones & Skinning
- Mesh deformation
- Audio playback
- ViewModel/Data binding

## 🚀 Quick Start

### Build
```bash
cmake -S . -B build_converter
cmake --build build_converter --target rive_convert_cli import_test
```

### Convert
```bash
./build_converter/converter/rive_convert_cli input.json output.riv
```

### Validate
```bash
./build_converter/converter/import_test output.riv
python3 converter/analyze_riv.py output.riv
```

## 📝 JSON Schema

### Basic Shape
```json
{
  "artboard": {
    "name": "MyArtboard",
    "width": 500,
    "height": 400
  },
  "shapes": [
    {
      "type": "rectangle",
      "x": 50,
      "y": 80,
      "width": 150,
      "height": 120,
      "fill": { "color": "#FF0000" },
      "stroke": {
        "color": "#000000",
        "thickness": 4,
        "dash": {
          "length": 10,
          "gap": 5
        }
      }
    }
  ]
}
```

### Gradient
```json
{
  "type": "circle",
  "x": 200,
  "y": 200,
  "width": 150,
  "height": 150,
  "gradient": {
    "type": "radial",
    "stops": [
      {"position": 0.0, "color": "#4A90E2"},
      {"position": 1.0, "color": "#A8D5FF"}
    ]
  }
}
```

### Animation
```json
{
  "animations": [
    {
      "name": "Bounce",
      "fps": 60,
      "duration": 120,
      "scaleKeyframes": [
        {"frame": 0, "value": 0.5},
        {"frame": 60, "value": 1.0},
        {"frame": 120, "value": 0.5}
      ]
    }
  ]
}
```

### Polygon/Star
```json
{
  "type": "star",
  "x": 300,
  "y": 300,
  "width": 150,
  "height": 150,
  "points": 5,
  "innerRadius": 0.5,
  "cornerRadius": 5.0,
  "fill": { "color": "#FFD700" }
}
```

### Text (Skeleton)
```json
{
  "texts": [
    {
      "content": "Hello Rive!",
      "x": 50,
      "y": 100,
      "width": 500,
      "height": 80,
      "style": {
        "fontFamily": "Inter",
        "fontSize": 48,
        "align": 1
      }
    }
  ]
}
```

## 📚 Examples

- `shapes_demo.json` - All shape types showcase
- `bouncing_ball.json` - Simple y-position animation
- `health_breathe.json` - Apple Health style meditation
- `all_shapes.json` - Complete shape type reference
- `dash_test.json` - Dashed stroke example
- `text_demo.json` - Text rendering skeleton

## 🏗️ Architecture

- `converter/include/json_loader.hpp` - JSON schema definitions
- `converter/src/json_loader.cpp` - JSON parser
- `converter/src/core_builder.cpp` - Object hierarchy builder
- `converter/src/serializer.cpp` - Binary RIV serializer
- `converter/analyze_riv.py` - Binary inspection tool

## 📖 Documentation

- `AGENTS.md` - Development workflow guide
- `converter/src/riv_structure.md` - Binary format specification

## ✨ Highlights

- **Production Ready:** Successfully generates `.riv` files that import cleanly in Rive Play
- **Tested:** All examples validated with official Rive runtime
- **Extensible:** Clean architecture for adding new features
- **Well Documented:** Comprehensive inline comments and guides

## 🎨 Demo Gallery

**Bouncing Ball:**
- Smooth cubic-eased vertical animation
- 60fps @ 2 seconds

**Health Breathe:**
- Apple Health inspired design
- Radial gradients with smooth color transitions
- 10-second breathing cycle
- Multi-property animation (scale + opacity)

**All Shapes:**
- Rectangle, Circle, Triangle, Hexagon, Star
- Demonstrates all geometric primitives

## 🤝 Contributing

The converter is built on reverse-engineered Rive runtime specifications. To add new features:

1. Check `include/rive/generated/` for type definitions
2. Update JSON schema in `json_loader.hpp`
3. Add parsing in `json_loader.cpp`
4. Implement in `core_builder.cpp`
5. Add property typeMap entries
6. Test with `import_test`
7. Validate in Rive Play

## 📄 License

Built on top of Rive Runtime (see original LICENSE)

## 🔗 Links

- [Rive](https://rive.app)
- [Rive Runtime Format Docs](https://rive.app/docs/runtimes/advanced-topic/format)
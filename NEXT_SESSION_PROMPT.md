# Next Session: State Machine Implementation

## 📋 Session Goal
Implement **State Machines** with Layers, States, and Transitions for interactive Rive animations.

---

## ✅ Current Status (What's Already Done)

### Tier 1 - Shapes & Animation: 100% ✅
- 8 shape types (Rectangle, Ellipse, Triangle, Polygon, Star, Image, ClippingShape, Path)
- Full paint system (Fill, Stroke, SolidColor, Gradients, Dash, Feather)
- Keyframe animation with cubic interpolation
- Transform system (position, rotation, scale, opacity)

### Tier 2 - Text Rendering: 100% ✅
- **FULLY WORKING IN RIVE PLAY** ✅
- Text (134), TextStylePaint (137), TextValueRun (135)
- Font embedding (Arial.ttf, 755KB)
- Variable fonts (TextStyleAxis - weight, width, slant)
- Text stroke/outline
- 37+ text properties
- Verified: "Hello World" renders correctly!

### State Machine: Partial (10% Complete)
- ✅ StateMachine container (53) - working
- ✅ Inputs working: StateMachineBool (59), StateMachineNumber (56), StateMachineTrigger (58)
- ❌ Layers, States, Transitions - NOT working yet

**Last commit:** `3bcd4058` - Text rendering complete  
**Repository:** https://github.com/adilyoltay/anime_mobile

---

## 🚧 The Problem with State Machine

### Issue Discovered:
StateMachine hierarchy uses **Core** objects (not Component), so they don't have `parentId`. Hierarchy is **implicit by file order**, not explicit parent-child relationships.

**Core Hierarchy (no parentId):**
```
StateMachine (53) extends Animation extends Core
  ├─ StateMachineInput (56/58/59) extends StateMachineComponent extends Core
  └─ StateMachineLayer (57) extends StateMachineComponent extends Core
      ├─ LayerState (61/62/63/64) extends StateMachineLayerComponent extends Core
      └─ StateTransition (65) extends StateMachineLayerComponent extends Core
          └─ TransitionCondition (67/68/69) extends Core
```

### Current Behavior:
- ✅ StateMachine + Inputs: **WORKS** (tested, import successful)
- ❌ Adding Layer: **MALFORMED** (import fails)
- ❌ Adding States: **MALFORMED**

### Root Cause:
We're treating StateMachine objects like Component objects (using `setParent()`), but they don't support parentId. Runtime uses **implicit import order** instead.

---

## 🎯 Next Session Tasks

### Task 1: Analyze Reference Files
**Files to analyze:**
- `renderer/webgpu_player/rivs/fire_button.riv` (has 2 layers)
- `renderer/webgpu_player/rivs/new_file.riv` (has 1 layer)
- Any other small .riv with state machines

**What to find:**
1. Object ordering in file (what comes after StateMachine?)
2. How layers are positioned in stream
3. How states relate to layers (implicit parent by order?)
4. Property keys used

**Tools:**
```bash
./build_converter/converter/import_test renderer/webgpu_player/rivs/fire_button.riv
python3 converter/analyze_riv.py renderer/webgpu_player/rivs/fire_button.riv --json
```

### Task 2: Study Runtime Importer
**Files to read:**
- `src/importers/state_machine_importer.cpp`
- `src/importers/state_machine_layer_importer.cpp`  
- `src/importers/layer_state_importer.cpp`

**Key questions:**
1. How does `StateMachineImporter::addLayer()` work?
2. How does `StateMachineLayerImporter::addState()` work?
3. Is there an implicit "current parent" during import?
4. Do we need special ImportStack handling?

### Task 3: Implement Correct Pattern
Based on findings from Task 1 & 2:
1. Determine correct object ordering
2. Remove `setParent()` calls (they're causing malformed files)
3. Write StateMachine objects in correct sequence
4. Test with minimal example (1 layer, 2 states, 0 transitions)

### Task 4: Add AnimationState.animationId
**Challenge:** animationId references LinearAnimation
- Animations are built AFTER StateMachine in our code
- But animationId needs the artboard-local index
- Solution: Either reorder build, or use placeholder + remap in serializer

### Task 5: Add Transitions
- StateTransition (65)
- TransitionCondition (67/68)
- Property: stateToId (151) - index to target state

### Task 6: Create Working Demos
1. Simple button (idle ↔ hover states)
2. Toggle switch (bool input)
3. Interactive animation

---

## 📚 Key Information

### TypeKeys (State Machine)
| TypeKey | Name | Parent Type | Properties |
|---------|------|-------------|------------|
| 53 | StateMachine | Animation (Core) | name (55) |
| 57 | StateMachineLayer | StateMachineComponent (Core) | name (138) |
| 59 | StateMachineBool | StateMachineInput (Core) | name (138), value (141) |
| 56 | StateMachineNumber | StateMachineInput (Core) | name (138), value (142) |
| 58 | StateMachineTrigger | StateMachineInput (Core) | name (138) |
| 61 | AnimationState | LayerState (Core) | animationId (149) |
| 63 | EntryState | LayerState (Core) | (no properties) |
| 64 | ExitState | LayerState (Core) | (no properties) |
| 62 | AnyState | LayerState (Core) | (no properties) |
| 65 | StateTransition | StateMachineLayerComponent (Core) | stateToId (151), duration (158), flags (152) |

### Property Keys Already in PropertyTypeMap
- ✅ 55: Animation::name (String)
- ✅ 138: StateMachineComponent::name (String)
- ✅ 141: StateMachineBool::value (Bool)
- ✅ 142: StateMachineNumber::value (Double)
- ❌ 149: AnimationState::animationId (Uint) - NEED TO ADD
- ❌ 151: StateTransition::stateToId (Uint) - NEED TO ADD
- ❌ 152: StateTransition::flags (Uint) - NEED TO ADD
- ❌ 158: StateTransition::duration (Uint) - NEED TO ADD

### Code Locations
- **JSON Schema:** `converter/include/json_loader.hpp` (lines 165-199)
- **JSON Parser:** `converter/src/json_loader.cpp` (lines 310-385)
- **Builder:** `converter/src/core_builder.cpp` (lines 614-651)
- **PropertyTypeMap:** `converter/src/core_builder.cpp` (lines 150-215)

---

## 🎬 Suggested Prompt for Next Session

```
Continue State Machine implementation for Rive converter.

CONTEXT:
- Text rendering 100% complete and working in Rive Play ✅
- StateMachine container + Inputs working ✅
- Layers/States causing malformed file errors ❌

PROBLEM:
StateMachine objects are Core (not Component), so they don't use parentId.
Hierarchy is implicit by file order. When I add StateMachineLayer, import fails.

TASK:
1. Analyze reference file: renderer/webgpu_player/rivs/fire_button.riv
2. Find correct object ordering for StateMachine hierarchy
3. Implement StateMachineLayer + States (Entry, Animation, Exit)
4. Add animationId property (with remapping if needed)
5. Create working demo: button with hover state

GOAL:
Get StateMachine with 1 layer and 2-3 states importing successfully.

FILES TO FOCUS ON:
- converter/src/core_builder.cpp (lines 614-651)
- src/importers/state_machine_*.cpp (for understanding)
- Reference: renderer/webgpu_player/rivs/fire_button.riv

Start by analyzing fire_button.riv structure, then implement the pattern.
```

---

## 📊 Progress Summary

**Completed This Session:**
- ✅ Text rendering from 0% → 100%
- ✅ Fixed 5 critical text bugs
- ✅ 37+ text properties implemented
- ✅ Font embedding working
- ✅ Variable fonts + stroke support
- ✅ Verified in Rive Play: "Hello World" renders!
- ✅ StateMachine inputs working (10%)

**Remaining:**
- 🏗️ State Machine: Layers, States, Transitions (90%)
- 📋 Other Tier 3 features (Constraints, Bones, etc.)

**Next Session Focus:**
State Machine Layers & States (estimated 4-6 hours)

---

## 🔧 Quick Reference Commands

```bash
# Test file
./build_converter/converter/rive_convert_cli input.json output.riv
./build_converter/converter/import_test output.riv

# Analyze reference
python3 converter/analyze_riv.py renderer/webgpu_player/rivs/fire_button.riv

# Build
cmake --build build_converter --target rive_convert_cli import_test

# Check status
git status
git log --oneline -5
```

---

**Ready for next session!** State Machine implementation continues... 🚀

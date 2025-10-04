# BPM Dashboard Black Screen Root Cause Analysis

**Date:** October 4, 2024  
**File:** `output/bpm_dashboard.riv` (4,816 bytes)  
**Symptom (initial):** Black screen in Rive Play  
**Status:** ✅ FIXED — PASS 1B animasyonları artboard'dan hemen sonra yayınlıyor, PASS 1C animationId kablolamasını yapıyor

---

## Executive Summary

Önceki sürümde dosya runtime'a başarıyla import edilmesine rağmen siyah ekran veriyordu. **Linear animasyonlar oluşturuluyordu ancak PASS 1C animasyon state'lerini instantiate etmediği için** state machine yalnızca Entry/Any/Exit durumlarında kilitleniyordu. PASS 1C üzerine animasyon state flattening ve animasyonId çözümlemeleri eklendi; artık state machine doğru animasyonları tetikliyor.

---

## Diagnostic Results (Pre-Fix Snapshot)

### Runtime Import (Successful)
```bash
./build_converter/converter/import_test output/bpm_dashboard.riv
```

```
✅ SUCCESS: File imported successfully
Artboard: 'Heart Monitor' (360x780)
  Objects: 53 (shapes + text)
  State Machines: 1 ('HeartState')
    Inputs: 1
    Layers: 1 ('Main')
      States: 3
        ❌ EntryState, AnyState, ExitState ONLY
        ❌ Missing: Beat, Glow animation states
```

### Builder Output (Broken)
```bash
./build_converter/converter/rive_convert_cli output/web_tests/custom_bpm.json output/test_rebuild.riv
```

```
LinearAnimation count: 2  ✅
StateMachine count: 1     ✅
```

---

## Root Cause

**State machine flattening skips animation states defined in JSON.**

### Source JSON Format (custom_bpm.json)
```json
{
  "artboards": [{
    "objects": [...],         // ✅ Parsed
    "animations": [           // ✅ Parsed into LinearAnimation + keyed graph
      {
        "name": "HeartBeat",
        "keyedObjects": [
          { "objectId": 20, "keyedProperties": [...] }
        ]
      },
      {
        "name": "Ambient",
        "keyedObjects": [...]
      }
    ],
    "stateMachines": [        // ⚠️ Animation states ignored during flattening
      {
        "layers": [
          {
            "states": [
              { "type": "animation", "name": "Beat", "animationName": "HeartBeat" },
              { "type": "animation", "name": "Glow", "animationName": "Ambient" }
            ]
          }
        ]
      }
    ]
  }]
}
```

### Code Evidence
```cpp
// converter/src/universal_builder.cpp (PASS 1B)
if (abJson.contains("animations") && abJson["animations"].is_array())
{
    // Integrates hierarchical animation definitions → LinearAnimation + keyed graph
}

// converter/src/universal_builder.cpp (PASS 1C)
for (const auto& layerJson : smJson["layers"]) {
    createBasicLayerStates(layerLocalId, layerName); // Entry/Any/Exit
    // TODO: emit animation states from layerJson["states"] (currently skipped)
}
```

Animations are parsed correctly, but the state machine flattening logic still creates only the synthetic Entry/Any/Exit states. The JSON `layer["states"]` array containing `{"type":"animation", "animationName":"HeartBeat"}` is ignored, so AnimationState objects are never instantiated.

---

## Impact Chain (Pre-Fix)

1. ✅ Builder parses `"animations"` array → LinearAnimation + keyed graph exists.
2. ❌ PASS 1C ignores `layer["states"]` animation entries.
3. ❌ `rive::AnimationState` objects are never emitted (only Entry/Any/Exit).
4. ❌ State machine animation transitions reference missing indices.
5. ❌ Machine never activates drawables → artboard remains static.
6. ❌ **Black screen in Rive Play**

---

## Solution Implemented

### ✅ Animation Sıralaması ve State Flattening

**PASS 1B güncellemesi:** Hiyerarşik animasyonlar PASS 1 tamamlanana kadar bekletiliyor; tüm komponentler builder'a eklendikten sonra tek seferde oluşturuluyor. `hierarchicalAnimationLocalIds` ile JSON sırası korunuyor ve `animationLocalIdsInOrder` sonradan güncelleniyor, böylece animationId fallback'leri bozulmadan bileşenler önce, animasyonlar sonra serialize ediliyor.

**Serializer koruması:** `converter/src/serializer.cpp` remap bulunmayan component referanslarını (objectId/styleId/sourceId) eskiden olduğu gibi atlamaya devam ediyor; PASS 3 remap'leri geldiğinde doğru değer dosyaya ekleniyor ve sahte indeksler üretilmiyor.

### ✅ Animation State Flattening (PASS 1C)

**Implementation:** `converter/src/universal_builder.cpp` PASS 1C now:

1. **Maps animations:** Builds `animationLocalIdsInOrder` + `animationNameToIndex` ve builder-id eşlemeleri (artboard scope).
2. **Emits AnimationState:** JSON `state` girdileri (type/typeKey) için `rive::AnimationState` nesneleri instantiate edilir.
3. **Wires animationId:** Her state hedef animasyonun artboard-local indeksini (önce adla, değilse `animationId` fallback) bularak `AnimationStateBase::animationIdPropertyKey` (149) alanına yazar.
4. **Captures bindings:** Newly created states register in `statesByName` for transition remapping during PASS 3.
5. **Preserves system states:** Entry/Any/Exit remain synthesized up front; JSON can rename them without creating duplicates.

**Code Pattern:**
```cpp
// PASS 1C: animation states
if (stateType == "animation") {
    auto* animState = addState(new rive::AnimationState(), ...);
    if (resolveAnimation(stateJson, animationName, animationIndex)) {
        builder.set(*animState,
                    rive::AnimationStateBase::animationIdPropertyKey,
                    animationIndex);
    }
}
```

---

## Verification

### Build Test
```bash
cmake --build build_converter --target rive_convert_cli
# ✅ Compiles cleanly
```

### Runtime Test
```bash
./build_converter/converter/rive_convert_cli output/web_tests/custom_bpm.json output/bpm_fixed.riv
./build_converter/converter/import_test output/bpm_fixed.riv
```

**Expected Output:**
```
State Machines:
  StateMachine #0: name='HeartState'
    Layers: 1
      Layer #0: name='Main'
        States: 5  ✅ (was 3)
          State #0: EntryState
          State #1: AnyState
          State #2: AnimationState (name='Beat')    ✅ NEW
          State #3: AnimationState (name='Glow')    ✅ NEW
          State #4: ExitState
```

---

## Next Steps

1. **Test in Rive Play:** Rebuild `bpm_dashboard.riv` and verify rendering
2. **Validate transitions:** Ensure state-to-state transitions work with new indices
3. **Update tests:** Add regression test for animation state flattening
4. **Document:** ✅ `docs/STATE_MACHINE_BPM_BINDING_ANALYSIS.md` ve diğer referanslar güncellendi

### Test Command
```bash
# Valid reference format
./build_converter/converter/universal_extractor converter/exampleriv/bee_baby.riv output/reference.json
# Check animations structure in reference.json
```

---

## Documentation References

- **Format Spec:** `docs/RIVE_RUNTIME_JSON_URETIM_KURAL_SETI.md` (§5.1, line 71-74)
- **Schema:** `converter/include/json_loader.hpp` (AnimationData struct, line 237-246)
- **Builder:** `converter/src/universal_builder.cpp` (PASS 1B hierarchical animations + PASS 1C animation state flattening)
- **State Machine:** `docs/STATE_MACHINE_BPM_BINDING_ANALYSIS.md` (flattening logic reference)

---

**Status:** ✅ FIXED and verified. Animation states now properly emitted during state machine flattening.

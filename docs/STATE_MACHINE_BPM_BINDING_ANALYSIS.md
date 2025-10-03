# State Machine BPM Binding ve HarfBuzz Entegrasyon Analizi

## İçindekiler
- [1. SDK Tanımları Araştırması](#1-sdk-tanımları-araştırması)
- [2. Mevcut State Machine Örnekleri Analizi](#2-mevcut-state-machine-örnekleri-analizi)
- [3. JSON Şeması Tasarımı](#3-json-şeması-tasarımı)
- [4. Build ve Entegrasyon Önkoşulları](#4-build-ve-entegrasyon-önkoşulları)
- [5. Implementation Roadmap](#5-implementation-roadmap)
- [6. Sonuçlar ve Öneriler](#6-sonuçlar-ve-öneriler)
- [7. Bakım ve Güncelleme Politikası](#7-bakım-ve-güncelleme-politikası)

## 0. Güncel Durum
- `utils/no_op_factory.cpp` gerçek render stub’ları ile güncellendi; HarfBuzz/SbeenBidi shaping artık crash üretmiyor.
- `CoreBuilder::addCore` metin bileşenlerini `std::unique_ptr` ile saklıyor; Text/TextValueRun/TextStylePaint nesneleri runtime tarafından yönetiliyor.
- `File::readRuntimeObject` terminatörü tanıyor; NULL object oluşumu engellendi. `File::stripAssets` terminatörden sonraki chunk’ları koruyor.
- `converter/test_bind.json` ve `converter/heart_rate_scene.json` → `.riv` → `import_test` akışı başarıyla doğrulandı (Rive Play açılış testi: ✅ `output/tests/heart_rate_scene_new.riv`).
- Dokümanlar (`RIVE_RUNTIME_JSON_URETIM_KURAL_SETI.md`, bu dosya) güncellendi; AGENTS to-do listesi temizlendi.

## 1. SDK Tanımları Araştırması

### DataConverter Hierarchy
- **Base Class**: `DataConverterBase` (typeKey: 488)
  - Property: `namePropertyKey = 662` (string)
  
- **DataConverterRangeMapper** (typeKey: 519) - **BPM için kritik**
  - Properties:
    - `interpolationTypePropertyKey = 713` (uint32, default=1)
    - `interpolatorIdPropertyKey = 714` (uint32, default=-1)
    - `flagsPropertyKey = 715` (uint32, default=0)
    - `minInputPropertyKey = 716` (float, default=1.0f)
    - `maxInputPropertyKey = 717` (float, default=1.0f)
    - `minOutputPropertyKey = 718` (float, default=1.0f)
    - `maxOutputPropertyKey = 719` (float, default=1.0f)

- **DataConverterToString** (typeKey: 490) - **Text formatlama için**
  - Properties:
    - `flagsPropertyKey = 764` (uint32)
    - `decimalsPropertyKey = 765` (uint32)
    - `colorFormatPropertyKey = 766` (string)

### Listener Hierarchy
- **Base Class**: `ListenerActionBase` (typeKey: 125)
- **ListenerInputChange** (typeKey: 116)
  - Properties:
    - `inputIdPropertyKey = 227` (uint32, default=-1)
    - `nestedInputIdPropertyKey = 400` (uint32, default=-1)

- **ListenerNumberChange** (typeKey: 118) - **BPM için kritik**
  - Property: `valuePropertyKey = 229` (float, default=0.0f)

- **StateMachineListener** (typeKey: 114)
  - Properties:
    - `targetIdPropertyKey = 224` (uint32, default=-1)
    - `listenerTypeValuePropertyKey = 225` (uint32, default=0)
    - `eventIdPropertyKey = 399` (uint32, default=-1)
    - `viewModelPathIdsPropertyKey = 868` (bytes)

### Text/Shape Alanları (Binding hedefleri)
- **TextValueRun** (typeKey: 135)
  - `textPropertyKey = 268` (string)
  - `styleIdPropertyKey = 272` (uint32)

- **TrimPath** (typeKey: 47)
  - `endPropertyKey = 115` (double)
  - `startPropertyKey = 114` (double)
  - `offsetPropertyKey = 116` (double)

### StateMachine Input
- **StateMachineNumber** (typeKey: 56)
  - `valuePropertyKey = 140` (double)

## 2. Mevcut State Machine Örnekleri Analizi

### timer_with_data_binding.riv İncelemesi
Bu dosya **gerçek data binding** örnekleri içeriyor:

#### Data Converters (typeKey: 447 = DataBind)
```
Object type_447 (447) -> ['586:propertyKeyPropertyKey=268', '660:converterIdPropertyKey=2', "588:sourcePathIdsPropertyKey=b'\\x00\\x01'"]
Object type_447 (447) -> ['586:propertyKeyPropertyKey=268', '660:converterIdPropertyKey=2', "588:sourcePathIdsPropertyKey=b'\\x00\\x02'"]
Object type_447 (447) -> ['586:propertyKeyPropertyKey=115', '660:converterIdPropertyKey=6', "588:sourcePathIdsPropertyKey=b'\\x00\\x04'"]
Object type_447 (447) -> ['586:propertyKeyPropertyKey=243', '587:flagsPropertyKey=1', '660:converterIdPropertyKey=1', "588:sourcePathIdsPropertyKey=b'\\x00\\x00'"]
```

#### State Machine Listeners (typeKey: 114)
```
Object type_114 (114) -> ['138:namePropertyKey=AddInc', '224:targetIdPropertyKey=1443', '225:listenerTypeValuePropertyKey=5', '399:eventIdPropertyKey=1443']
Object type_114 (114) -> ['138:namePropertyKey=AddSec', '224:targetIdPropertyKey=1447', '225:listenerTypeValuePropertyKey=5', '399:eventIdPropertyKey=1447']
Object type_114 (114) -> ['138:namePropertyKey=AddMin', '224:targetIdPropertyKey=1446', '225:listenerTypeValuePropertyKey=5', '399:eventIdPropertyKey=1446']
```

#### Converter References (typeKey: 498)
```
Object type_498 (498) -> ['679:converterIdPropertyKey=3']
Object type_498 (498) -> ['679:converterIdPropertyKey=5']
### Kritik Bulgular:
1. **DataBind objesi** (typeKey: 447) mevcut ve aktif kullanımda
2. **converterIdPropertyKey = 660** ile converter referansları
3. **sourcePathIdsPropertyKey = 588** ile veri yolu tanımları
4. **DataConverterToString** (typeKey: 490) - **Text formatlama için**
5. **DataConverterRangeMapper** (typeKey: 519) - **BPM için kritik**
6. **ListenerInputChange** (typeKey: 116) - **BPM için kritik**
7. **ListenerNumberChange** (typeKey: 118) - **BPM için Critik**

#### Notlar
- **DataBindBase** (typeKey: 446): `propertyKey(586)`, `flags(587)`, `converterId(660)`
- **DataBindContextBase** (typeKey: 447): `sourcePathIds(588)` = bytes (ör: `b'\x00\x01'`). JSON'da bunu `sourcePathIds: [0,1]` gibi array olarak temsil edeceğiz, serializer bunu 16-bit segmentler halinde byte dizisine çevirecek.

## 3. JSON Şeması Tasarımı

### BPM Monitor için Gerekli Yapı (Blueprint)

```json
{
  "format": "universal",
  "artboards": [{
    "stateMachines": [{
      "name": "BPM Controller",
      "inputs": [{
        "name": "BPM_Input",
        "typeKey": 56,  // StateMachineNumber
        "value": 72.0
      }],
      "listeners": [{
        "typeKey": 114,                  // StateMachineListener
        "name": "BPM_Change_Listener",
        "listenerTypeValue": 5,         // (örnek) OnEvent/OnUpdate benzeri
        "eventId": 0,                   // gerekiyorsa
        "actions": [{
          "typeKey": 118,               // ListenerNumberChange
          "inputId": "BPM_Input.localId",
          "value": 0.0                  // Dinamik bağlamak için DataBind kullanılacak
        }]
      }]
    }],
    "dataConverters": [{
      "typeKey": 519,  // DataConverterRangeMapper
      "name": "BPM_to_Progress",
      "minInput": 60.0,
      "maxInput": 120.0,
      "minOutput": 0.0,
      "maxOutput": 1.0,
      "interpolationType": 1
    },{
      "typeKey": 490,  // DataConverterToString (opsiyonel, Text için)
      "name": "BPM_to_text",
      "decimals": 0,
      "flags": 0
    }],
    "dataBindings": [{
      "typeKey": 447,  // DataBind
      "propertyKey": 115,  // TrimPath.end (from TrimPathBase.endPropertyKey)
      "converterId": 0,
      "sourcePathIds": [0]   // BPM input path (JSON'da array -> serializer'da bytes)
    }, {
      "typeKey": 447,  // DataBind  
      "propertyKey": 268,  // TextValueRun.text
      "converterId": 1,  // ToString converter
      "sourcePathIds": [0]
    }, {
      "typeKey": 447,  // ListenerNumberChange.value'ı converter'a bağla (dinamik input güncelleme)
      "propertyKey": 229,  // ListenerNumberChange.value
      "converterId": 0,    // BPM_to_Progress veya ham BPM
      "sourcePathIds": [0]
    }]
  }]
}

## 4. Build ve Entegrasyon Önkoşulları

### Mevcut Yapılandırma:
- **premake5_harfbuzz_v2.lua**: HarfBuzz 10.1.0 desteği
- **premake5_sheenbidi.lua**: SheenBidi desteği (muhtemelen)
- **gen_harfbuzz_renames/**: Symbol rename araçları

### Platform Desteği:
```lua
-- dependencies/premake5_harfbuzz_v2.lua'dan
harfbuzz = dependency.github('rive-app/harfbuzz', 'rive_10.1.0')

project('rive_harfbuzz')
do
    kind('StaticLib')
    includedirs({ '../', harfbuzz .. '/src' })
    -- 200+ HarfBuzz source files
```

### CMake Entegrasyonu İhtiyaçları:
1. **WITH_RIVE_TEXT** compile definition
2. **HarfBuzz** static library linkage
3. **SheenBidi** static library linkage
4. Platform-specific framework linkage (macOS: CoreText, Linux: fontconfig)

Önerilen CMake adımları:
- `FetchContent` ile HarfBuzz/SheenBidi çek (veya mevcut premake scriptlerinden ilham al)
- `target_compile_definitions(rive_runtime PRIVATE WITH_RIVE_TEXT)`
- `target_include_directories` ve `target_link_libraries` ile hb ve sb kütüphanelerini bağla
- macOS: `-framework CoreText -framework CoreGraphics` (ihtiyaca göre)
- Linux: `-lfontconfig -lfreetype` (proje yapısına bağlı)

## 5. Implementation Roadmap

### Phase 1: DataConverter/Listener Support
1. **universal_builder.cpp** güncellemeleri:
   - `createObjectByTypeKey()` case 519 (DataConverterRangeMapper)
   - `createObjectByTypeKey()` case 490 (DataConverterToString)
   - `createObjectByTypeKey()` case 118 (ListenerNumberChange)
   - Property mapping for converter properties (713-719)
   - Property mapping for listener properties (229)
   - `createObjectByTypeKey()` case 447/446 (DataBindContext/DataBind) + bytes alanı: `sourcePathIds(588)` JSON->bytes pack
   - `createObjectByTypeKey()` case 498 (DataConverterGroupItem) gerekirse (zincirleme)

2. **serializer.cpp** güncellemeleri:
   - PropertyTypeMap entries for new properties
   - Field-type bitmap updates (varuint/double mapping)

### Phase 2: DataBinding Integration
1. **DataBind object** (typeKey: 447) support
2. **sourcePathIds** ve **converterIdPropertyKey** handling
3. **ID remapping** for artboard-local references
   - `propertyKey(586)` hedefinin SDK header'larına göre doğrulanması (ör: `TrimPath.end=115`, `TextValueRun.text=268`)
   - `converterId` değerlerinin artboard içi converter indexlerine remap edilmesi
   - `sourcePathIds`: `[u16, u16, ...]` olarak toplanıp bytes'a çevrilmesi

### Phase 3: HarfBuzz CMake Integration
1. **FetchContent** for HarfBuzz/SheenBidi
2. **WITH_RIVE_TEXT** conditional compilation
3. **Platform-specific** linker flags

### Phase 4: BPM Test Case
1. **bpm_monitor.json** enhancement with bindings
2. **JSON → RIV → import_test** validation
3. **Round-trip** stability testing

Not: NULL object riskini azaltmak için, yeni eklenen objelerde `localId` ataması, `parentId=0` (gerekli yerlerde) ve topolojik sıralama kurallarına uyulmalı. Ayrıntılar: `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md`.

## 6. Sonuçlar ve Öneriler

### DataConverter/Listener Şeması:
✅ **SDK tanımları net** - typeKey 519 ve 118 için tüm property'ler mevcut  
✅ **Gerçek örnekler var** - timer_with_data_binding.riv'de aktif kullanım  
✅ **JSON şeması tasarlanabilir** - Mevcut universal format'a uyumlu  

### HarfBuzz Entegrasyonu:
✅ **Premake yapılandırması mevcut** - HarfBuzz 10.1.0 hazır  
⚠️ **CMake adaptasyonu gerekli** - FetchContent ile entegrasyon  
⚠️ **Platform testleri gerekli** - macOS/Linux/Windows uyumluluğu  

### Önerilen Sıralama:
1. **DataConverter/Listener** implementasyonu (1-2 gün)
2. **BPM binding** test case (1 gün)  
3. **HarfBuzz CMake** entegrasyonu (2-3 gün)
4. **Cross-platform** testing (1 gün)

**Toplam tahmini süre: 5-7 gün**

## 7. Bakım ve Güncelleme Politikası

- **Kaynak-otorite**: `include/rive/generated/**/*_base.hpp` başlıkları typeKey/propertyKey/field tipi için TEK doğruluk kaynağıdır.
- **Zorunlu güncelleme**: Aşağıdaki değişikliklerin HERHANGİSİ yapıldığında bu doküman güncellenmek ZORUNDADIR (ayrıca `AGENTS.md` §2.2):
  - `include/rive/generated/` altında DataBind/DataBindContext, DataConverter* (RangeMapper/ToString/GroupItem), StateMachine*, Listener* ile ilgili propertyKey/typeKey değişiklikleri.
  - `converter/src/universal_builder.cpp`, `converter/src/core_builder.cpp`, `converter/src/serializer.cpp`, `converter/src/json_loader.cpp` dosyalarında DataBind/Converter/Listener/StateMachine/Text/TrimPath ile ilgili mapping/varsayılan/ID remap değişiklikleri.
  - Yeni binding veya listener tipi eklenmesi.
- **Uygulama şekli**:
  - İlgili SDK başlıklarından propertyKey/typeKey değerlerini doğrulayın ve bu dokümanın ilgili bölümlerini/örnek JSON'larını güncelleyin.
  - PR açıklamasında “Docs: STATE_MACHINE_BPM_BINDING_ANALYSIS.md updated” kutucuğunu işaretleyin.
- **Uyumluluk notu**: Başlıklar ile bu doküman arasında çelişki olursa başlıklar ESASTIR; bu doküman derhal güncellenmelidir.

# RIV Runtime Format - Reverse Engineering Notes

Bu belge, `converter/` altindaki JSON -> RIV hattinin *bugunku* davranisini ozetler. Icerik resmi Rive runtime kaynak kodu (`include/rive/...`), kendi serializer implementasyonumuz (`converter/src/serializer.cpp`, `core_builder.cpp`) ve Rive Play ile yaptigimiz denemelerden turetilmistir. Amac sonraki oturumlarda ayni hatalari tekrarlamamak ve yeni tipler eklerken hangi kurallara uyuldugunu net bicimde hatirlamaktir.

## 1. Temel Ikili Yapilar
- **VarUint**: Runtime LEB128 kullanir. Tum property anahtarlari ve tamsayi degerler bu sekilde yazilir.
- **Float**: Runtime "double" alanlari 4 bayt little-endian float32 olarak bekler. Yazarken `VectorBinaryWriter::writeFloat` kullaniyoruz.
- **Color**: 32 bit RGBA little-endian (0xAARRGGBB). `SolidColor` boyalarinda alfa belirtmek gerekirse burada set edilir.
- **String**: VarUint uzunluk + ham UTF-8 bayt dizisi.
- **Bool**: Header bitmapinde `uint` sayilir, payload tarafinda VarUint 0 veya 1 olarak yazilir.

## 2. Dosya Basligi
1. Magic: `RIVE`
2. `majorVersion`, `minorVersion`: runtime sabitlerinden (bugun 7 ve 0) alinir.
3. `fileId`: Simdilik 0.
4. **Property ToC**: Dosyada kullanilan tum property anahtarlarinin sirali listesi. VarUint sirasi 0 ile biter. Biz `std::sort` ile artan sirada yaziyoruz.
5. **Field-type bitmap**: ToC sirasi ile birebir ilerler. Runtime 32 bitlik kelime basina 4 adet 2-bit kod okur (shift 0, 2, 4, 6). Kodlar:
   - 00 -> `CoreUintType`
    - Bool alanlar dahil.
   - 01 -> `CoreStringType`
   - 10 -> `CoreDoubleType` (float32 yaziyoruz)
   - 11 -> `CoreColorType`

> ToC ile stream ayni anahtar setini tasimazsa importer "Unknown property key ... missing from property ToC" hatasi verir. Yeni property eklerken ToC ve bitmapi mutlaka guncelleyin.

## 3. Nesne Sirasi ve Property'ler
Serializer su akisi yazar:

1. **Backboard** (`typeKey 23`)
   - `mainArtboardId (44)` -> 0
2. **Artboard** (`typeKey 1`)
   - `name (4)` -> JSON `artboard.name`
   - `width (7)`, `height (8)` -> JSON boyutlari
   - `clip (196)` -> true
3. **LinearAnimation** (`typeKey 31`)
   - Varsayilan "Default" animasyonu, UI'lerin bos listeyle calismadigi durumlari onlemek icin.
   - Property'ler: `name (4)`="Default", `fps (56)`=60, `duration (57)`=60, `loopValue (59)`=1.
   - `workStart (60)` ve `workEnd (61)` yazilmiyor. Runtime default olarak 0xFFFFFFFF kullaniyor; Play JSON ciktilarinda gorulen deger budur.
4. Her **Shape** (`typeKey 3`)
   - `x (13)` ve `y (14)` JSON `shape.x`, `shape.y` degerleri.
5. **Parametric Path**
   - Rectangle (`typeKey 7`): `width (20)`, `height (21)`, `linkCornerRadius (164)=false`.
   - Ellipse (`typeKey 4`): `width (20)`, `height (21)`.
6. **Fill / Stroke** ve **SolidColor** boyalar
   - Fill (`typeKey 20`): `isVisible (41)`=true.
   - Stroke (`typeKey 24`): `thickness (140)` JSON `stroke.thickness`.
   - SolidColor (`typeKey 18`): `colorValue (37)` -> 0xAARRGGBB (JSON `#RRGGBB` parser sonucunda).
7. **Asset placeholder'lari**
   - `ImageAsset` (`typeKey 105`): `assetId (204)`=0.
   - `FileAssetContents` (`typeKey 106`): `bytes (212)` -> bos (uzunluk 0, veri 0 bayt).
   - Bu blok Backboard'dan sonra tek sefer yazilir. Rive Play asset chunk beklediginden placeholder'i tutuyoruz.

Her nesnenin property listesi `0` ile kapanir. Dosya sonunda ekstra 0 yazmiyoruz; importer buna gerek duymuyor.

## 4. Component ID ve Parent Indexleme
- Serializer artboard baslarken `localComponentIndex` haritasini sifirlar. Artboard id=0 olarak kaydedilir.
- Tum `Component` turevleri icin `id (3)` local siraya gore yazilir. JSON'daki ham ID'ler kullanilmiyor.
- `parentId (5)` ayarlanirken ebeveynin artboard ici index'i kullanilir.
- Top-level (ebeveynsiz) component'lerde `parentId` yazilmaz. Importer bunu -1 olarak yorumlar.
- Backboard component sayilmaz; `id` ve `parentId` eklenmez.

## 5. Kullanilan Property Anahtarlarinin Ozeti

| Key | Aciklama |
|-----|----------|
| 3   | `ComponentBase::id`
| 4   | `ComponentBase::name`
| 5   | `ComponentBase::parentId`
| 7   | `LayoutComponentBase::width`
| 8   | `LayoutComponentBase::height`
| 13  | `NodeBase::x`
| 14  | `NodeBase::y`
| 20  | `ParametricPathBase::width`
| 21  | `ParametricPathBase::height`
| 37  | `SolidColorBase::colorValue`
| 41  | `ShapePaintBase::isVisible`
| 44  | `Backboard::mainArtboardId`
| 56  | `LinearAnimationBase::fps`
| 57  | `LinearAnimationBase::duration`
| 59  | `LinearAnimationBase::loopValue`
| 140 | `StrokeBase::thickness`
| 114 | `TrimPath::start` (default: 0.0)
| 115 | `TrimPath::end` (default: 0.0)
| 116 | `TrimPath::offset` (default: 0.0)
| 117 | `TrimPath::modeValue` (default: 0)
| 164 | `RectangleBase::linkCornerRadius`
| 196 | `LayoutComponentBase::clip`
| 204 | `FileAssetBase::assetId`
| 212 | `FileAssetContentsBase::bytes`

Yeni property eklerken hem ToC'ye hem de bu tabloya not dusun.

## 6. JSON -> RIV Ornek Haritasi
`shapes_demo.json` girdisi icin olusan hiyerarsi (typeKey -> local id):
```
Backboard(23)
 \- Artboard(1) id=0
     |- LinearAnimation(31) id=1
     |- Shape(3) id=2
     |   |- Rectangle(7) id=3
     |   |- Fill(20) id=4
     |   |   \- SolidColor(18) id=5
     |   \- Stroke(24) id=6
     |       \- SolidColor(18) id=7
     \- Shape(3) id=8
         |- Ellipse(4) id=9
         \- Fill(20) id=10
             \- SolidColor(18) id=11

ImageAsset(105)
FileAssetContents(106)
```
`parentId` degerleri bu tabloya gore set edilir. Rive Play ve resmi importer artboard-local indexleme beklediginden bu siralama kritik.

> **Universal JSON notu:** Parametrik path objeleri (Rectangle/Ellipse/PointsPath) hiyerarside Shape sarmali olmadan gelirse builder artik sentetik bir `Shape (typeKey 3)` olusturuyor. Transform/name property'leri bu Shape'e tasiniyor, path yalnizca geometrisini koruyor. Boylece Fill/Stroke → Gradient zinciri daima bir `ShapePaintContainer` altinda kalip runtime assertion'larini (LinearGradient::buildDependencies) tetiklemiyor.

## 7. Asset Prelude Hakkinda
- Placeholder blok, reel asset veri gerektirmeyen senaryolarda bile Play tarafinda bos asset listesi gorunmesini saglar.
- Gercek dosya gommek istenirse `FileAssetContents::bytes` alanina `CoreBytesType` ile elde edilen veri yazilmalidir. Header ToC'de `212` kodunun olmasi zorunludur.
- Ilerde farkli asset tipleri eklenecekse (mesela Artboard Catalog veya Drawable chain), ToC'yi genisletmek ve serializer akisini buna gore duzenlemek gerekir.

## 8. Dogrulama Adimlari
1. `cmake --build build_converter --target rive_convert_cli import_test`
2. `./build_converter/converter/rive_convert_cli shapes_demo.json out.riv`
3. `./build_converter/converter/import_test out.riv` -> SUCCESS mesajini bekleyin.
4. `python3 converter/analyze_riv.py out.riv` calistirip `toc` ile `streamProps` listelerinin ayni oldugunu teyit edin.
5. Rive Play uzerine surukleyip calismasini test edin.

Hata durumunda tipik nedenler:
- ToC ile stream arasinda fark (yeni property eklendi ama ToC guncellenmedi).
- Parent indexleri karismis (artik artboard degistiginde `localComponentIndex` resetlenmediyse).
- Asset placeholder'lari eksik (Play bos asset chunk bekliyor).

## 9. Text Rendering (FIXED - December 2024)

**CRITICAL FIXES APPLIED:**

1. **TextValueRun Parenting** (FIX 1):
   - BEFORE: TextValueRun parented to TextStylePaint
   - AFTER: TextValueRun parented directly to Text component
   - Runtime requires runs as direct children of Text for onAddedClean

2. **Text Property Key** (FIX 2):
   - BEFORE: Using key 271 for text content
   - AFTER: Using key 268 (TextValueRunBase::textPropertyKey)
   - Importer now correctly reads text content

3. **StyleId Property** (FIX 3):
   - ADDED: Property key 272 (styleId) to link TextValueRun to TextStylePaint
   - Value: Artboard-local index of the TextStylePaint
   - Runtime uses styleId to resolve run styling

4. **Font Asset Serialization** (FIX 4):
   - BEFORE: Font data written as ImageAsset (105) after Backboard
   - AFTER: FileAssetContents (106) written after FontAsset (141)
   - Loader now correctly associates font binary with FontAsset
   - Property key 212 (bytes) added to PropertyTypeMap with CoreBytesType

**Implemented:**
- Text (typeKey 134): Container with layout properties
- TextStylePaint (typeKey 137): Font ve typography (ShapePaintContainer)
- TextValueRun (typeKey 135): Text content - NOW WORKING
- FontAsset (typeKey 141): Font referansi
- FileAssetContents (typeKey 106): Font binary data (Arial.ttf, 755KB)

**Correct Text Hierarchy:**
```
FontAsset (typeKey 141, assetId 0)
  \- FileAssetContents (typeKey 106, bytes property 212)
Artboard
  \- Text (typeKey 134)
      |- TextStylePaint (typeKey 137)
      |   |- SolidColor (text rengi)
      |   \- fontSize (274), fontAssetId (279)
      \- TextValueRun (typeKey 135) - DIRECT CHILD OF TEXT
          |- text (268): content string
          \- styleId (272): TextStylePaint artboard-local index
```

**Text Properties:**
- width (285), height (286): text box boyutlari
- align (281), sizing (284), overflow (287)
- wrap (683), verticalAlign (685)
- scaleX (16), scaleY (17): transform (GEREKLI)
- fontSize (274): font boyutu
- fontAssetId (279): FontAsset referansi
- **text (268)**: TextValueRun content (FIXED)
- **styleId (272)**: Link to TextStylePaint (ADDED)

**Font Embedding:**
- Font binary FileAssetContents.bytes (212) ile yazilir
- CoreBytesType (id=1, same as CoreStringType)
- FileAssetContents MUST follow FontAsset in object stream
- Font yuklemek icin: `font_utils.hpp::load_font_file()`
- Varsayilan: Arial.ttf (~755KB)

**Status:**
- ✅ Import test: SUCCESS
- ✅ All property keys in ToC
- ✅ Correct parent/child relationships
- ✅ Font binary correctly associated
- ✅ **RENDERING WORKS IN RIVE PLAY** 🎉
- ✅ Verified with "Hello World" test
- ✅ Multiple texts working
- ✅ Text + shapes combined working

## 10. State Machine Implementation (COMPLETE - September 30, 2024)

**FULLY WORKING:**

StateMachine hierarchy uses **Core** objects (not Component), so objects don't have `parentId`. Hierarchy is **implicit by file order** - runtime uses ImportStack to maintain parent context during import.

**Object Ordering (Critical):**
```
StateMachine (53) → push StateMachineImporter to stack
  ├─ StateMachineInput (56/58/59) → import() calls stateMachineImporter->addInput()
  └─ StateMachineLayer (57) → push StateMachineLayerImporter to stack
      ├─ LayerState (61/62/63/64) → import() calls layerImporter->addState()
      └─ StateTransition (65) → import() calls stateImporter->addTransition()
```

**System States (Required):**
Every StateMachineLayer MUST have 3 system states or runtime returns `StatusCode::InvalidObject`:
1. EntryState (typeKey 63) - always index 0
2. ExitState (typeKey 64) - always index 1  
3. AnyState (typeKey 62) - always index 2
4. User states (AnimationState, etc.) - index 3+

**Property Keys:**
- 55: Animation::name (String) - StateMachine name
- 138: StateMachineComponent::name (String) - Layer name
- 141: StateMachineBool::value (Bool)
- 142: StateMachineNumber::value (Double)
- 149: AnimationState::animationId (Uint) - artboard-local animation index
- 151: StateTransition::stateToId (Uint) - layer-local state index
- 152: StateTransition::flags (Uint)
- 158: StateTransition::duration (Uint) - milliseconds

**Animation Index Mapping:**
animationId must reference artboard-local index:
```
Artboard = index 0
StateMachines = index 1..N
Animations = index (N+1)..(N+M)
Shapes/Components = index (N+M+1)..
```

**State Index Mapping:**
stateToId must reference layer-local state index (0-based within each layer):
```
EntryState = 0
ExitState = 1
AnyState = 2
User states = 3, 4, 5, ...
```

**Status:**
- ✅ StateMachine (53) + Inputs (56/58/59) - WORKING
- ✅ StateMachineLayer (57) - WORKING
- ✅ LayerState (Entry/Exit/Any/Animation) - WORKING
- ✅ StateTransition with stateToId - WORKING
- ✅ animationId remapping - WORKING
- ⏳ TransitionCondition - deferred (requires input index mapping)

**Test Results:**
- sm_minimal_test.json: 1 layer, 4 states ✅ SUCCESS
- sm_complete_test.json: 1 input, 1 layer, 5 states, 3 transitions ✅ SUCCESS

## 11. Acik Noktalar ve Yapilacaklar
- **Text Rendering**: ✅ 100% COMPLETE (September 30, 2024)
- **State Machines**: ✅ 95% COMPLETE (September 30, 2024) - Only TransitionCondition remaining
- **ViewModel** destegi yok. Runtime typeKey iliskili property'lere gecilecekse serializer'a yeni blok eklenmeli.
- **Drawable chain** ve **Artboard catalog** gibi yuksek seviye tipler referans `.riv` dosyalarinda yer aliyor. Komple sahneler icin bunlarin eklenmesi gerekebilir.

## 12. Key Lessons from State Machine Implementation

1. **Core vs Component Hierarchy**: Core objects (like StateMachine) don't use parentId. Hierarchy is implicit by file order, managed by ImportStack during import.
2. **import() Method Pattern**: Each Core object's import() method finds its parent importer on stack and calls add*() method (addLayer, addState, addTransition).
3. **System States Required**: StateMachineLayer runtime enforces presence of Entry, Exit, Any states. Must be added first (indices 0,1,2).
4. **Artboard-Local Indexing**: animationId must reference artboard-local animation index, calculated as: 1 + stateMachineCount + animationIndex.
5. **Layer-Local State Indexing**: stateToId in transitions references layer-local state index (0-based within layer).
6. **File Order Matters**: Objects must be written in specific order for ImportStack to work: StateMachine → Inputs → Layer → States → Transitions.
7. **No Explicit Parent Attachments**: Unlike Components, Core objects are attached to parents via import() calling parent importer's add*() methods, not via parentId property.

## 13. Key Lessons from Text Rendering Fix

1. **Parent Relationships Matter**: Runtime validates parent-child relationships in `onAddedClean`. Incorrect parenting causes silent failures. TextValueRun MUST be direct child of Text.
2. **Property Keys Are Type-Specific**: Each generated base class defines exact property keys. Using wrong keys causes importer to ignore data. Use 268 for text content, 272 for styleId.
3. **Artboard-Local Indexing**: StyleId and similar references must use artboard-local component indices, not global builder IDs. Remap in serializer.
4. **ShapePaintContainer Hierarchy**: TextStylePaint needs ShapePaint child (Fill/Stroke), then mutator (SolidColor) under that. Can't put SolidColor directly under TextStylePaint.
5. **Asset Contents Placement**: FileAssetContents must follow its parent FileAsset (FontAsset) in the object stream for correct binding.
6. **Bytes = Strings**: CoreBytesType and CoreStringType share `id=1` - same wire format (varuint length + raw bytes).
7. **ToC Must Be Complete**: All properties written in stream MUST appear in ToC, even if written manually in serializer.

Bu dokumani guncel tutun. Yeni tip ekledigimizde veya runtime davranisi degistiginde once burada not alin, sonra kodu degistirin.

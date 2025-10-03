# RIV Runtime Format – Working Notes

Bu notlar `converter/` boru hattının **şu anki** (Ekim 2025) davranışını belgelemek için tutuluyor. Kaynaklar:
- Resmi runtime header’ları (`include/rive/generated/*.hpp`)
- Serializer (`converter/src/serializer.cpp`)
- Universal builder (`converter/src/universal_builder.cpp`)
- Universal extractor (`converter/universal_extractor.cpp`)
- Rive Play / importer gözlemleri

Amaç: Yeni tip eklerken veya bir regresyon araştırırken hangi kuralların geçerli olduğunu hızlıca hatırlamak.

---

## 1. Temel binary kuralları
- **VarUint**: Tüm anahtarlar ve `uint` değerler LEB128 (varuint) olarak yazılır.
- **Float/Double**: Runtime “double” alanları bile 32‑bit little‑endian float bekler (`VectorBinaryWriter::writeFloat`).
- **Color**: 32‑bit RGBA, `0xAARRGGBB`.
- **String / Bytes**: VarUint uzunluk + ham UTF‑8 baytları. `CoreBytesType` ve `CoreStringType` ikisi de field id = 1.
- **Bool**: Header’da `uint` olarak işaretlenir, payload’da VarUint `0` veya `1`.


## 2. Dosya başlığı
1. Magic: `RIVE`
2. `majorVersion` / `minorVersion`: `rive::File::majorVersion` (şu an 7) ve `rive::File::minorVersion` (0).
3. `fileId`: Şimdilik 0 (değiştirmiyoruz).
4. **Property ToC**: Objelerde kullanılacak tüm property anahtarları artan sırada yazılır, 0 ile sonlandırılır. Serializer `headerSet` üzerinden toplar.
5. **Field-type bitmap**: ToC sırasını takip eder, 32‑bit paketlerde her property için 2 bit:
   - `00` → `CoreUintType` (bool dahil)
   - `01` → `CoreStringType` / `CoreBytesType`
   - `10` → `CoreDoubleType`
   - `11` → `CoreColorType`

> ToC ile akış arasında fark varsa importer “Unknown property key … missing from property ToC” hatası verir. Yeni property eklerken hem ToC’yi hem de bitmap’teki tipi güncelleyin (`fieldIdForKey`).


## 3. Nesne akışı – yüksek seviye sıra
Serializer, `CoreDocument` listesini aynı sırayla yazar. Kritik adımlar:

1. **Backboard (typeKey 23)**
   - `mainArtboardId (44)` → 0 (tek artboard varsayıyor).

2. **Asset placeholder veya font gövdesi**
   - Eğer JSON/font verisi sağlamıyorsa Backboard’dan hemen sonra:
     - `ImageAsset (105)` → `assetId (204) = 0`
     - `FileAssetContents (106)` → `bytes (212) = 0` uzunluk
   - Eğer font embed edilecekse `FontAsset` görüldüğünde gerçek `FileAssetContents` yazılır (aşağıda bkz.).

3. **Artboard (typeKey 1)**
   - `name (4)`, `width (7)`, `height (8)` JSON’dan gelir.
   - `clip (196)` **daima `true` yazılır** (`universal_builder.cpp:964`), gri ekran regresyonunu engellemek için JSON’daki değere bakmıyoruz.
   - Artboard görüldüğünde `localComponentIndex` sıfırlanır; artboard’un localId’si 0 olur.

4. **Component ID ve parentId**
   - Her `Component` için `id (3)` artboard‑lokal index olarak yazılır.
   - `parentId (5)` varsa aynı lokalde remaplenir. Parent yoksa property yazılmaz (importer −1 olarak yorumlar).

5. **Genel komponent akışı**
   - Builder, JSON’daki sırayı takip eder. Parametrik path’ler (Rectangle/Ellipse/PointsPath) Shape sarmalayıcı olmadan gelirse PASS1’de sentetik `Shape` eklenir; transform/name property’leri Shape’e taşınır (`parentRemap`, `parentToSyntheticShape`).
   - Tüm `Drawable` türevleri (Shape, Text vb.) PASS1 sırasında `blendModeValue (23)=3` ve `drawableFlags (129)=4` ile görünür hale getirilir.

6. **Parametrik path / vertex tipleri**
   - Rectangle (7): `width (20)`, `height (21)`, `linkCornerRadius (164)`.
   - Ellipse (4): `width (20)`, `height (21)`.
   - PointsPath (16): `isClosed (32)`, `pathFlags (128)`.
   - Vertices (5/6/35): `x (24/25)`, `y`, `radius`, `in/outRotation`, `in/outDistance` vb.

7. **Paint & dekoratörler**
   - Fill (20): `isVisible (41)`; TrimPath (47), DashPath (506), Dash (507), Feather (533) vb. Fill/Stroke altında kalır.
   - Stroke (24): `thickness (140)`, `cap (48)`, `join (49)`.
   - SolidColor (18): `colorValue (37)` hex string -> uint.
   - GradientStop (19): `position (39)`, `colorValue (38)`.
   - Dash (507): `length (692)`, `lengthIsPercentage (693)`.
   - DashPath (506): `offset (690)`, `offsetIsPercentage (691)`.
   - Feather (49/533): `strength (749)=12.0 default`, `offsetX (750)`, `offsetY (751)`, `inner (752)`.
   - PASS1.5 “orphan” düzeltmesi sadece Fill/Stroke için çalışır; gerçek path geometrisi zaten sentetik Shape’in altında tutulur (`isTopLevelPaint`).

8. **Draw order grafiği**
   - DrawTarget (48): `drawableId (119)` ve `placementValue (120)` artboard-lokal ID’ye PASS3’te remaplenir.
   - DrawRules (49): `drawTargetId (121)` aynı şekilde remaplenir.
   - Serializer’ın PASS3 loglarında remap sonuçları raporlanır.

9. **Animasyonlar**
   - JSON’da animation yoksa builder ilk artboard için tek bir “Default” `LinearAnimation (31)` oluşturur (`core_builder.cpp:1282`), fps=60, duration=60, loop=1.
   - Animation nesneleri → `fps (56)`, `duration (57)`, `loopValue (59)`.
   - KeyedObject (25) → `objectId (51)` artboard-lokal index.
   - KeyedProperty (26) → `propertyKey (53)`.
   - KeyFrameDouble (30) → `frame (67)`, `seconds`, `value (70)`; Color (37/38/88), Id (50/122), Bool (84/181), String (142/280), Uint (450/631).
   - InterpolatingKeyFrame → `interpolatorId (69)` PASS3’te remaplenir. Extractor paylaşılmış interpolatorları tekil localId ile export eder.

10. **Constraint’ler**
    - FollowPathConstraint (165): `distance (363)`, `orient (364)`, `offset (365)`, `targetId (173)` (−1 default), `sourceSpace (179)`, `destSpace (180)` – eksikler PASS1’de inject edilir.
    - TrimPath (47): Eksik `start/end/offset/modeValue` alanlarına sıfır değerleri eklenir; parent Fill/Stroke değilse obje tamamen atlanır.

11. **Text pipeline**
    - Text (134): `align (281)`, `sizing (284)`, `overflow (287)`, `width (285)`, `height (286)`, `wrap (683)`, `verticalAlign (685)` vb.
    - TextStylePaint (137) → TextStyle (font) property’leri `fontSize (274)`, `fontAssetId (279)`, `lineHeight (370)` vb.
    - TextValueRun (135) doğrudan Text altında: `text (268)`, `styleId (272)` (artboard-lokal index).
    - FontAsset (141) + FileAssetContents (106) → `assetId (204)`, `bytes (212)`.

12. **State Machine**
    - StateMachine (53) → Inputs (56/58/59) → Layers (57) → States (61/62/63/64) → Transitions (65).
    - Sistem state’leri (Entry/Exit/Any) her layer’da ilk üç sırada, `stateToId (151)` layer-lokal index.
    - AnimationState `animationId (149)` artboard-lokal anim index (1 + stateMachineCount + animIndex).

13. **Artboard catalog**
    - Serializer katalog chunk’ını (8726/8776) yazmak için altyapıya sahip ancak Play “Unknown property key 8726” uyarısıyla drawables yaratmayı bırakıyor. Bu yüzden katalog yazımı **şu anda devre dışı** (yorum satırı, `serializer.cpp:455`).


## 4. Önemli property anahtarları
Aşağıdaki tablo hem builder’ın `PropertyTypeMap`’ini hem de serializer’ın ToC’sini güncel tutmak için kullanılıyor:

| Key | Açıklama |
|-----|----------|
| 3   | `ComponentBase::id` (artboard-lokal index)
| 4   | `ComponentBase::name`
| 5   | `ComponentBase::parentId`
| 7/8 | `LayoutComponentBase::width` / `height`
| 13/14/15/16/17/18 | `Node` transform (`x`, `y`, `rotation`, `scaleX`, `scaleY`, `opacity`)
| 20/21 | `ParametricPathBase::width` / `height`
| 23 | `DrawableBase::blendModeValue` (varsayılan 3 = SrcOver)
| 32 | `PointsCommonPathBase::isClosed`
| 37 | `SolidColorBase::colorValue`
| 38/39 | `GradientStop::colorValue` / `position`
| 41 | `ShapePaintBase::isVisible`
| 44 | `Backboard::mainArtboardId`
| 48/49 | `StrokeBase::cap` / `join`
| 56/57/59 | `LinearAnimation` fps / duration / loopValue
| 63-66 | `CubicInterpolator` kontrol noktaları
| 67 | `KeyFrame.frame`
| 69 | `InterpolatingKeyFrame.interpolatorId`
| 70 | `KeyFrameDouble.value`
| 88 | `KeyFrameColor.value`
| 89 | `Bone.length`
| 92 | `ClippingShape.sourceId`
| 104-109 | Bones/Skin bağlantıları
| 114-117 | `TrimPath` alanları
| 119/120 | `DrawTarget.drawableId` / `placementValue`
| 121 | `DrawRules.drawTargetId`
| 122 | `KeyFrameId.value`
| 128 | `PointsPath.pathFlags`
| 129 | `DrawableBase::drawableFlags`
| 138 | `StateMachineComponentBase::name`
| 140 | `Stroke.thickness`
| 141/142 | `StateMachineBool.value` / `StateMachineNumber.value`
| 149 | `AnimationState.animationId`
| 151/152/158 | Transition `stateToId`, `flags`, `duration`
| 164 | `Rectangle.linkCornerRadius`
| 173 | `TargetedConstraint.targetId`
| 179/180 | `TransformSpaceConstraint` source/dest space
| 181 | `KeyFrameBool.value`
| 196 | `LayoutComponentBase::clip`
| 204 | `FileAssetBase::assetId`
| 212 | `FileAssetContentsBase::bytes`
| 268 | `TextValueRun.text`
| 272 | `TextValueRun.styleId`
| 274 | `TextStyle.fontSize`
| 279 | `TextStyle.fontAssetId`
| 280 | `KeyFrameString.value`
| 285/286 | `Text.width` / `height`
| 363-365 | `FollowPathConstraint` özel alanları

## 5. UNIVERSAL exact stream JSON formatı
- Extractor `__riv_exact__ = true` içeren JSON üreterek `.riv` akışını **bire bir** yeniden kurar.
- Üst düzey alanlar:
  - `headerKeys`: ToC sırası; her girdide `key` (uint) ve `names` (diagnostic amaçlı).
  - `bitmaps`: 32‑bit field-type paketleri; `(len(headerKeys)+3)/4` adet olmalı.
  - `objects`: Sıralı nesne listesi. Her nesne `componentIndex` (diagnostik), `typeKey` ve `properties` içerir.
    - `properties[*].category` değer türünü belirtir (`uint`, `double`, `color`, `string`, `bytes`, `bool`).
    - Değer `category`ye göre ham varuint/float/bytes olarak yeniden yazılır; bilinmeyen kategoriler `uint` varsayımıyla varuint olarak dökülür.
  - `objectTerminator`: Obje akışını sonlandıran raw varuint baytları (base64). Bazı üretim dosyaları bu alanı boş bırakarak akışı dosya sonu ile bitirir; serializer yalnızca alan verilmişse bayt yazar, aksi halde eski JSON formatları için varsayılan `0` terminatörünü üretir.
  - `tail`: Obje akışından sonra kalan ham baytlar (örn. katalog chunk’ları) base64 olarak saklanır ve aynen tekrar yazılır.
- JSON ek alanları (ör. `source`, `inputPath`) serializer tarafından görmezden gelinir.
- Bu formatı kullanan CLI kodu `converter/src/main.cpp` içinde `serialize_exact_riv_json` yoluna delegasyon yapar.

(Not: Tablo sadece converter’ın bugün yazdığı property’leri listeler. Yeni tip eklerken tabloyu genişletin.)


### 4.1. Data binding / data converter property anahtarları
- `DataBind (typeKey 446)` → `propertyKey (586)`, `flags (587)`, `converterId (660)`
- `DataBindContext (typeKey 447)` → `sourcePathIds (588)` (bytes; JSON `uint16` listesi LE olarak paketlenir)
- `DataConverterBase (typeKey 488)` → `name (662)`
- `DataConverterRangeMapper (typeKey 519)` → `interpolationType (713)`, `interpolatorId (714)`, `flags (715)`, `minInput (716)`, `maxInput (717)`, `minOutput (718)`, `maxOutput (719)`
- `DataConverterToString (typeKey 490)` → `flags (764)`, `decimals (765)`, `colorFormat (766)`
- `DataConverterGroupItem (typeKey 498)` → `converterId (679)`
- `StateMachineListener (typeKey 114)` → `viewModelPathIds (868)` (bytes; extractor JSON destekli)
- `ListenerNumberChange (typeKey 118)` → `value (229)`

Serializer field-type bitmap eşlemesi:
- 586/587/660/713/714/715/719/764/765/679 → `CoreUintType`
- 716/717/718/229 → `CoreDoubleType`
- 662/766 → `CoreStringType`
- 588/868 → `CoreBytesType`

### 4.2. Data binding nesne sırası
- Backboard, JSON’daki `dataConverters` listesini `DataConverter` türevleri olarak ekler; `converterId` değerleri PASS3 sırasında artboard/backboard lokal indekslere remap edilir.
- Her converter’ın altındaki `contexts` dizisi `DataBindContext` objeleri üretir. `sourcePathIds` JSON’da `uint16` listesi olarak gelir; parser bunu depolar ve serializer varuint uzunluk + ham byte sekansı olarak yazar.
- Artboard içindeki `dataBinds`, hedef `Component` üretiminden sonra builder tarafından eklenir. `propertyKey` değerleri SDK header’larından (`*_base.hpp`) doğrulanır.
- State machine listener’ları ve listener action’ları hem `dataBinds` hem `dataBindings` anahtarlarıyla gelen binding listelerini destekler; parser iki adı da kabul eder ve tüm binding’ler artboard’a eklenir.
- PASS3 remap aşaması `converterId`, `targetId`, `inputId`, `viewModelPathIds` gibi referansları importer’ın beklediği artboard-lokal indekslere dönüştürür.


## 5. ID remap kuralları
- Artboard başlarken `localComponentIndex` map’i sıfırlanır; component id 0 artboard’a aittir.
- Writer component için `id (3)` ve gerekiyorsa `parentId (5)` yazar.
- Referans property’leri (`objectId (51)`, `sourceId (92)`, `styleId (272)`, `interpolatorId (69)`) yazılmadan önce artboard-lokal indexe remaplenir. Remap başarısızsa property tamamen atlanır (importer invalid index ile crash olmasın diye).
- PASS3, DrawTarget/DrawRules ve interpolator remap’lerini ayrıca yürütür (`deferredComponentRefs`).


## 6. Constraint ve dekoratör default’ları
- TrimPath: Start/End/Offset/Mode eksik ise 0/0/0/0 eklenir; parent Fill/Stroke değilse obje drop edilir.
- FollowPathConstraint: `distance=0`, `orient=true`, `offset=false`, `targetId=-1`, `source/destSpace=0` default.
- Feather: Runtime default 12.0 olduğundan eksik `strength` alanına 12.0 yazılır.
- Dash/DashPath: Eksikse `length=0`, `offset=0`, `%` bayrakları false.


## 7. Text pipeline
```
FontAsset (141)
  └─ FileAssetContents (106, bytes 212)
Artboard
  └─ Text (134)
       ├─ TextStylePaint (137)
       │   └─ SolidColor (18)
       └─ TextValueRun (135)
```
- TextValueRun doğrudan Text’in çocuğu olmak zorunda (runtime `onAddedClean` kontrolü).
- `styleId (272)` artboard-lokal index; serializer PASS3’te remapler.
- Font gövdesi FileAssetContents ile artboard akışında FontAsset’in hemen ardından yazılır.


## 8. State machine özet
- Objeler explicit `parentId` taşımaz; ImportStack sırası önemli: `StateMachine → Inputs → Layer → States → Transitions`.
- Her layer’da Entry/Exit/Any otomatik eklenir; `stateToId` layer-lokal indextir.
- `animationId` hesaplanırken artboard’daki state machine sayısı göz önünde bulundurulur.
- TransitionCondition henüz uygulanmadı (input ref remap eksik).


## 9. Analyzer / doğrulama akışı
1. `cmake --build build_converter --target rive_convert_cli import_test`
2. `./build_converter/converter/rive_convert_cli <json> out.riv`
3. `./build_converter/converter/import_test out.riv` → “SUCCESS” beklenir; objelerin `coreType` listesi burada görülebilir.
4. `python3 converter/analyze_riv.py out.riv` → ToC/stream karşılaştırması, header farkları, property dump.
5. Rive Play’de manuel doğrulama.

Sık hatalar:
- ToC’de property eksik/yanlış tip → importer hata.
- Parent remap yapılmadan global id yazmak → importer index out-of-range.
- Asset placeholder kaldırmak → Rive Play asset chunk beklerken hata.
- Field tipi yanlış (`CoreColorType` yerine `Uint`) → runtime property’yi yok sayar.


## 10. Bilinen sınırlamalar / TODO
- Artboard catalog (8726/8776) Play’de warning verdiği için kapalı; resmi encoding araştırılmalı.
- TransitionCondition henüz yok.
- ViewModel, LayoutComponentStyle (420) gibi karmaşık tipler JSON tarafında “stub” olarak kalıyor.
- Runtime tarafından üretilmiş (extractor’ın `resolvedObjects` patch’i) bileşenler için hiyerarşi normalde korunuyor; yeni tip eklerken extractor/builder eşleşmesini kontrol edin.

---
Bu belgeyi kodla senkron tutun. Bir property anahtarı, default değeri veya akış sırası değişirse önce burada güncelleyin, sonra PR açıklamasına ekleyin.

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
   - Rectangle (`typeKey 7`): `width (20)`, `height (21)`, `linkCornerRadius (382)=false`.
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
| 196 | `LayoutComponentBase::clip`
| 204 | `FileAssetBase::assetId`
| 212 | `FileAssetContentsBase::bytes`
| 382 | `RectangleBase::linkCornerRadius`

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

## 9. Acik Noktalar ve Yapilacaklar
- **StateMachine** ve **ViewModel** destegi alinmadi. Runtime typeKey 33 ve iliskili property'lere gecilecekse serializer'a yeni blok eklenmeli.
- **Drawable chain** ve **Artboard catalog** gibi yuksek seviye tipler referans `.riv` dosyalarinda yer aliyor. Komple sahneler icin bunlarin eklenmesi gerekebilir.
- **Gercek asset verisi** icin `ImageAsset` uzerindeki diger property'ler (width, height, file path) arastirilacak.

Bu dokumani guncel tutun. Yeni tip ekledigimizde veya runtime davranisi degistiginde once burada not alin, sonra kodu degistirin.

# RIV Runtime Format — Tersine Mühendislik Özeti

Bu doküman, bu repo içindeki Rive Runtime’ın gerçek okuma yoluna dayanarak `.riv` dosya yapısını ve üretici tarafında uymamız gereken kuralları derler. Tüm çıkarımlar kaynak koda (özellikle src/file.cpp, include/rive/runtime_header.hpp ve core field type implementasyonları) ve yaptığımız hexdump/importer testlerine dayanmaktadır.

## 1) Üstbilgi (Header)

Sıralama (include/rive/runtime_header.hpp):
- Magic: `"RIVE"` (4 bayt)
- `majorVersion` (varuint)
- `minorVersion` (varuint)
- `fileId` (varuint)
- Property ToC (Table of Contents): dosyada kullanılacak TÜM property key’leri, varuint olarak, `0` ile sonlanır. ToC yalnızca gerçekten yazılan anahtarları içermelidir.
- Property “field type” bitmap’i: ToC’deki her property için 2‑bit’lik tür kodları.

Alan türleri ve 2‑bit kodları (field_types ve src/file.cpp):
- `CoreUintType::id = 0`  → header kodu 0
- `CoreStringType::id = 1`→ header kodu 1
- `CoreDoubleType::id = 2`→ header kodu 2 (runtime okuması 4B float32)
- `CoreColorType::id = 3` → header kodu 3 (32‑bit RGBA)
- `bool` alanlar header bitmap’te `uint` (0) olarak değerlendirilir ve payload’da varuint `0/1` yazılır.

ÖNEMLİ ÇEŞİT (Bu Runtime’a özgü): Header bitmap okuması 32‑bit başına 4 adet 2‑bit kod tüketiyor.
- Kod (include/rive/runtime_header.hpp) akışı: `currentBit` 8’den başlar; `currentBit == 8` olduğunda yeni `uint32` okunur, `currentBit=0` yapılır; her property’de `fieldIndex = (currentInt >> currentBit) & 3` ve `currentBit += 2` ilerler.
- Bu davranış “32‑bit’te 4 kod” demektir (shift 0,2,4,6). Klasik 2‑bit×16/uint32 anlatımlarıyla uyumsuzdur. Converter’ın bitmap yazımı bu varyanta uymalıdır; aksi halde akış kayar ve importer yanlış tiplerle decode edip “Unknown property …/type_72” gibi hatalar verir.

## 2) Nesne Akışı (Object Stream)

Okuma (src/file.cpp → `readRuntimeObject`):
1) `coreType` (varuint) → `CoreRegistry::makeCoreInstance(coreType)`.
2) `propertyKey` (varuint) döngüsü; `0` görülünce nesne biter.
3) Nesne property’yi tanımazsa tip belirleme sırası:
   - `CoreRegistry::propertyFieldId(key)` → bulunamazsa
   - Header ToC bitmap’inden `fieldId`
   - Her ikisi de yoksa: `Unknown property key … missing from property ToC.`

Property değer kodlaması:
- `uint`/`Id`: varuint (LEB128)
- “double” (runtime’da float32): 4B LE IEEE754 (ham kopya güvenli)
- `color`: 32‑bit RGBA (0xAARRGGBB) tek `uint32` LE
- `string`: varuint uzunluk + ham baytlar
- `bool`: varuint `0/1`

Sonlandırmalar:
- Her nesne property listesi `0` ile biter.
- Akış sonunda tek bir `0` daha (null object) görülebilir; importer bunu no‑op sayar.

## 3) Önemli TypeKey’ler ve Property Key’ler

Sık TypeKey’ler (converter/typekey_mapping.json, generated headerlar):
- `23` Backboard
- `1`  Artboard
- `3`  Shape
- `7`  Rectangle
- `4`  Ellipse
- `20` Fill
- `24` Stroke
- `18` SolidColor

Sık Property Key’ler:
- `3`  ComponentBase::id (uint) — biz yazıyoruz (eşleştirme/analiz için yararlı)
- `5`  ComponentBase::parentId (uint) — hiyerarşi çözümü için kritik
- `4`  ComponentBase::name (string)
- `7`  LayoutComponentBase::width (float32)
- `8`  LayoutComponentBase::height (float32)
- `13` NodeBase::x (float32)
- `14` NodeBase::y (float32)
- `20` ParametricPathBase::width (float32)
- `21` ParametricPathBase::height (float32)
- `37` SolidColorBase::colorValue (color 0xAARRGGBB)
- `41` ShapePaintBase::isVisible (bool→uint/0)
- `196` LayoutComponentBase::clip (bool→uint/0)
- `44` Backboard::mainArtboardId (Id/uint) — varsayılan artboard’u işaretlemek için faydalı

Notlar:
- “Computed” alanlar (worldX, computedWidth vb.) serialize edilmez; ToC’ye de eklenmez.
- `bool` payload’da varuint yazılır, header bitmap’te `uint(0)` kodlanır.

## 4) Hiyerarşi ve `parentId` Semantiği

Importer hiyerarşiyi `Component::validate` ve `onAddedDirty` (src/component.cpp) aşamalarında çözer:
- `parentId(5)`, ARTBOARD İÇİ yerel indeksleri referanslar (artboard’ın `objects()` listesine göre). `id(3)` ile karıştırılmamalıdır; importer ebeveyn ararken `id(3)`’e bakmaz.

Minimal zincir için indeksleme örneği:
- Artboard → indeks 0
- İlk `Shape` → `parentId = 0`
- `Rectangle` → `parentId = 1` (ebeveyn: Shape)
- `Fill`/`Stroke` → `parentId = 1` (ebeveyn: Shape)
- `SolidColor` → `parentId` = bağlı olduğu paint’in indeksi

Uygulama kuralları:
- `Backboard` bir `Component` değildir; `id(3)`/`parentId(5)` yazmayın.
- `id(3)` yazmak zorunlu değil ama analiz/izleme için yararlıdır.
- `Backboard.mainArtboardId(44)`’ü ilgili artboard indeksine ayarlamak varsayılanı belirlemeye yardımcı olur.

## 5) Sıra, ToC ve Terminasyon

Önerilen nesne sırası: `Backboard (23) → Artboard (1) → Shape (3) → Rectangle (7) → Fill (20) → SolidColor (18)`.

Önerilen property sırası (kolay teşhis için): `3 (id) → 5 (parent) → 7/8/13/14/20/21 (float32) → 4 (string) → 41/196 (bool/uint) → 37 (color)`.

ToC:
- Yalnızca gerçekten yazılan anahtarları içerir.
- Bitmap, bu runtime’daki “32‑bit başına 4 kod” okuma düzeniyle uyumlu yazılmalıdır.

Terminasyon:
- Nesne sonu: tek `0`
- Akış sonu: tek `0` (null)

## 6) Asset ve Katalog Blokları

Importer düz akışı kabul eder; asset/katalog olmadan da dosya yüklenebilir. Bazı araçlar (ör. Rive Play) beklentiler ekleyebilir:
- `FileAssetContents` TypeKey: **106** — asset paketlerini taşır (boş paket de olabilir).
- “Artboard Catalog” benzeri listeler: resmi tanım import akışında özel bir tip olarak yer almıyor; Play’e özel bir üst katman olabilir. Şimdilik “TBD”.

Öneri: Önce hiyerarşi ve property hizasını sağlamlaştırın; ardından gerektiğinde asset/katalog placeholder’larını ekleyin.

## 7) Saha Hataları ve Teşhis İpuçları

Belirtiler → Olası Nedenler:
- `Unknown property key 8648/...`, `type_72` gibi sahte nesneler → Header bitmap paketi runtime varyantıyla uyumsuz (özellikle 2‑bit paketlemenin yanlış grup boyu) veya ToC≠stream.
- Artboard‑only dosya bile `Malformed` → `parentId` karışıklığı (id(3) vs artboard‑local indeks), Backboard/Artboard property sonlandırmalarında fazladan/eksik `0`.
- Hiyerarşi çözülemiyor → `parentId` artboard‑local indeksleri işaret etmiyor; builder ID’leri doğrudan kullanılmış.

Kontrol listesi:
- ToC ⟷ stream birebir (serializer sonunda eşitlik kontrolü yapın).
- Bitmap paketi bu runtime’ın okuma biçimine uyumlu.
- `float` 4B LE ham yazım; `string` varlen+bytes; `bool` varuint 0/1; `color` 4B RGBA.
- Her nesnede tek `0`, akış sonunda tek `0`.
- `Backboard` için component alanı yazmayın; gerekirse `mainArtboardId(44)` ayarlayın.

## 8) Mevcut Uygulama Durumu (Converter)

- Serializer: ToC’yi dinamik toplar, field türlerini 2‑bit’e map’ler ve unified writer ile değerleri doğru ikili formda yazar.
- Builder: Artboard/Shape/Rectangle/Fill/Stroke/SolidColor için gerekli property’leri sağlar. `parentId` tekrar yazımı kaldırılmalı ve artboard‑local indeks remap’i uygulanmalıdır.
- Asset/katalog placeholder’ları şu an devre dışı; hiyerarşi stabilize edildikten sonra eklenecek.

## 9) Minimal Örnekler

- Artboard‑Only: Backboard(23), Artboard(1) → ToC: `[3,5,7,8]` (isteğe bağlı `44`).
- Shape/Rectangle: Artboard’a ek olarak Shape(3)[`5`], Rectangle(7)[`5,13,14,(20,21)`].
- Boyutsal alanlar (7,8,13,14,20,21) 4B float32 LE yazılır.

Bu doküman, her yeni doğrulama ve import davranışı tespitinde güncellenecektir.

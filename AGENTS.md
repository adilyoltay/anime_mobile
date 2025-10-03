# Agent Quickstart

## 1. Proje Ozeti
- **Adres**: `/Users/adilyoltay/Desktop/anime_mobile`
- **Amac**: Sinirli bir JSON sahnesini (artboard + temel sekiller) resmi Rive runtime'in okuyabilecegi tek parca `.riv` dosyasina donusturmek.
- **Ana kaynak**: `converter/` klasoru (parser, builder, serializer).
- **Durum**: Minimal sahneler (Backboard + tek artboard + sekiller + asset placeholder) dogru sekilde uretiliyor ve Rive Play importunda calisiyor.

## 2. Referans Belgeler
- **📚 `docs/`**: Detayli dokumanlar klasoru
- `converter/src/riv_structure.md`: Binary format ve serializer davranisinin ayrintili ozeti. KANONIK SPES; her yeni tip/field/varsayilan/deger degisikliginde GUNCELLENMESI ZORUNLUDUR.
- `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md`: ID remap ve parent/child hiyerarsi analizinin KANONIK KAYDI. `localId`/`objectId`/`parentId`/ordering degisiklikleri bu belgede kayit altina alinmalidir.
- `converter/src/core_builder.cpp`: JSON verisinden runtime core nesnelerini olusturan katman.
- `converter/src/serializer.cpp`: Core nesnelerini `.riv` byte dizisine ceviren katman.
- `converter/analyze_riv.py`: Uretilen dosyalarin header/stream dogrulamasini yapan araci.

## 2.1 Zorunlu SDK Uyumlulugu
- Tum gelistirme kararlarinin Rive runtime SDK'nin guncel tanimlariyla (typeKey, property key, field tipi, varsayilan deger) bire bir uyumlu olmasi ZORUNLUDUR.
- Yeni bir anahtar/ozellik eklenmeden once `include/rive/generated/` altindaki ilgili `*_base.hpp` dosyasi ile `include/rive/core_registry.hpp` kayitlarina bakilarak dogrulama yapilacak.
- Harici belgelerden gelen gecici haritalar veya varsayimlar kesinlikle kullanilmayacak; SDK tarafinda karsiligi yoksa ozellik eklenmeyecek.
- Uyumsuzluk tespit edilirse degisiklik geri cekilir, tespit eden ajan bunu raporlamakla yukumludur.
- SDK basvurulari (TEK DOGRULUK KAYNAGI): `include/rive/generated/**/*_base.hpp` ve `include/rive/generated/core_registry.hpp`. Tum `typeKey`/`propertyKey`/field tipi/varsayilan degerler bu basliklardan teyit edilmelidir.
- Belge uyumu: `converter/src/riv_structure.md` ve `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md` belgeleri SDK basliklariyla celisirse, SDK basliklari ESASTIR. Belgeler derhal guncellenmelidir.

## 3. Build ve Test Rutini
```bash
cmake -S . -B build_converter
cmake --build build_converter --target rive_convert_cli import_test
./build_converter/converter/rive_convert_cli <json> <out.riv>
./build_converter/converter/import_test <out.riv>
python3 converter/analyze_riv.py <out.riv>
```
- `import_test` calistiktan sonra `SUCCESS` mesaji beklenir.
- Analyzer ciktilarinda `toc` ile `streamProps` kesismiyorsa serializer guncellemesi gerekir.

### 3.1 Test Dosyalari
```bash
# Minimal test (basit geometri)
converter/exampleriv/rectangle.riv (230 bytes)

# Production test (animasyon + state machine)
converter/exampleriv/bee_baby.riv (9.7KB)

# Round-trip test
scripts/roundtrip_compare.sh converter/exampleriv/bee_baby.riv
```

### 3.2 Debugging Araclari
```bash
# RIV analizi
python3 converter/analyze_riv.py <file.riv>        # Header + ToC dogrulama
python3 converter/analyze_chunks.py <file.riv>     # Chunk-level detay
python3 converter/validate_riv.py <file.riv>       # Format dogrulama

# Round-trip karsilastirma
scripts/roundtrip_compare.sh <original.riv>        # Full pipeline test
scripts/simple_stability_test.sh                   # Quick stability check
```

## 4. Calisma Akisi (JSON -> RIV)

### Pipeline Dosya Sorumlulugu
```
RIV → JSON (Export):
  converter/universal_extractor.cpp      # Runtime objects → JSON export
  converter/extractor_postprocess.hpp    # Topological sort + validation

JSON → RIV (Import):
  converter/src/universal_builder.cpp    # JSON → Core objects + property wiring
  converter/src/serializer.cpp           # Core objects → .riv binary

Yeni TypeKey eklerken:
  1. universal_builder.cpp: createObjectByTypeKey() + property mapping
  2. serializer.cpp: PropertyTypeMap + field-type bits (2-bit)
  3. riv_structure.md: Belgeleme guncelle
  4. ID_MAPPING_HIERARCHY_ANALYSIS.md: ID/ordering degisikliklerini kaydet
```

### Adimlar
1. JSON girisi hierarchical veya legacy format olabilir (auto-detect).
2. `universal_builder.cpp`: Backboard + Artboard + objects olusturur; parent indexleri artboard-local'e remap eder.
3. Topological sort: Parent-first + objectId dependency ordering.
4. `serializer.cpp`: ToC + field-type bitmap olusturur, nesneleri sirayla yazar.
5. Test: `import_test` SUCCESS + `analyze_riv.py` ile ToC/stream dogrulama.

## 5. Siklikla Yapilan Hatalar
- ToC'ye yeni property eklemeyi unutmak -> importer `Unknown property key` hatasi verir.
- `parentId` degerlerinde artboard-local index yerine JSON ID'si kullanmak -> sahne bos ya da hatali yuklenir.
- Asset placeholder'ini kaldirmak -> Rive Play bos asset chunk beklerken hata verir.
- Float alanlari varuint olarak yazmak -> runtime degeri 0 olarak okur.
- LinearAnimation'a `localId` vermemek -> Keyed data stream sonuna dusup MALFORMED hatasina yol acar.
- Topological sort'ta `objectId` dependency'yi gozardi etmek -> KeyedObject target'tan once gelip NULL object yaratir.

## 5.1 Hata Ayiklama Workflow
```
import_test FAILED:
  1. analyze_riv.py ile ToC kontrol et (toc vs streamProps)
  2. Property key mapping'i dogrula (SDK headers vs serializer)
  3. Field-type bit dogrulugunu kontrol et (varuint/double/color)

NULL objects:
  1. ID_MAPPING_HIERARCHY_ANALYSIS.md'yi incele
  2. localId assignment kontrol et (extractor)
  3. Topological sort ordering kontrol et (postprocess)
  4. Parent/objectId dependencies dogrula

Grey screen / Render issue:
  1. Clip property (196) kontrol et (default=false)
  2. Blend mode (23) kontrol et (default=3, SrcOver)
  3. Drawable flags (129) kontrol et (default=0; Hidden=1, Opaque=8)

Round-trip instability:
  1. interpolatorId (69) eksik mi?
  2. objectId remap calisiyor mu?
  3. Shared interpolator dedup'i var mi?
```

## 6. Kontrol Listesi (Yeni Ozellik Eklerken)
- [ ] Yeni property anahtarini `PropertyTypeMap` ve ToC'ye eklediniz mi?
- [ ] Field-type bitmap icin dogru 2-bit kodu kullandiniz mi?
- [ ] `parentId` artboard icindeki indexe isaret ediyor mu?
- [ ] Analyzer'da `toc` ve `streamProps` listeleri birebir mi?
- [ ] `import_test` ve Rive Play importu sorunsuz mu?
- [ ] `converter/src/riv_structure.md` ve `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md` belgelerini guncel tuttunuz mu?

## 7. Common Workflows

### Yeni Ozellik Ekleme
```
1. SDK basliklari kontrol et:
   include/rive/generated/<type>_base.hpp
   include/rive/core_registry.hpp

2. Builder support ekle:
   universal_builder.cpp:
   - createObjectByTypeKey() case ekle
   - Property mapping ekle (line ~200-400)
   - Default injection ekle (line ~1000-1200)

3. Serializer property mapping:
   serializer.cpp:
   - PropertyTypeMap'e ekle (varuint/double/color/string)
   - Field-type bitmapi guncelle (2-bit: 00/01/10/11)

4. Test:
   - rectangle.riv ile minimal test
   - bee_baby.riv ile production test
   - roundtrip_compare.sh ile stability test

5. Belgele:
   - riv_structure.md: Type/property documentation
   - ID_MAPPING_HIERARCHY_ANALYSIS.md: ID/ordering changes
```

### Hata Ayiklama
```
1. import_test FAILED:
   analyze_riv.py ile ToC kontrol

2. NULL objects:
   ID mapping kontrol et (extractor + postprocess)

3. Grey screen:
   Clip/blend/drawable flags kontrol et

4. Round-trip instability:
   interpolatorId + objectId remap kontrol et
```

Her degisiklikten sonra hem bu dosyayi hem de `converter/src/riv_structure.md` ile `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md` belgelerini guncel tutun.

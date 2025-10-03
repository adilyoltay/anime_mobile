# Agent Quickstart

## 1. Proje Ozeti
- **Adres**: `/Users/adilyoltay/Desktop/anime_mobile`
- **Amac**: Sinirli bir JSON sahnesini (artboard + temel sekiller) resmi Rive runtime'in okuyabilecegi tek parca `.riv` dosyasina donusturmek.
- **Ana kaynak**: `converter/` klasoru (parser, builder, serializer).
- **Durum**: Minimal sahneler (Backboard + tek artboard + sekiller + asset placeholder) dogru sekilde uretiliyor ve Rive Play importunda calisiyor.
- **Yeni Yetenek**: Exact round-trip mode - RIV → JSON → RIV byte-perfect reconstruction (__riv_exact__ format)

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

## 2.2 Zorunlu Dokuman Guncellemesi
- Asagidaki degisikliklerin HERHANGISINDE asagidaki dokumanlar GUNCELLENMEK ZORUNDADIR:
  - `docs/STATE_MACHINE_BPM_BINDING_ANALYSIS.md` (State Machine BPM binding, DataBind/DataConverter/Listener kapsamı)
  - `docs/RIVE_RUNTIME_JSON_URETIM_KURAL_SETI.md` (JSON uretim kurallari; NL→JSON icin tek referans)
  - `converter/src/riv_structure.md`
  - `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md`
- Tetikleyiciler (zorunlu guncelleme durumlari):
  - `include/rive/generated/**/*_base.hpp` altinda DataBind/DataBindContext, DataConverter* (RangeMapper/ToString/GroupItem), StateMachine*, Listener* icin `typeKey`/`propertyKey`/field tipi degisikligi
  - `converter/src/universal_builder.cpp`, `converter/src/core_builder.cpp`, `converter/src/serializer.cpp`, `converter/src/json_loader.cpp` dosyalarinda ilgili mapping/varsayilan/ID remap degisiklikleri
  - Yeni binding/Listener tipi eklenmesi veya mevcutlarin davranissal degisimi
- Proses:
  - Degisiklik PR'inda “Docs: STATE_MACHINE_BPM_BINDING_ANALYSIS.md updated” kutucugunu isaretleyin ve degisiklikleri PR aciklamasinda ozetleyin.
  - Basliklarla (SDK) bu dokumanlar celisir ise SDK esastir; dokumanlar derhal guncellenir.

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
converter/exampleriv/test.riv (230 bytes)

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
scripts/roundtrip_compare.sh <original.riv>        # Full pipeline test (auto-detects exact mode)
scripts/simple_stability_test.sh <original.riv>    # 3-cycle stability test (auto-detects exact mode)
```

## 4. Calisma Akisi (JSON -> RIV)

### Pipeline Dosya Sorumlulugu
```
RIV → JSON (Export):
  converter/universal_extractor.cpp      # Runtime objects → JSON export
  converter/extractor_postprocess.hpp    # Topological sort + validation
  converter/analyze_riv.py:62-320        # Exact mode: Property types from headers + objectTerminator

JSON → RIV (Import):
  converter/src/universal_builder.cpp    # JSON → Core objects + property wiring
  converter/src/serializer.cpp:880-999   # Exact mode: ToC/bitmap validation + category-based serialization

Exact Round-Trip Mode:
  universal_extractor.cpp → __riv_exact__ JSON with:
    - componentIndex (diagnostic)
    - Property types from generated headers
    - objectTerminator (raw terminator bytes)
    - tail (post-stream bytes: catalog chunks)
  serializer.cpp → Byte-perfect reconstruction:
    - ToC/bitmap validation against JSON
    - 64-bit varuint fixes
    - Category-based value serialization (uint/double/string/bytes/bool)
    - Exact terminator/tail byte replication

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
- [ ] `import_test` ve RivHer degisiklikten sonra hem bu dosyayi hem de `converter/src/riv_structure.md`, `docs/ID_MAPPING_HIERARCHY_ANALYSIS.md`, `docs/STATE_MACHINE_BPM_BINDING_ANALYSIS.md` ve `docs/RIVE_RUNTIME_JSON_URETIM_KURAL_SETI.md` belgelerini guncel tutun.
uz mu?

## 7. Common Workflows

### Exact Round-Trip Test
```bash
# Byte-perfect round-trip validation (minimal test)
./build_converter/converter/universal_extractor converter/exampleriv/test.riv output/tests/test_exact.json
./build_converter/converter/rive_convert_cli --exact output/tests/test_exact.json output/tests/test_exact_roundtrip.riv
cmp converter/exampleriv/test.riv output/tests/test_exact_roundtrip.riv  # Should be identical
./build_converter/converter/import_test output/tests/test_exact_roundtrip.riv  # Should show SUCCESS

# Production file validation (catalog-heavy: animations + state machines)
./build_converter/converter/universal_extractor converter/exampleriv/bee_baby.riv output/tests/bee_exact.json
./build_converter/converter/rive_convert_cli --exact output/tests/bee_exact.json output/tests/bee_exact_roundtrip.riv
cmp converter/exampleriv/bee_baby.riv output/tests/bee_exact_roundtrip.riv  # ✅ VERIFIED byte-perfect
```

**CLI Usage:**
```bash
# Exact mode (requires __riv_exact__ = true in JSON)
rive_convert --exact <exact.json> <output.riv>

# Auto-detect mode (works with all formats)
rive_convert <input.json> <output.riv>

# Without --exact flag, exact JSON will show warning:
# ⚠️  Warning: Exact mode JSON detected. Consider using --exact flag for clarity.

# With --exact flag on non-exact JSON, will error:
# ❌ Error: --exact flag requires JSON with __riv_exact__ = true
```

**Critical Requirements:**
- Extractor must output `__riv_exact__ = true` for exact mode
- JSON must include `headerKeys`, `bitmaps`, `objectTerminator`, `tail` fields
- Serializer validates ToC/bitmap against JSON before writing
- objectTerminator field preserves exact stream termination (empty = EOF, non-empty = explicit terminators)
- Category field drives serialization: uint→varuint, double→float64, color→color32, string→string, bytes→raw
- Use `--exact` flag to enforce exact mode and prevent accidental universal JSON processing

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
   - test.riv ile minimal test
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

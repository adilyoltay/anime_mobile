# Agent Quickstart

## 1. Proje Ozeti
- **Adres**: `/Users/adilyoltay/Desktop/rive-runtime`
- **Amac**: Sinirli bir JSON sahnesini (artboard + temel sekiller) resmi Rive runtime'in okuyabilecegi tek parca `.riv` dosyasina donusturmek.
- **Ana kaynak**: `converter/` klasoru (parser, builder, serializer).
- **Durum**: Minimal sahneler (Backboard + tek artboard + sekiller + asset placeholder) dogru sekilde uretiliyor ve Rive Play importunda calisiyor.

## 2. Referans Belgeler
- **📚 `docs/`**: Detayli dokümanlar klasörü (hierarchical parser, implementation guides)
  - `docs/HIERARCHICAL_COMPLETE.md`: Hierarchical parser production dokumanı
  - `docs/NEXT_SESSION_HIERARCHICAL.md`: Implementation guide ve referans
- `converter/src/riv_structure.md`: Binary format ve serializer davranisinin ayrintili ozeti. Ilk once burayi inceleyin.
- `converter/src/core_builder.cpp`: JSON verisinden runtime core nesnelerini olusturan katman.
- `converter/src/serializer.cpp`: Core nesnelerini `.riv` byte dizisine ceviren katman.
- `converter/analyze_riv.py`: Uretilen dosyalarin header/stream dogrulamasini yapan araci.

## 3. Build ve Test Rutini
```
cmake -S . -B build_converter
cmake --build build_converter --target rive_convert_cli import_test
./build_converter/converter/rive_convert_cli <json> <out.riv>
./build_converter/converter/import_test <out.riv>
python3 converter/analyze_riv.py <out.riv>
```
- `import_test` calistiktan sonra "SUCCESS" mesaji beklenir.
- Analyzer ciktilarinda `toc` ile `streamProps` kesismiyor ise serializer guncellemesi gerekir.

## 4. Calisma Akisi (JSON -> RIV)
1. JSON girisi `converter/include/json_loader.hpp` tanimindaki skemaya uymali (artboard + sekiller).
2. `build_core_document` Backboard, Artboard, varsayilan LinearAnimation ve sekil hiyerarsisini olusturur; parent indexleri artboard icinde yeniden haritalanir.
3. `serialize_minimal_riv` ToC ve field-type bitmapini olusturur, nesneleri sirayla yazar, Backboard sonrasinda asset placeholder bloklarini ekler.
4. Rive Play testinde sorun gorurseniz once analyzer ile header/stream uyumunu bulun, ardindan parent indexlemeyi kontrol edin.

## 5. Siklikla Yapilan Hatalar
- ToC'ye yeni property eklemeyi unutmak -> importer "Unknown property key" hatasi verir.
- `parentId` degerlerinde artboard-lokal index yerine JSON ID'si kullanmak -> sahne bos ya da hatali yuklenir.
- Asset placeholder'ini kaldirmak -> Rive Play bos asset chunk beklerken hata verir.
- Float alanlari varuint olarak yazmak -> runtime degersiz veriyi 0 olarak okur.

## 6. Text Rendering (TAMAMLANDI - Sep 30, 2024)
- ✅ Text (134), TextStylePaint (137), TextValueRun (135) tamamen implement edildi.
- ✅ FontAsset (141) + FileAssetContents (106) ile font embedding calisiyor (Arial.ttf 755KB).
- ✅ Paint hierarchy: SolidColor → Fill → TextStylePaint.
- ✅ Property keys: text (268), styleId (272) artboard-local remapping ile.
- ✅ Rive Play'de test edildi: "Hello World" gorunuyor!
- ✅ Coklu text objeleri calisiyor (9 text tested).

## 7. State Machine (TAMAMLANDI - Sep 30, 2024)
- ✅ StateMachine (53) + Inputs (Bool/Number/Trigger) calisiyor.
- ✅ StateMachineLayer (57) ile katman yonetimi implement edildi.
- ✅ Sistem state'leri (Entry/Exit/Any) her layer'a otomatik ekleniyor.
- ✅ AnimationState (61) ile animation referanslari calisiyor.
- ✅ StateTransition (65) ile state gecisleri implement edildi.
- ✅ animationId ve stateToId artboard/layer-local index remapping ile.
- ✅ Import testleri basarili: 1 input, 1 layer, 5 state, 3 transition.
- ⏳ TransitionCondition henuz implement edilmedi (input mapping gerekli).

## 8. Multiple Artboards (TAMAMLANDI - Sep 30, 2024)
- ✅ "artboards" array format desteği eklendi.
- ✅ Her artboard kendi shapes, texts, animations, stateMachines'lere sahip.
- ✅ Builder loop tüm artboard'lari isliyor.
- ✅ 2 ve 3 artboard testleri basarili.
- ✅ Legacy tek artboard format hala calisiyor (backwards compatible).
- ✅ Apex Legends yapisi (3 artboard, artboard basina birden fazla SM) destekleniyor.
- ✅ Font loading tüm artboard'lar icin calisior.

## 9. Custom Path Vertices (TAMAMLANDI - Sep 30, 2024)
- ✅ StraightVertex (5), CubicDetachedVertex (6), PointsPath (16)
- ✅ Property keys: 24-26, 84-87, 120
- ✅ Casino Slots %66'sini unlock etti (10,366 obje)

## 10. Events & Bones (TAMAMLANDI - Sep 30, 2024)  
- ✅ Event (128), AudioEvent (407), property key 408
- ✅ Bone (40), RootBone (41), Tendon (44), Weight (45), Skin (43)
- ✅ Property keys: 89-91, 104-109
- ✅ Casino Slots %100 destek!

## 11. Hierarchical Parser (TAMAMLANDI - Sep 30, 2024)
- ✅ **PRODUCTION READY!** Shape geometry %100 perfect copy!
- ✅ Multi-path-per-shape architecture (781 Shapes, 897 Paths)
- ✅ Reference remapping: objectId (51), sourceId (92), styleId (272)
- ✅ Asset streaming: FontAsset → FileAssetContents adjacency
- ✅ Property optimization: Default suppression (5% size reduction)
- ✅ Format auto-detection: Hierarchical vs legacy JSON
- ✅ Casino Slots test: 15,210/15,683 objects (97.0%)
- ✅ Core geometry: 11,044/11,044 objects (%100.0!)
- **Files:** `hierarchical_parser.cpp`, `hierarchical_schema.hpp`
- **Pipeline:** RIV → extractor → hierarchical JSON → parser → builder → RIV
- **Belgeler:** `HIERARCHICAL_COMPLETE.md`, `NEXT_SESSION_HIERARCHICAL.md`

## 12. Acik Gorevler
- TransitionCondition (%1 - opsiyonel)
- ViewModel/Data Binding (Casino Slots'ta yok)
- Mesh vertices (Casino Slots'ta yok)
- Hierarchical extractor genisletme (Nodes, Events, Bones - %3 eksik)

## 13. Multiple Artboards Kontrol Listesi
- [x] JSON format "artboards" array kullan.
- [x] Her artboard kendi content field'larina sahip.
- [x] Builder'da tüm artboard'lar icin loop.
- [x] Legacy format desteği koru (tek artboard).
- [x] Font loading tüm artboard'lar icin kontrol et.
- [x] 2+ artboard ile test et.

## 14. State Machine Kontrol Listesi
- [ ] Core nesneleri icin `setParent()` KULLANMA - implicit file order kullan.
- [ ] Her layer icin Entry/Exit/Any state'lerini MUTLAKA ekle (indices 0,1,2).
- [ ] animationId artboard-local index kullan: 1 + stateMachineCount + animIndex.
- [ ] stateToId layer-local index kullan: 0-based state sirasi.
- [ ] Nesne sirasi: StateMachine → Inputs → Layer → States → Transitions.
- [ ] Property keys: 149 (animationId), 151 (stateToId), 152 (flags), 158 (duration).
- [ ] Import testinde layer ve state sayilarini kontrol et.

## 15. Kontrol Listesi (Yeni Ozellik Eklerken)
- [ ] Yeni property anahtarini `PropertyTypeMap` ve ToC'ye eklediniz mi?
- [ ] Field-type bitmapi icin dogru 2-bit kodu kullandiniz mi?
- [ ] `parentId` artboard icindeki indexe isaret ediyor mu?
- [ ] Analyzer'da `toc` ve `streamProps` listeleri birebir mi?
- [ ] `import_test` ve Rive Play importu sorunsuz mu?
- [ ] `riv_structure.md` dokumanini guncel tutun; yeni tip eklemeden veya runtime davranisini degistirmeden once degisiklikleri burada not alin.

Her degisiklikten sonra hem bu dosyayi hem de `riv_structure.md` belgesini guncel tutun.

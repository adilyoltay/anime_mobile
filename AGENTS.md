# Agent Quickstart

## 1. Proje Ozeti
- **Adres**: `/Users/adilyoltay/Desktop/rive-runtime`
- **Amac**: Sinirli bir JSON sahnesini (artboard + temel sekiller) resmi Rive runtime'in okuyabilecegi tek parca `.riv` dosyasina donusturmek.
- **Ana kaynak**: `converter/` klasoru (parser, builder, serializer).
- **Durum**: Minimal sahneler (Backboard + tek artboard + sekiller + asset placeholder) dogru sekilde uretiliyor ve Rive Play importunda calisiyor.

## 2. Referans Belgeler
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

## 7. Acik Gorevler
- State Machine ve ViewModel tipleri icin serializer destegi yok; eklenirse `riv_structure.md` ve ToC mantigi guncellenmeli.
- Drawable chain / Artboard catalog gibi ust seviye tipler implement edilmedi. Daha karmasik sahneler icin referans `.riv` dosyalari incelenmeli.
- Text animation (text property keyframe'leri) henuz test edilmedi.

## 7. Kontrol Listesi (Yeni Ozellik Eklerken)
- [ ] Yeni property anahtarini `PropertyTypeMap` ve ToC'ye eklediniz mi?
- [ ] Field-type bitmapi icin dogru 2-bit kodu kullandiniz mi?
- [ ] `parentId` artboard icindeki indexe isaret ediyor mu?
- [ ] Analyzer'da `toc` ve `streamProps` listeleri birebir mi?
- [ ] `import_test` ve Rive Play importu sorunsuz mu?
- [ ] `riv_structure.md` dokumanini guncel tutun; yeni tip eklemeden veya runtime davranisini degistirmeden once degisiklikleri burada not alin.

Her degisiklikten sonra hem bu dosyayi hem de `riv_structure.md` belgesini guncel tutun.

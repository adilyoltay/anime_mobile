# 🧹 Proje Temizlik Planı

## 📊 Mevcut Durum Analizi

### Disk Kullanımı:
- build_converter/: 845 MB ✅ **KORUMA** (aktif build)
- build_real/: 270 MB ❌ **ARŞİV** (eski deneme)
- build_working/: 246 MB ❌ **ARŞİV** (eski deneme)
- build/: 80 KB ❌ **ARŞİV** (boş/eski)
- **Toplam tasarruf: ~516 MB**

### Root Level Karmaşası:
- 10 test JSON dosyası
- 4 test RIV dosyası
- 6 Python script
- 9 eski converter .cpp/.hpp
- 6 CMakeLists varyasyonu

---

## 🗂️ ARŞİV YAPISI

```
archive/
├── old_builds/          # Eski build klasörleri
│   ├── build_real/
│   ├── build_working/
│   └── build/
│
├── old_converters/      # Eski converter denemelerı
│   ├── improved_json_converter.cpp
│   ├── simple_json_converter.cpp
│   ├── working_riv_converter.cpp
│   ├── json_to_riv_converter.*
│   ├── real_converter_main.cpp
│   └── real_json_to_riv_converter.*
│
├── old_cmake/           # Eski CMakeLists varyasyonları
│   ├── CMakeLists_heart.txt
│   ├── CMakeLists_improved.txt
│   ├── CMakeLists_real.txt
│   ├── CMakeLists_simple.txt
│   └── CMakeLists_working.txt
│
├── test_jsons/          # Test JSON dosyaları
│   ├── ALL_CASINO_TYPES_TEST.json
│   ├── advanced_effects.json
│   ├── all_shapes.json
│   ├── artboard_only.json
│   ├── bee_detailed.json
│   ├── bouncing_ball.json
│   ├── breathe.json
│   ├── complete_demo.json
│   └── dash_test.json
│
├── test_rivs/           # Test RIV dosyaları
│   ├── artboard_only.riv
│   ├── casino_PERFECT.riv (v1 - superseded)
│   └── test_multi_chunk.riv
│
└── scripts/             # Python analiz scriptleri
    ├── analyze_casino_detailed.py
    ├── compare_exact_copy.py
    ├── extract_casino_structure.py
    ├── generate_casino_exact_copy.py
    ├── riv_to_json_complete.py
    └── validate_exact_match.py
```

---

## ✅ KORUMA (Root Level'da Kalacak)

### Aktif Klasörler:
- `converter/` - Ana kaynak kod
- `build_converter/` - Aktif build
- `src/`, `include/` - Rive runtime core
- Diğer runtime klasörleri (renderer, tests, vb.)

### Aktif Dosyalar:
- `CMakeLists.txt` - Ana build config
- `casino_HIERARCHICAL.json` - Son extraction (3.3 MB)
- `casino_PERFECT_v2.riv` - Son optimized output (435 KB)

### Dokümanlar:
- `README.md`
- `AGENTS.md` ✅ **GÜNCEL**
- `HIERARCHICAL_COMPLETE.md` ✅ **YENİ**
- `NEXT_SESSION_HIERARCHICAL.md` - Referans
- `PROJECT_STATUS.md`
- `LICENSE`

---

## 🗑️ ARŞİVLENECEKLER

### Build Klasörleri (~516 MB):
```bash
mv build_real/ archive/old_builds/
mv build_working/ archive/old_builds/
mv build/ archive/old_builds/
```

### Eski Converter Dosyaları:
```bash
mv *json_converter* archive/old_converters/
mv *real_converter* archive/old_converters/
mv simple_json_converter.cpp archive/old_converters/
mv working_riv_converter.cpp archive/old_converters/
mv test_converter.cpp archive/old_converters/
```

### Eski CMakeLists:
```bash
mv CMakeLists_*.txt archive/old_cmake/
```

### Test Dosyaları:
```bash
# JSON tests (keep casino_HIERARCHICAL.json!)
mv ALL_CASINO_TYPES_TEST.json archive/test_jsons/
mv advanced_effects.json archive/test_jsons/
mv all_shapes.json archive/test_jsons/
mv artboard_only.json archive/test_jsons/
mv bee_detailed.json archive/test_jsons/
mv bouncing_ball.json archive/test_jsons/
mv breathe.json archive/test_jsons/
mv complete_demo.json archive/test_jsons/
mv dash_test.json archive/test_jsons/

# RIV tests (keep casino_PERFECT_v2.riv!)
mv artboard_only.riv archive/test_rivs/
mv casino_PERFECT.riv archive/test_rivs/  # v1, superseded by v2
mv test_multi_chunk.riv archive/test_rivs/
```

### Python Scripts:
```bash
# Arşivle ama tamamen silme - analiz için faydalı olabilir
mv analyze_casino_detailed.py archive/scripts/
mv compare_exact_copy.py archive/scripts/
mv extract_casino_structure.py archive/scripts/
mv generate_casino_exact_copy.py archive/scripts/
mv riv_to_json_complete.py archive/scripts/
mv validate_exact_match.py archive/scripts/
```

---

## 📋 TEMİZLİK ADIMLARI

1. Arşiv yapısını oluştur
2. Dosyaları kategorize ederek taşı
3. Root level'ı temizle
4. Son durumu doğrula

---

## ⚠️ KORUMA KURALLARI

**KESİNLİKLE DOKUNMA:**
- converter/ (ana kaynak)
- build_converter/ (aktif build)
- include/, src/ (runtime core)
- renderer/, tests/, decoders/ (runtime modules)
- CMakeLists.txt (ana config)
- casino_HIERARCHICAL.json (son extraction)
- casino_PERFECT_v2.riv (son output)
- Tüm .md belgeler

**ARŞİVLE:**
- Eski build'ler (build_real, build_working, build)
- Eski converter kaynak dosyaları
- Test JSON/RIV dosyaları
- Analiz scriptleri
- Eski CMakeLists varyasyonları

**TAM SİL (şimdilik hiçbir şey):**
- Arşivledikten 1-2 hafta sonra karar verilecek


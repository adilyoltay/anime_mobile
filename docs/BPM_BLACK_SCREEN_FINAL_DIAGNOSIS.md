# BPM Dashboard - Final Root Cause Diagnosis

**Date:** 2025-10-04  
**File:** `output/bpm_dashboard.riv` (5.3KB)  
**Status:** 🟢 **FIXED**

---

## Problem Summary

✅ Animations parse ediliyor (2 adet)  
✅ Animation graph oluşturuluyor (5 KeyedObject, 8 KeyedProperty, 38 KeyFrame)  
✅ AnimationState'ler oluşturuluyor (Beat, Glow)  
✅ AnimationState.animationId property yazılıyor (sırasıyla 0, 1)  
✅ Animasyonlar artboard bileşenlerinden sonra serialize edilirken animationId sıralaması korunuyor

---

## Kök Neden: Animasyon Nesne Sıralaması

PASS 1B 	hiyerarşik animasyonları artboard eklendikten hemen sonra oluşturduğu için `KeyedObject.objectId` değerleri hedef bileşenler streamBde henüz görünmeden serialize ediliyordu. Serializer bu remap denemesini bulamadığı için property'yi tamamen atlıyor, runtime'da animasyonun hedefi bulunamadığından sahne başlamıyor ve Rive Play kilitleniyordu.

```
Önceki çıktı (hata):
  0x0094 : Artboard "Heart Monitor"
  ...
  0x00d0 : LinearAnimation "HeartBeat" (bileşenlerden önce)
  0x0120 : KeyedObject (objectId bulunamadı → property skip)
```

**Çözüm:** PASS 1 döngüsü tamamlanana kadar hiyerarşik animasyonları bekletiyoruz ve artboarddaki komponentler sıralandıktan sonra tek seferde ekliyoruz. Lambda hâlâ tek çağrı garantisiyle çalışıyor; ancak animasyonların artboard-local sırası `hierarchicalAnimationLocalIds` aracılığıyla korunuyor. Serializer tarafında remap mantığı eski davranışına döndü; yani hedef bileşen henüz emilmemişse property atlanıyor ve PASS 3'te gelir gelmez doğru değerle tekrar yazılıyor.

```
Güncel çıktı (düzeltildi):
  0x0094 : Artboard "Heart Monitor"
  0x00d0-0x1200 : component stream
  0x1294 : LinearAnimation "HeartBeat"  ← componentlerden sonra
  0x1454 : LinearAnimation "Ambient"
```

---

## Kanıt

### Analyzer Çıktısı
```
Object type_31 (31) -> ['55:namePropertyKey=HeartBeat', ...]
Object type_61 (61) -> ['138:namePropertyKey=Beat', '149:animationIdPropertyKey=0']
Object type_61 (61) -> ['138:namePropertyKey=Glow', '149:animationIdPropertyKey=1']
```
`animationId` property key (149) artık dosyada yer alıyor ve doğru indeksleri gösteriyor.

### Offset Doğrulaması
```python
with open('output/bpm_dashboard.riv','rb') as f:
    data = f.read()
print(hex(data.find(b'Heart Monitor')))  # 0x94
print(hex(data.find(b'HeartBeat')))      # 0x1294
print(hex(data.find(b'Ambient')))        # 0x1454
```
Animasyon isimleri artık komponent akışından sonra geliyor; animationId indeksleri `animationLocalIdsInOrder` vektöründe doğru sırayla tutulduğu için state machine fallback'leri hâlâ 0/1 değerleriyle eşleşiyor.

### import_test
```
SUCCESS: File imported successfully!
StateMachine #0: 'HeartState'
  States: AnimationState(animationId=0), AnimationState(animationId=1)
```
State machine Rive runtime testinde başarıyla başlıyor.

---

## Uygulanan Düzeltmeler

- `converter/src/universal_builder.cpp`
  - PASS 1 döngüsü boyunca hiyerarşik animasyonlar yalnızca yerel ID listesine alınır; gerçek `builder.addCore` çağrıları PASS 1 tamamlandıktan sonra yapılır.
  - `hierarchicalAnimationLocalIds` vektörü ile JSON'daki animasyon sırası korunur; PASS 1 sonrası `animationLocalIdsInOrder` sonuna eklenir.
  - `lastKeyframe` tanımı eklendi, tekrar tanımlanan sayaçlar kaldırıldı.
- `converter/src/serializer.cpp`
  - `KeyedObject.objectId`, `ClippingShape.sourceId` ve `TextValueRun.styleId` gibi bileşen referansları için remap bulunamadığında property'yi atlama davranışı geri yüklendi; böylece PASS 3 remap'leri harici sahte indekslerle çakışmıyor.
- Doğrulama: `rive_convert_cli`, `analyze_riv.py`, `import_test` komutları `converter/heart_rate_scene.json` üzerinde başarıyla çalıştırıldı.

---

## Takip

- PR notlarında `docs/BPM_DASHBOARD_BLACK_SCREEN_ANALYSIS.md` ile senkronizasyon sağlandı (bkz. ayrı doc güncellemesi).
- Gelecekte hiyerarşik JSON içinde `transitions` verisi geldikçe PASS 1C'de state transition nesneleri üretilmeli (ayrı görev).

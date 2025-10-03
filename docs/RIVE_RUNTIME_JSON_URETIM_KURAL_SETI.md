# Rive Runtime JSON Üretim Kural Seti

Bu kılavuz, converter sözleşmesine ve Rive SDK başlıklarına bire bir uyumlu deterministik ve doğrulanabilir JSON üretimi için tek yetkili referanstır. Tüm kurallar ve örnekler çalışma alanındaki gerçek kodlara dayanır.

## Kaynak-otorite ve Referanslar
- `converter/include/json_loader.hpp` (şema, tipler)
- `converter/src/json_loader.cpp` (parse mantığı, defaultlar)
- `converter/src/core_builder.cpp` (JSON→Core mapping, varsayılan enjeksiyon)
- `converter/src/serializer.cpp` (ToC/bitmap/stream kuralları)
- `converter/src/riv_structure.md` (binary + exact JSON format özeti)
- `include/rive/generated/**/*_base.hpp` (SDK typeKey/propertyKey/field tipi; TEK doğruluk kaynağı)
- `include/rive/animation/loop.hpp` (Loop enum: 0 oneShot, 1 loop, 2 pingPong)

> Uyumsuzlukta SDK başlıkları esastır. Belge/uygulama derhal güncellenir.

---

## 1) Kapsam ve Kullanım Senaryoları
- NL→JSON ajanının üreteceği “simplified Document JSON” formatını tanımlar.
- Desteklenen varlıklar: Artboard, Shapes, Custom Paths (vertices), Text, Animations, State Machines, Events, Bones, Skins.
- Exact round-trip JSON (gelişmiş) desteklenir ancak ayrı format (bkz. §5.2).

## 2) Kaynak-otorite ve Sürümleme
- Tek otorite: `include/rive/generated/**/*_base.hpp`.
- JSON’a sürüm alanı eklenmez (yeni alan uydurulamaz). Sürüm takibi depo/PR düzeyindedir.

## 3) Determinizm İlkeleri
- Alan sırası (kanonik):
  - Artboard: `name → width → height → shapes → customPaths → texts → animations → stateMachines → dataConverters → dataBinds → constraints → events → bones → skins → useHierarchical → hierarchicalShapes`
  - Shape: `type → x → y → width → height → points → cornerRadius → innerRadius → assetId → originX → originY → sourceId → fillRule → clipVisible → fill → stroke`
  - CustomPath: `isClosed → vertices → fillEnabled → hasGradient → gradient → fillColor → strokeEnabled → strokeColor → strokeThickness`
  - Text: `content → x → y → width → height → align → sizing → overflow → wrap → verticalAlign → paragraphSpacing → fitFromBaseline → style`
  - Animation: `name → fps → duration → loop → yKeyframes → scaleKeyframes → opacityKeyframes`
  - StateMachine: `name → inputs → layers → listeners → dataBinds`
- Sayısal biçim: NaN/Inf yok. Gereksiz son sıfırlar yok (1 yerine 1.0 yazmayın).
- Birimler: pozisyon/ölçüler px; rotation/slant derece; duration (anim) frame; duration (transition) ms.
- Null/empty: Sadece sözleşmenin izin verdiği yerlerde; aksi halde alanı hiç yazmayın.
- Diziler deterministik sırada yazılır; giriş sırası korunur.

## 4) Üst Seviye JSON ve Kimliklendirme
- Kök nesne tek obje.
- Tercih edilen format: `artboards: Artboard[]`.
- Legacy kök alanlar parse aşamasında `artboards[0]` içine taşınır (`json_loader.cpp`).
- Simplified formatta explicit id kullanılmaz; isim/indeksleme builder tarafından çözümlenir. Döngüsel referans üretmeyin.

## 5) Çekirdek Kavramlar ve Şema
### 5.1 Simplified (Document) JSON
- Artboard (`ArtboardData`):
  - Zorunlu yazınız: `name`, `width`, `height`.
  - İçerik: `shapes[]`, `customPaths[]`, `texts[]`, `animations[]`, `stateMachines[]`, `dataConverters[]`, `dataBinds[]`, `constraints[]`, `events[]`, `bones[]`, `skins[]`, `useHierarchical`, `hierarchicalShapes[]`.
- Shapes (`ShapeData`):
  - `type`: "rectangle"|"ellipse"|"triangle"|"polygon"|"star"|"image"|"clipping"|"path"
  - `x,y,width,height`; polygon/star: `points`, `cornerRadius`; star: `innerRadius`.
  - image: `assetId`, `originX`, `originY`.
  - clipping: `sourceId`, `fillRule`(0=nonZero,1=evenOdd), `clipVisible`.
  - `fill`: `{ color? | gradient? | feather? | trimPath? }`
  - `stroke`: `{ color, thickness, dash?, trimPath? }`
  - `gradient`: `{ type:"radial"|"linear", stops:[{position,color}] }`
  - `feather`: `{ strength, offsetX, offsetY, inner }`
  - `dash`: `{ length, gap, isPercentage }`
  - `trimPath`: `{ start, end, offset, mode }`
  - Not: TrimPath JSON’da tanımlanabilir; builder’da ilişkili yazım ileriki çalışma (uyumluluk notu).
- CustomPath (`CustomPathData`):
  - `isClosed`, `vertices[{type:"straight"|"cubic", x,y, radius?, inRotation?, inDistance?, outRotation?, outDistance?}]`
  - `fillEnabled`, `strokeEnabled`, `hasGradient`, `gradient`, `fillColor`, `strokeColor`, `strokeThickness`.
- Text (`TextData`):
  - `content,x,y,width,height`
  - `align` 0/1/2, `sizing` 0/1, `overflow` 0/1/2, `wrap` 0/1, `verticalAlign` 0/1/2, `paragraphSpacing`, `fitFromBaseline`
  - `style`: `fontFamily`, `fontSize`, `fontWeight(100–900)`, `fontWidth(50–200)`, `fontSlant(-15..15)`, `lineHeight(-1=auto)`, `letterSpacing`, `color`, `hasStroke`, `strokeColor`, `strokeThickness`
  - HarfBuzz/SheenBidi entegrasyonu: JSON → Core dönüşümünde `CoreBuilder::addCore` metin bileşenlerini (`Text`, `TextValueRun`, `TextStylePaint`) `std::unique_ptr` ile sahiplenir; serializer yalnızca tip/metaveri kullanır. Runtime bellek yönetimi Rive SDK’ya bırakıldığı için bu nesneler builder tarafından serbest bırakılmaz. CLI içindeki `NoOpFactory` gerçek render stub’ları üretir (RenderPaint/RenderPath/RenderBuffer/RenderImage/RenderShader) ve glyph shaping sırasında oluşan veriler güvenle tutulur.
- Animation (`AnimationData`):
  - `name`, `fps:uint`, `duration:uint`, `loop:uint` → Loop: 0=oneShot, 1=loop, 2=pingPong
  - `yKeyframes|scaleKeyframes|opacityKeyframes`: `[{frame:uint, value:number}]`
  - Not: Örnek builder default olarak cubic (68=1) eğri yazar; gelişmiş eğriler için exact mod gerekir.
- StateMachine (`StateMachineData`):
  - `name`
  - `inputs`: `[{name,type:"bool"|"number"|"trigger", defaultValue}]`
  - `layers`: `[{name, states:[{name,type:"entry"|"any"|"exit"|"animation", animationName?}], transitions:[{from,to,duration,conditions?}]}]`
  - `conditions` (opsiyonel): `[{input, op:"=="|"!="|"<"|"<="|">"|">=", value}]`
  - Sistem state’leri Entry/Exit/Any otomatik eklenir; indeksler buna göre hesaplanır (builder).
- Events/Bones/Skins: `json_loader.hpp` tanımlarına uygun olarak desteklenir.

### 5.2 Exact Round-Trip JSON (Gelişmiş)
- Kapı: `__riv_exact__ = true` (bkz. `riv_structure.md`).
- Alanlar: `headerKeys`, `bitmaps`, `objects{typeKey,properties{key,category,value}}`, `objectTerminator`, `tail`.
- Kategoriye göre yazım: `uint/double/color/string/bytes/bool`. Serializer ToC/bitmap doğrular.
- Terminator sonrası chunk’ların (ör. Artboard Catalog) kaybolmaması için `File::stripAssets` terminatöre ulaştığında kalan tüm baytları çıktıya ekler. Stripped `.riv` dosyaları Rive Play’de eksiksiz açılmalıdır.

## 6) Animasyon ve Keyframe
- `fps/duration/loop` zorunlu yazın (defaultlar §10). Zaman birimi: frame.
- Interpolasyon: Simplified JSON’da verilmez; builder default cubic yazar.

## 7) State Machine ve Dış Girdiler
- Input tipleri: bool/number/trigger.
- `animationName` → artboard içi 0-based anim indeksi (builder çözümlemesi).
- Transitions: `duration` ms; `conditions` JSON’da kabul, builder yazımı ileri çalışma.

## 8) Hata Politikası ve Validasyon
- Zorunlu alan eksikse üretimi kesin; hata JSON’u üretin:
  ```json
  { "error": { "code": "MISSING_FIELD", "field": "artboards[0].name" } }
  ```
- Aralık/enumeration kontrolleri:
  - loop ∈ {0,1,2}; align ∈ {0..2}; sizing ∈ {0,1}; overflow ∈ {0..2}; wrap ∈ {0,1}; verticalAlign ∈ {0..2}
  - fontWeight 100–900; fontWidth 50–200; fontSlant -15..15
- Renk formatı: `#RRGGBB` | `#AARRGGBB` | `0xAARRGGBB`. Hatalıysa default kullanın.
- CLI doğrulama (özet): import_test → SUCCESS; `analyze_riv.py` ToC/stream uyumlu olmalı.

## 9) Güvenlik ve Kaynak Sınırları (Guardrails)
- Maks: artboards 16, shapes 500, custom vertices 10k, texts 200, animations 200, stateMachines 50, JSON 2MB.
- Aşıldığında: `{ "error": { "code": "LIMIT_EXCEEDED", "detail": "..." } }` üretin.

## 10) Varsayılanlar ve Belirsizlik Çözümü
- Defaultlar (`json_loader.hpp`):
  - Artboard: name="Artboard", width=400, height=300
  - Shape: width=100, height=100, points=5, innerRadius=0.5, originX/Y=0.5, clipVisible=true
  - Fill: #FFFFFFFF; Stroke: #FF000000; Feather.strength=12; Dash.length=10,gap=10
  - Text: fontSize=24, lineHeight=-1, color #FF000000, fitFromBaseline=true
  - Animation: fps=60, duration=60, loop=1
- Belirsizlik:
  - Ölçü yoksa width/height=100; konum yoksa x=y=0.
  - Renk belirtilmemişse ilgili bölümü yazmayın (builder defaultlarına güvenilebilir).
  - Animation state için `animationName` yoksa hata üretin.

## 11) NL→JSON Üretim Talimatları (Ajan)
- Sadece sözleşme alanlarını kullanın; yeni alan uydurmayın.
- Kanonik alan sırasını koruyun.
- Zorunlu: `artboards[].{name,width,height}`, `shapes[].type`, `animations[].{name,fps,duration,loop}`.
- Varsayılanları §10’a göre doldurun; çözülemeyen belirsizlikte hata üretin.
- Çıktı yalnızca JSON; açıklama/yorum yok.
- Guardrail ihlalinde üretimi kesin (bkz. §9).

## 12) Kalite Kapıları
- Şema kontrolleri (tip/enum/aralık) → geçmeli.
- `import_test` → SUCCESS.
- `analyze_riv.py` ToC/stream farkı yok.
- Rive Play’de render sorunsuz (builder clip/visible/blend varsayılanları yazar).

## 13) Örnekler
### 13.1 Statik + Animasyon
NL: “400x300 ‘Dashboard’; 100x80 kırmızı rectangle (x=150,y=110); ‘Pulse’ 60 fps/60 frame/loop; y:0→50→0.”
```json
{
  "artboards": [
    {
      "name": "Dashboard",
      "width": 400,
      "height": 300,
      "shapes": [
        {
          "type": "rectangle",
          "x": 150,
          "y": 110,
          "width": 100,
          "height": 80,
          "fill": { "color": "#FF0000" }
        }
      ],
      "animations": [
        {
          "name": "Pulse",
          "fps": 60,
          "duration": 60,
          "loop": 1,
          "yKeyframes": [
            { "frame": 0, "value": 0 },
            { "frame": 30, "value": 50 },
            { "frame": 60, "value": 0 }
          ]
        }
      ]
    }
  ]
}
```

### 13.2 Custom Path + Text + State Machine
NL: “360x360 ‘HUD’; üçgen custom path lineer gradient kırmızı→sarı, stroke 2; ‘BPM 75’ ortalı; ‘Wobble’ 60 fps/60 frame/pingPong; opacity 1→0.5→1; SM ‘Main’, bool input ‘armed’, Idle→Animate 300ms.”
```json
{
  "artboards": [
    {
      "name": "HUD",
      "width": 360,
      "height": 360,
      "customPaths": [
        {
          "isClosed": true,
          "vertices": [
            { "type": "straight", "x": 0, "y": 0, "radius": 0 },
            { "type": "straight", "x": 100, "y": 0, "radius": 0 },
            { "type": "straight", "x": 50, "y": 80, "radius": 0 }
          ],
          "fillEnabled": true,
          "hasGradient": true,
          "gradient": {
            "type": "linear",
            "stops": [
              { "position": 0.0, "color": "#FF0000" },
              { "position": 1.0, "color": "#FFFF00" }
            ]
          },
          "strokeEnabled": true,
          "strokeColor": "#000000",
          "strokeThickness": 2
        }
      ],
      "texts": [
        {
          "content": "BPM 75",
          "x": 0,
          "y": 40,
          "width": 200,
          "height": 30,
          "align": 1,
          "sizing": 0,
          "overflow": 0,
          "wrap": 0,
          "verticalAlign": 0,
          "paragraphSpacing": 0,
          "fitFromBaseline": true,
          "style": {
            "fontFamily": "Inter",
            "fontSize": 24,
            "fontWeight": 400,
            "fontWidth": 100,
            "fontSlant": 0,
            "lineHeight": -1,
            "letterSpacing": 0,
            "color": "#000000",
            "hasStroke": false
          }
        }
      ],
      "animations": [
        {
          "name": "Wobble",
          "fps": 60,
          "duration": 60,
          "loop": 2,
          "opacityKeyframes": [
            { "frame": 0, "value": 1.0 },
            { "frame": 30, "value": 0.5 },
            { "frame": 60, "value": 1.0 }
          ]
        }
      ],
      "stateMachines": [
        {
          "name": "Main",
          "inputs": [{ "name": "armed", "type": "bool", "defaultValue": 0 }],
          "layers": [
            {
              "name": "L1",
              "states": [
                { "name": "Idle", "type": "entry" },
                { "name": "Animate", "type": "animation", "animationName": "Wobble" }
              ],
              "transitions": [
                { "from": "Idle", "to": "Animate", "duration": 300 }
              ]
            }
          ]
        }
      ]
    }
  ]
}
```

---

## Bakım ve Güncelleme Politikası
- Kaynak-otorite: `include/rive/generated/**/*_base.hpp` başlıkları.
- Zorunlu güncelleme tetikleyicileri: SDK başlıklarındaki DataBind/DataConverter*/Listener*/StateMachine* değişiklikleri; converter dosyalarında mapping/ID remap değişiklikleri; yeni binding/Listener tipleri.
- PR süreci: “Docs: RIVE_RUNTIME_JSON_URETIM_KURAL_SETI.md updated” kutucuğunu işaretleyin ve değişiklikleri özetleyin.
- Çelişkide SDK esastır; bu doküman derhal güncellenir.

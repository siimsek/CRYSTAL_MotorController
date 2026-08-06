# STM32 MotorKontrol Bellenim ve Kullanım Kılavuzu

Bu belge, **STM32F030C8T6** mikrodenetleyicili motor kontrol kartının bellenim mimarisi, donanım pin bağlantıları, menü navigasyonu, ayar sınırları, alarm sistemleri ve güvenlik kurallarını içeren %100 doğrulanmış teknik kullanım kılavuzudur.

---

## 1. Genel Sistem Mimarisi ve Özellikler

- **Mikrodenetleyici:** STM32F030C8T6 (48 MHz HSI PLL, 64 KB Flash, 8 KB SRAM).
- **Ekran (I2C2):** 0.96" / 1.3" SSD1306 OLED (128x64 piksel, 7-bit I2C Adresi: `0x3C`).
- **EEPROM (I2C1):** 24C02 / 24C04 (7-bit I2C Adresi: `0x50`).
- **Sıcaklık Algılama (ADC_IN2 - PA2):** 100k NTC Termistör + 100k Seri Direnç (Beta: 3950).
- **Akım Algılama (ADC_IN1 - PA1):** ACS712ELCTR-20A-T (100 mV/A Hassasiyet, Güç açılışında Auto-Zero).
- **Güç Kesme Rölesi (PB12):** Omron G2RL-2 (ULN2003A Sürücülü, Fail-Safe Güç Kesme).
- **Uyarı Sinyalleri:** Sesli Buzzer (PA12), Yüksek Sıcaklık Alarm Çıkışı (PA8), Yüksek Akım Alarm Çıkışı (PB15), Durum LED'i (PB0).

---

## 2. Donanım & Pin Bağlantı Haritası

`include/motor_ui_config.h` ve kart şemasına uygun pin eşlemeleri:

| Donanım İşlevi | STM32 Pini | Aktif Seviye / Mod | Açıklama |
| :--- | :--- | :--- | :--- |
| **OLED SCL (I2C2)** | `PB10` | Alternate Function AF1 | OLED Saat Sinyali |
| **OLED SDA (I2C2)** | `PB11` | Alternate Function AF1 | OLED Veri Sinyali |
| **EEPROM SCL (I2C1)**| `PB6` | Alternate Function AF1 | EEPROM Saat Sinyali |
| **EEPROM SDA (I2C1)**| `PB7` | Alternate Function AF1 | EEPROM Veri Sinyali |
| **ACS712 Akım** | `PA1` | Analog Input (ADC_IN1) | Akım Sensörü Girişi (Ofset ~2.675V) |
| **NTC Sıcaklık** | `PA2` | Analog Input (ADC_IN2) | Sıcaklık Sensörü Girişi |
| **DOWN Butonu** | `PA0` | Input (Ext. Pull-Down) | Aktif HIGH, Aşağı / Azalt |
| **BOOT Butonu** | `PA6` | Input (Ext. Pull-Down) | Aktif HIGH, Ayarlar / İptal |
| **UP Butonu** | `PA7` | Input (Ext. Pull-Down) | Aktif HIGH, Yukarı / Artır |
| **OK Butonu** | `PC14` | Input (Ext. Pull-Down) | Aktif HIGH, Seç / Onayla |
| **Röle Çıkışı** | `PB12` | Output (ULN2003A) | HIGH = Motor Güç İzni (Bobin Enerjili) |
| **Buzzer Çıkışı** | `PA12` | Output (ULN2003A) | HIGH = Ses Aktif |
| **Aşırı Sıcaklık** | `PA8` | Output (ULN2003A) | HIGH = Sıcaklık Alarmı Aktif |
| **Aşırı Akım** | `PB15` | Output (ULN2003A) | HIGH = Akım Alarmı Aktif |
| **Durum LED'i** | `PB0` | Output (2N7002) | HIGH = Normal Çalışma (Alarm Yok) |

---

## 3. Buton Kullanımı ve Ekran Navigasyonu

Sistemde 4 adet fiziksel buton bulunur: **OK**, **DOWN**, **BOOT**, **UP**.

### 3.1. Ana Ekran (Main Screen)
- **Ekran İçeriği:** Sol sütunda anlık ölçülen sıcaklık (°C), sağ sütunda anlık ölçülen akım (A) gösterilir.
- **`BOOT` Butonu:** Ayarlar Menüsüne (`AYARLARI`) açar.
- **`OK` / `UP` / `DOWN` Butonları:** Ana ekranda işlevsizdir (yanlışlıkla basmalara karşı korumalı).

### 3.2. Ayarlar Menüsü (Settings Menu)
- **Menü Öğeleri:**
  1. `Sicaklik` (Sıcaklık Eşik Ayarı)
  2. `Akim` (Akım Eşik Ayarı)
  3. `Fabrika Ayarlari` (Fabrika Ayarlarına Dönüş)
  4. `Ana Ekrana Don`
- **`UP` / `DOWN` Butonları:** Menü imlecini (ok işareti) yukarı/aşağı hareket ettirir.
- **`OK` Butonu:** Seçili menü öğesine girer.
- **`BOOT` Butonu:** Yapılan değişiklikleri kaydetmeden doğrudan **Ana Ekrana** döner.

### 3.3. Sıcaklık ve Akım Ayar Ekranları
- **`UP` / `DOWN` Butonları:** Eşik değerini artırır veya azaltır.
  - Basılı tutulduğunda değişim hızı kademeli olarak artar (Hız çarpanları: `1x`, `2x`, `5x`, `10x`, `20x`, `50x`).
- **`OK` Butonu:** Ayarlanan değeri dondurur ve **Çift Onay Adımına** geçer.
- **`BOOT` Butonu:** Ayarı iptal eder ve kaydetmeden **Ayarlar Menüsüne** döner.

### 3.4. Çift Onay Mekanizması (Double Confirmation)
Tüm parametre değişiklikleri ve varsayılana dönme işlemleri 2 kademeli onay gerektirir:
1. **1. Onay Ekranı:** `DEGISIKLIKLERI ONAYLIYOR MUSUNUZ?` (Varsayılan Seçim: `HAYIR`)
2. **2. Onay Ekranı:** `DEGISIKLIKLERDEN EMIN MISINIZ?` (Varsayılan Seçim: `HAYIR`)
- **`UP` / `DOWN`:** `EVET` ve `HAYIR` arasında seçim yapar.
- **`OK`:** `EVET` seçiliyse bir sonraki onay adımına geçer / son adımda EEPROM ve RAM'e kaydeder. `HAYIR` seçiliyse Ayarlar menüsüne döner.
- **`BOOT`:** İşlemi anında iptal eder ve Ayarlar menüsüne döner.

---

## 4. Parametre Aralıkları ve Varsayılan Değerler

Sistemin sınır değerleri, kod karmaşasına girmeden kolayca düzenlenebilmesi için `include/motor_ui_config.h` dosyasının **en tepesindeki** `[ KULLANICI AYARLARI ]` bloğunda toplanmıştır.

| Parametre | İç Birim | Minimum | Maksimum | Varsayılan | İnce Adım | Alarm Histerezisi |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Sıcaklık Eşiği** | `x10` (0.1°C) | `10.0°C` (100) | `70.0°C` (700) | `40.0°C` (400) | `1.0°C` (10) | `1.0°C` (10) |
| **Akım Eşiği** | `x100 A` (10mA) | `1000mA` (100) | `5000mA` (500) | `1500mA` (150) | `10mA` (1) | `100mA` (10) |

*Not: Sıcaklık veya akım belirtilen eşik değerine ulaştığında alarm tetiklenir. Alarmın temizlenmesi için değerin Histerezis miktarı kadar eşiğin altına düşmesi gerekir.*

---

## 5. Alarm Sistemleri ve İşitilebilir Paternler

Sistemde 4 farklı alarm durumu tanımlıdır. Alarm durumunda ekran ilgili alarm arayüzüne kilitlenir ve yanıp sönen uyarı ikonu gösterilir.

### 5.1. Buzzer Ses Paternleri
Buzzer non-blocking durum makinesi ile özel ritmik sesler üretir:
- **Sıcaklık Alarmı (`._._`):** Kısa - Uzun - Kısa - Uzun
- **Akım Alarmı (`..__..`):** Kısa - Kısa - Uzun - Uzun - Kısa - Kısa
- **İkisi / Sensör Arızası (`______.......`):** 6 Uzun - 7 Kısa

#### Zamanlama Değerleri:
- Kısa Ses (`.`): `120 ms`
- Uzun Ses (`_`): `480 ms`
- Sembol Arası Boşluk: `120 ms`
- Patern Tekrar Boşluğu: `850 ms`

---

## 6. Güvenlik ve Röle Mantığı (Fail-Safe)

1. **Güç Kesme Yetkisi:** Herhangi bir sıcaklık/akım alarmı veya sensör kopukluğu/hatasında röle **derhal kesilir** (`PB12 = LOW`).
2. **Kendiliğinden Başlamama (Latch):** Alarm durumu ortadan kalktığında motor **otomatik olarak çalışmaya başlamaz**. Uygulamanın `MotorUI_SetMotorRunRequest(true)` fonksiyonu ile yeniden onay vermesi şarttır.
3. **Güvenli Başlatma Gecikmesi:** Cihaz ilk açıldığında sensörlerin oturması için ilk `1000 ms` boyunca motor çalıştırma yetkisi engellenir.
4. **Sensör Doğrulama:** ADC ölçümleri ham değer aralığı dışına çıkarsa (`NTC` veya `ACS712` kopması) sensör hatası (`ALARM_SENSOR_FAULT`) tetiklenir ve röle emniyete alınır.

---

## 7. Derleme ve Yükleme Talimatları

### 7.1. PlatformIO ile Derleme
```bash
./build.sh
# veya
pio run
```
**Çıktı Dosyaları:**
- `.pio/build/motor_kontrol/firmware.elf`
- `.pio/build/motor_kontrol/firmware.bin`

### 7.2. ST-Link ile Yükleme
```bash
./flash.sh
```

#### SWD Bağlantı Tablosu:
| ST-Link V2 | STM32F030 Kart |
| :--- | :--- |
| **SWCLK** | `PA14` |
| **SWDIO** | `PA13` |
| **GND** | `GND` |
| **NRST** | `NRST` (Opsiyonel) |
| **3.3V** | `3.3V` (Kart harici besleniyorsa bağlamayın) |

---

## 8. Doğrulama ve Test Komutları

Yazılım değişikliklerinin ardından aşağıdaki 3 kontrol sırasıyla çalıştırılmalıdır:

```bash
# 1. Aşama 1-4 Koşullu Derleme Kontrolü
./tools/check_all_stages.sh

# 2. OLED Arayüz Kilit ve Koordinat Kontrolü
python3 ./tools/check_ui_lock.py

# 3. Bellenim Derleme Kontrolü
pio run
```

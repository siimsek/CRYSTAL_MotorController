# CRYSTAL STM32 Endüstriyel Motor Kontrol Sistemi

[![STM32F030](https://img.shields.io/badge/MCU-STM32F030C8T6-blue.svg)](https://www.st.com/)
[![Display](https://img.shields.io/badge/Display-SSD1306_OLED_128x64-brightgreen.svg)](https://github.com/olikraus/u8g2)
[![Stage Compliance](https://img.shields.io/badge/Stages-1--4_Verified-success.svg)](#8-doğrulama-ve-testler)
[![License](https://img.shields.io/badge/License-Proprietary-orange.svg)]()

**CRYSTAL Motor Controller**, endüstriyel ortamlarda çalışan pompaların ve motorların sıcaklık ile akım değerlerini anlık olarak izleyen, aşırı yük ve ısınma durumlarında gücü keserek sistemi emniyete alan yüksek kararlılığa sahip STM32 tabanlı akıllı sürücü bellenimidir.

---

## 📌 İçindekiler
- [1. Öne Çıkan Özellikler ve İnovasyonlar](#1-öne-çıkan-özellikler-ve-i̇novasyonlar)
- [2. Sistem Mimarisi](#2-sistem-mimarisi)
- [3. Donanım & Pin Bağlantı Haritası](#3-donanım--pin-bağlantı-haritası)
- [4. Menü Navigasyonu ve Kullanım Kılavuzu](#4-menü-navigasyonu-ve-kullanım-kılavuzu)
- [5. Alarm ve Ses Paternleri](#5-alarm-ve-ses-paternleri)
- [6. Güvenlik ve Röle Mantığı (Fail-Safe)](#6-güvenlik-ve-röle-mantığı-fail-safe)
- [7. Derleme ve Yükleme](#7-derleme-ve-yükleme)
- [8. Doğrulama ve Testler](#8-doğrulama-ve-testler)

---

## 1. Öne Çıkan Özellikler ve İnovasyonlar

Sistemin sahada 7/24 kesintisiz ve parazitsiz çalışması için geliştirilen gelişmiş bellenim teknolojileri:

1. 🚀 **5 Saniyelik Açılış Kalkış Akımı Koruması (`ALERT_UI_ARM_MS = 5000U`):** Güç verildikten sonraki ilk 5 saniye boyunca pompanın yüksek kalkış (inrush) akımından kaynaklanabilecek hatalı alarm ve röle kesmeleri bastırılır.
2. 🔄 **Otomatik Akım Sensör Sıfırlama (Idle Auto-Zero Drift Correction):** Motor dururken ACS712 akım sensörünün 0A voltaj referansı arka planda EMA filtresi (`alpha = 0.05f`) ile sürekli izlenir. Pano içi sıcaklık değişimlerinin neden olduğu termal voltaj kaymaları (drift) %100 yok edilir.
3. 🛡️ **Röle Titremesi Koruması (Relay Chatter Guard - 3s):** Sensör değerlerinin tam sınırda gürültü sebebiyle dalgalanıp röleyi saniyede defalarca açıp kapatmasını engeller. Röle durum değiştirdikten sonra yeniden AÇILMA (ON) öncesinde en az 3 saniye beklenir. Emergency kesmeler ise anında (0 ms) uygulanır.
4. 💡 **OLED Ekran Koruyucu (Auto-Dimmer / Burn-In Protection):** 5 dakika boyunca tuş aktivitesi yoksa ve alarm bulunmuyorsa OLED kontrastı %5 seviyesine düşürülür. Tuş basımı veya alarm anında anında %100 parlaklığa uyanır. Ekran ömrü 5 katına çıkar.
5. 📊 **Ekranda Değer Titremesi Önleme (Display Hysteresis):** Göstergeye ±0.2°C ve ±20mA histerezis uygulanır. Küçük parazit dalgalanmaları ekrandaki son rakamı titretmez, rakamlar yağ gibi akar. Güvenlik kesmeleri arka planda %100 filtrelenmemiş anlık verilerle 200 ms hızında çalışır.
6. 🔑 **10s Hızlı Fabrika Ayarlarına Dönüş Kısayolu:** Cihaz çalışırken `BOOT` + `OK` tuşlarına 10 saniye basılı tutulduğunda tüm ayarlar fabrika değerlerine (`40.0°C`, `1500mA`) döndürülür ve EEPROM'a kaydedilir.
7. 🔊 **Öncelikli Buzzer Alarm Hafızası (`g_first_cut_alarm`):** Sıcaklık için sürekli tek ton ses, akım için ritmik kesikli ton ses verilir. Birden fazla alarm anında ilk gücü kesen alarmın ses ritmi korunur.

---

## 2. Sistem Mimarisi

- **Mikrodenetleyici:** STM32F030C8T6 (48 MHz HSI PLL, 64 KB Flash, 8 KB SRAM).
- **Ekran:** 0.96" / 1.3" SSD1306 OLED (128x64 piksel, I2C Adresi: `0x3C`).
- **EEPROM:** 24C02 / 24C04 (I2C Adresi: `0x50`).
- **Sensörler:** 100k NTC Termistör (PA2 - ADC IN2) + ACS712ELCTR-20A (PA1 - ADC IN1).
- **Sürücü Mimarisi:** Single-File MVC Yapısı (`src/motor_ui.c`, `include/motor_ui_config.h`).
- **Arayüz Doğruluğu:** `reference/OLED_Projesi.oled.json` piksel koordinat doğrusu korunmuştur.

---

## 3. Donanım & Pin Bağlantı Haritası

`include/motor_ui_config.h` içerisindeki pin konfigürasyon haritası:

| İşlev | STM32 Pini | Mod / Aktif Seviye | Açıklama |
| :--- | :--- | :--- | :--- |
| **OLED SCL** | `PB10` | I2C2 AF1 | OLED Saat Sinyali |
| **OLED SDA** | `PB11` | I2C2 AF1 | OLED Veri Sinyali |
| **EEPROM SCL** | `PB6` | I2C1 AF1 | EEPROM Saat Sinyali |
| **EEPROM SDA** | `PB7` | I2C1 AF1 | EEPROM Veri Sinyali |
| **ACS712 Akım** | `PA1` | ADC_IN1 | Akım Sensörü Girişi |
| **NTC Sıcaklık** | `PA2` | ADC_IN2 | Sıcaklık Sensörü Girişi |
| **DOWN Butonu** | `PA0` | Input (Pull-Down) | Aktif HIGH |
| **BOOT Butonu** | `PA6` | Input (Pull-Down) | Aktif HIGH |
| **UP Butonu** | `PA7` | Input (Pull-Down) | Aktif HIGH |
| **OK Butonu** | `PC14` | Input (Pull-Down) | Aktif HIGH |
| **Röle Çıkışı** | `PB12` | Output (ULN2003A) | HIGH = Motor Güç İzni |
| **Buzzer Çıkışı**| `PA12` | Output (ULN2003A) | HIGH = Ses Aktif |
| **Durum LED'i** | `PB0` | Output (2N7002) | HIGH = Normal Çalışma |

---

## 4. Menü Navigasyonu ve Kullanım Kılavuzu

### 4.1. Buton İşlevleri
- **`OK`:** Menüde seçim yapar / Ayarı onaylar / 10s basılı tutulursa alarm sesini susturur.
- **`UP` / `DOWN`:** Menüde gezinir / Ayar ekranında değeri artırır/azaltır (Basılı tutulursa `1x-50x` hızlanır).
- **`BOOT`:** Ayarlar menüsüne girer / Yapılan değişikliği kaydetmeden iptal edip çıkar.
- **`BOOT` + `OK` (10 Saniye):** Hızlı Fabrika Ayarlarına Dönüş kısayolu.

### 4.2. Çift Onay Adımları
Tüm parametre değişiklikleri 2 aşamalı onay gerektirir:
1. `DEGISIKLIKLERI ONAYLIYOR MUSUNUZ?`
2. `DEGISIKLIKLERDEN EMIN MISINIZ?`

---

## 5. Alarm ve Ses Paternleri

| Alarm Tipi | Tetiklenme Koşulu | Ekran Görünümü | Buzzer Ritim Paterni |
| :--- | :--- | :--- | :--- |
| **Sıcaklık Alarmı** | Ölçülen ≥ Ayarlanan Eşik | Yanıp Sönen Derece İkonu | Kesintisiz Sürekli Ton (`"C"`) |
| **Akım Alarmı** | Ölçülen ≥ Ayarlanan Eşik | Yanıp Sönen Akım İkonu | Kesikli Ritmik Ton (`"____"`) |
| **Çift Alarm** | Hem Temp Hem Current Alert | Çift Yanıp Sönen İkon | İlk Gücü Kesin Alarm Tonu |
| **Sensör Arızası** | ADC Aralık Dışı (Kopuk/Kısa) | `"ERROR"` Görünümü | Çift Alarm Tonu |

---

## 6. Güvenlik ve Röle Mantığı (Fail-Safe)

1. **Emniyetli Açılış:** İlk 1000 ms röle kapatılır. İlk 5000 ms alarm ve güç kesme bastırılır.
2. **Derhal Kesme:** Alarm veya sensör hatasında röle **0 ms** içinde gücü keser.
3. **Mandallama (Latch):** Alarm düzelse dahi motor otomatik olarak tekrar başlamaz; manuel çalıştırma isteği şarttır.
4. **Relay Chatter Guard:** Röle açılmadan önce en az **3 saniye** beklenir.

---

## 7. Derleme ve Yükleme

### PlatformIO ile Derleme:
```bash
./build.sh
# veya
pio run
```

### ST-Link V2 ile Kart Flaşlama:
```bash
./flash.sh
```

---

## 8. Doğrulama ve Testler

Her kod değişikliğinden sonra projedeki otomatik doğrulama betikleri çalıştırılmalıdır:

```bash
# 1. Stage 1-4 Koşullu Derleme Kontrolü
bash tools/check_all_stages.sh

# 2. OLED Arayüz Kilit & Koordinat Kontrolü
python3 tools/check_ui_lock.py

# 3. PlatformIO Bellenim Derlemesi
pio run
```

---

## 📜 Lisans & Sorumluluk
Bu proje **CRYSTAL Motor Controller** endüstriyel cihazı için özel olarak geliştirilmiştir. İzinsiz kopyalanamaz ve dağıtılamaz.

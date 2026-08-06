# CRYSTAL STM32 Endüstriyel Motor Kontrol Sistemi

[![STM32F030](https://img.shields.io/badge/MCU-STM32F030C8T6-blue.svg)](https://www.st.com/)
[![Display](https://img.shields.io/badge/Display-SSD1306_OLED_128x64-brightgreen.svg)](https://github.com/olikraus/u8g2)
[![Stage Compliance](https://img.shields.io/badge/Stages-1--4_Verified-success.svg)](#8-doğrulama-ve-testler)
[![License](https://img.shields.io/badge/License-Proprietary-orange.svg)]()

Bu belge, **STM32F030C8T6** mikrodenetleyicili motor kontrol kartının bellenim mimarisi, donanım pin bağlantıları, menü navigasyonu, ayar sınırları, alarm sistemleri, güvenlik kuralları ve endüstriyel inovasyonlarını içeren **%100 doğrulanmış teknik kullanım ve geliştirici kılavuzudur**.

---

## 📌 İçindekiler
- [1. Genel Sistem Mimarisi ve Özellikler](#1-genel-sistem-mimarisi-ve-özellikler)
- [2. Gelişmiş Endüstriyel İnovasyonlar](#2-gelişmiş-endüstriyel-i̇novasyonlar)
- [3. Donanım & Pin Bağlantı Haritası](#3-donanım--pin-bağlantı-haritası)
- [4. Buton Kullanımı ve Ekran Navigasyonu](#4-buton-kullanımı-ve-ekran-navigasyonu)
- [5. Parametre Aralıkları ve Varsayılan Değerler](#5-parametre-aralıkları-ve-varsayılan-değerler)
- [6. Alarm Sistemleri ve Ses Paternleri](#6-alarm-sistemleri-ve-ses-paternleri)
- [7. Güvenlik ve Röle Mantığı (Fail-Safe)](#7-güvenlik-ve-röle-mantığı-fail-safe)
- [8. Derleme, Yükleme ve ST-Link Bağlantısı](#8-derleme-yükleme-ve-st-link-bağlantısı)
- [9. Doğrulama ve Test Komutları](#9-doğrulama-ve-test-komutları)

---

## 1. Genel Sistem Mimarisi ve Özellikler

- **Mikrodenetleyici:** STM32F030C8T6 (48 MHz HSI PLL, 64 KB Flash, 8 KB SRAM).
- **Ekran (I2C2):** 0.96" / 1.3" SSD1306 OLED (128x64 piksel, 7-bit I2C Adresi: `0x3C`).
- **EEPROM (I2C1):** 24C02 / 24C04 (7-bit I2C Adresi: `0x50`).
- **Sıcaklık Algılama (ADC_IN2 - PA2):** 100k NTC Termistör + 100k Seri Direnç (Beta: 3950).
- **Akım Algılama (ADC_IN1 - PA1):** ACS712ELCTR-20A-T (100 mV/A Hassasiyet, Güç açılışında Auto-Zero).
- **Güç Kesme Rölesi (PB12):** Omron G2RL-2 (ULN2003A Sürücülü, Fail-Safe Güç Kesme).
- **Uyarı Sinyalleri:** Sesli Buzzer (PA12), Yüksek Sıcaklık Alarm Çıkışı (PA8), Yüksek Akım Alarm Çıkışı (PB15), Durum LED'i (PB0).
- **Yazılım Mimarisi:** Single-File MVC Yapısı (`src/motor_ui.c`, `include/motor_ui_config.h`).

---

## 2. Gelişmiş Endüstriyel İnovasyonlar

Sistemin sahada 7/24 kesintisiz, parazitsiz ve uzun ömürlü çalışması için entegre edilen teknikler:

1. 🚀 **5 Saniyelik Açılış Kalkış Akımı Koruması (`ALERT_UI_ARM_MS = 5000U`):** Güç verildikten sonraki ilk 5 saniye boyunca pompanın yüksek kalkış (inrush) akımından kaynaklanabilecek hatalı alarm ve röle kesmeleri bastırılır.
2. 🔄 **Otomatik Akım Sensör Sıfırlama (Idle Auto-Zero Drift Correction):** Motor dururken (`g_motor_power_permitted == false`) ACS712 akım sensörünün 0A voltaj referansı arka planda EMA filtresi (`alpha = 0.05f`) ile sürekli izlenir. Sıcaklık ve ortam değişimlerinin neden olduğu 0A voltaj kaymaları (drift) %100 yok edilir.
3. 🛡️ **Röle Titremesi Koruması (Relay Chatter Guard - 3s):** Sensör değerlerinin tam sınırda gürültü sebebiyle dalgalanıp röleyi saniyede defalarca açıp kapatmasını ve kontakları yakmasını önler. Röle durum değiştirdikten sonra yeniden AÇILMA (ON) öncesinde en az **3 saniye** beklenir. Acil durum kesmeleri ise anında (0 ms) uygulanır.
4. 💡 **OLED Ekran Koruyucu (Auto-Dimmer / Burn-In Protection):** 5 dakika boyunca herhangi bir tuşa basılmazsa ve aktif bir alarm yoksa OLED kontrast seviyesi **%5 parlaklığa (`OLED_CONTRAST_DIM = 13U`)** düşürülür. Herhangi bir tuşa basıldığında veya yeni bir alarm/hata geldiğinde ekran anında **%100 parlaklığa (`OLED_CONTRAST_HIGH = 255U`)** çıkar. Ekran ömrü 5 katına çıkar ve piksel yanması engellenir.
5. 📊 **Ekranda Değer Titremesi Önleme (Display Hysteresis):** Ekranda gösterilen sıcaklık ve akım değerlerine **±0.2°C (`DISPLAY_TEMP_HYST_X10 = 2U`)** ve **±20mA (`DISPLAY_CURRENT_HYST_X100 = 2U`)** gösterge histerezisi uygulanır. Küçük parazit dalgalanmaları ekrandaki son rakamı titretmez. Arka plandaki güvenlik ve röle kesme mantığı ise %100 filtrelenmemiş anlık değerlerle 200 ms hızında çalışmaya devam eder.
6. 🎯 **0.8A Boştaki Akım Sıfırlama Filtresi (`ACS_CURRENT_DEADBAND_X100 = 80U`):** Motor boştayken veya dururken ACS712 sensöründen gelen ufak gürültülerin ekranda rastgele akım gibi görünmesini engellemek için 0.8 Amper (800 mA) altındaki tüm okumalar otomatik olarak `0.0 A` (`0U`) kabul edilir.
7. 🔑 **Hızlı Fabrika Ayarlarına Dönüş Kısayolu (`BOOT` + `OK` 10s):** Cihaz hangi ekranda olursa olsun `BOOT` ve `OK` tuşlarına aynı anda 10 saniye boyunca basılı tutulursa tüm sıcaklık ve akım eşik değerleri sıfırlanarak fabrika varsayılanlarına (`40.0°C`, `1500mA`) döndürülür, EEPROM'a otomatik yazılır.

---

## 3. Donanım & Pin Bağlantı Haritası

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

## 4. Buton Kullanımı ve Ekran Navigasyonu

Sistemde 4 adet fiziksel buton bulunur: **OK**, **DOWN**, **BOOT**, **UP**.

### 4.1. Ana Ekran (Main Screen)
- **Ekran İçeriği:** Sol sütunda anlık ölçülen sıcaklık (°C), sağ sütunda anlık ölçülen akım (A) gösterilir.
- **`BOOT` Butonu:** Ayarlar Menüsünü (`AYARLARI`) açar.
- **`OK` / `UP` / `DOWN` Butonları:** Ana ekranda işlevsizdir (yanlışlıkla basmalara karşı korumalı).

### 4.2. Ayarlar Menüsü (Settings Menu)
- **Menü Öğeleri:**
  1. `Sicaklik` (Sıcaklık Eşik Ayarı)
  2. `Akim` (Akım Eşik Ayarı)
  3. `Fabrika Ayarlari` (Fabrika Ayarlarına Dönüş)
  4. `Ana Ekrana Don`
- **`UP` / `DOWN` Butonları:** Menü imlecini (ok işareti) yukarı/aşağı hareket ettirir.
- **`OK` Butonu:** Seçili menü öğesine girer.
- **`BOOT` Butonu:** Yapılan değişiklikleri kaydetmeden doğrudan **Ana Ekrana** döner.

### 4.3. Sıcaklık ve Akım Ayar Ekranları
- **`UP` / `DOWN` Butonları:** Eşik değerini artırır veya azaltır.
  - Basılı tutulduğunda değişim hızı kademeli olarak artar (Hız çarpanları: `1x`, `2x`, `5x`, `10x`, `20x`, `50x`).
- **`OK` Butonu:** Ayarlanan değeri dondurur ve **Çift Onay Adımına** geçer.
- **`BOOT` Butonu:** Ayarı iptal eder ve kaydetmeden **Ayarlar Menüsüne** döner.

### 4.4. Çift Onay Mekanizması (Double Confirmation)
Tüm parametre değişiklikleri ve varsayılana dönme işlemleri 2 kademeli onay gerektirir:
1. **1. Onay Ekranı:** `DEGISIKLIKLERI ONAYLIYOR MUSUNUZ?` (Varsayılan Seçim: `HAYIR`)
2. **2. Onay Ekranı:** `DEGISIKLIKLERDEN EMIN MISINIZ?` (Varsayılan Seçim: `HAYIR`)
- **`UP` / `DOWN`:** `EVET` ve `HAYIR` arasında seçim yapar.
- **`OK`:** `EVET` seçiliyse bir sonraki onay adımına geçer / son adımda EEPROM ve RAM'e kaydeder. `HAYIR` seçiliyse Ayarlar menüsüne döner.
- **`BOOT`:** İşlemi anında iptal eder ve Ayarlar menüsüne döner.

---

## 5. Parametre Aralıkları ve Varsayılan Değerler

Sistemin sınır değerleri `include/motor_ui_config.h` dosyasında tanımlıdır:

| Parametre | İç Birim | Minimum | Maksimum | Varsayılan | İnce Adım | Alarm Histerezisi |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Sıcaklık Eşiği** | `x10` (0.1°C) | `10.0°C` (100) | `70.0°C` (700) | `40.0°C` (400) | `1.0°C` (10) | `1.0°C` (10) |
| **Akım Eşiği** | `x100 A` (10mA) | `1000mA` (100) | `5000mA` (500) | `1500mA` (150) | `10mA` (1) | `100mA` (10) |

---

## 6. Alarm Sistemleri ve Ses Paternleri

Sistemde 4 farklı alarm durumu tanımlıdır. Alarm durumunda ekran ilgili alarm arayüzüne kilitlenir ve yanıp sönen uyarı ikonu gösterilir.

### 6.1. Buzzer Ses Paternleri ve Alarm Önceliği
Buzzer non-blocking durum makinesi ile özel ritmik sesler üretir:
- **Sıcaklık Alarmı (`"C"`):** Kesintisiz, sürekli tek ton ses (`_______________`).
- **Akım Alarmı (`"____"`):** Kesikli ton (350 ms ses / 350 ms sessizlik).
- **İlk Alarm Önceliği (`g_first_cut_alarm`):** Birden fazla alarm aynı anda veya sırayla geldiğinde, buzzer ilk gücü kesen ve tetiklenen alarmın ses ritmini çalmaya devam eder.

---

## 7. Güvenlik ve Röle Mantığı (Fail-Safe)

1. **Güç Kesme Yetkisi:** Herhangi bir sıcaklık/akım alarmı veya sensör kopukluğu/hatasında (`ERROR`) röle **derhal kesilir** (`PB12 = LOW`).
2. **Kendiliğinden Başlamama (Latch):** Alarm durumu ortadan kalktığında veya sensör düzelse bile motor **otomatik olarak çalışmaya başlamaz**. Uygulamanın `MotorUI_SetMotorRunRequest(true)` fonksiyonu ile yeniden onay vermesi şarttır.
3. **5 Saniyelik Açılış Kalkış Akımı Koruması (`ALERT_UI_ARM_MS = 5000U`):** Cihaz ilk açıldığında pompanın yüksek ilk kalkış akımı çekmesi nedeniyle yaşanabilecek hatalı alarm tetiklemelerini önlemek için güç verildikten sonraki ilk 5 saniye boyunca uyarı ve güç kesme devre dışı tutulur.
4. **Sensör Doğrulama & Hata Koruması:** ADC okumaları ham değer aralığı dışına çıkarsa (`NTC` veya `ACS712` kopması) ekranda `"ERROR"` gösterilir, sensör hatası (`ALARM_SENSOR_FAULT`) tetiklenir ve röle gücü derhal kesilerek kilitlenir.

---

## 8. Derleme, Yükleme ve ST-Link Bağlantısı

### 8.1. PlatformIO ile Derleme
```bash
./build.sh
# veya
pio run
```
**Çıktı Dosyaları:**
- `.pio/build/motor_kontrol/firmware.elf`
- `.pio/build/motor_kontrol/firmware.bin`

### 8.2. ST-Link V2 ile Yükleme
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

## 9. Doğrulama ve Test Komutları

Yazılım değişikliklerinin ardından aşağıdaki 3 kontrol sırasıyla çalıştırılmalıdır:

```bash
# 1. Aşama 1-4 Koşullu Derleme Kontrolü
bash tools/check_all_stages.sh

# 2. OLED Arayüz Kilit ve Koordinat Kontrolü
python3 tools/check_ui_lock.py

# 3. Bellenim Derleme Kontrolü
pio run
```

---

## 📜 Lisans & Sorumluluk
Bu proje **CRYSTAL Motor Controller** endüstriyel cihazı için özel olarak geliştirilmiştir. İzinsiz kopyalanamaz ve dağıtılamaz.

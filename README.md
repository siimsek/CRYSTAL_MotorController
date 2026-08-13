# CRYSTAL STM32 Motor Kontrol Bellenimi

[![STM32F030](https://img.shields.io/badge/MCU-STM32F030C8T6-blue.svg)](https://www.st.com/)
[![Display](https://img.shields.io/badge/Display-SSD1306_OLED-green.svg)](https://github.com/olikraus/u8g2)
[![Build Status](https://img.shields.io/badge/Build-Passing-success.svg)]()

Bu bellenim, **STM32F030C8T6** mikrodenetleyicisi üzerinde çalışan, endüstriyel elektrik motorları ve pompalar için tasarlanmış yüksek kararlılıklı kontrol ve koruma sistemidir. Akım ve sıcaklık parametrelerini gerçek zamanlı izler, tanımlanan eşiklerin aşılması durumunda röle çıkışını keserek donanımı emniyete alır.

---

## 📌 İçindekiler
- [1. Donanım ve Sistem Özellikleri](#1-donanım-ve-sistem-özellikleri)
- [2. Donanım Pin Eşleme Tablosu](#2-donanım-pin-eşleme-tablosu)
- [3. Saha Kullanım Kılavuzu (Operatör Rehberi)](#3-saha-kullanım-kılavuzu-operatör-rehberi)
- [4. Sinyal İşleme ve Bellenim Fonksiyonları](#4-sinyal-işleme-ve-bellenim-fonksiyonları)
- [5. Güvenlik ve Röle Kontrol Mantığı](#5-güvenlik-ve-röle-kontrol-mantığı)
- [6. Alarm Durumları ve Sinyal Paternleri](#6-alarm-durumları-ve-sinyal-paternleri)
- [7. Parametre Aralıkları](#7-parametre-aralıkları)
- [8. Derleme, Yükleme ve Testler](#8-derleme-yükleme-ve-testler)

---

## 1. Donanım ve Sistem Özellikleri

- **Mikrodenetleyici:** STM32F030C8T6 (48 MHz ARM Cortex-M0, 64 KB Flash, 8 KB SRAM).
- **Görüntüleme:** SSD1306 0.96" / 1.3" OLED Ekran (128x64 piksel, I2C2).
- **Kalıcı Bellek:** 24C02 / 24C04 EEPROM (I2C1).
- **Sıcaklık Ölçümü:** 100k NTC Termistör + 100k Seri Gerilim Bölücü (Beta: 3950, PA2 / ADC_IN2).
- **Akım Ölçümü:** ACS712ELCTR-20A-T Akım Sensörü (100 mV/A, PA1 / ADC_IN1).
- **Güç Kontrolü:** Omron G2RL-2 Röle Çıkışı (PB12, ULN2003A Sürücü).
- **Uyarı Sinyalleri:** Piezo Buzzer (PA12), Aşırı Sıcaklık Çıkışı (PA8), Aşırı Akım Çıkışı (PB15), Durum LED'i (PB0).

---

## 2. Donanım Pin Eşleme Tablosu

| İşlev | STM32 Pini | Mod / Aktif Seviye | Açıklama |
| :--- | :--- | :--- | :--- |
| **OLED SCL** | `PB10` | I2C2 AF1 | Ekran Saat Sinyali |
| **OLED SDA** | `PB11` | I2C2 AF1 | Ekran Veri Sinyali |
| **EEPROM SCL** | `PB6` | I2C1 AF1 | EEPROM Saat Sinyali |
| **EEPROM SDA** | `PB7` | I2C1 AF1 | EEPROM Veri Sinyali |
| **ACS712 Akım** | `PA1` | ADC_IN1 | Akım Girişi (0.8A Ölü Bantlı) |
| **NTC Sıcaklık** | `PA2` | ADC_IN2 | Sıcaklık Girişi |
| **DOWN Butonu** | `PA0` | Input (Ext. Pull-Down) | Aktif HIGH, Aşağı / Azalt |
| **BOOT Butonu** | `PA6` | Input (Ext. Pull-Down) | Aktif HIGH, Ayarlar / İptal |
| **UP Butonu** | `PA7` | Input (Ext. Pull-Down) | Aktif HIGH, Yukarı / Artır |
| **OK Butonu** | `PC14` | Input (Ext. Pull-Down) | Aktif HIGH, Seç / Onayla |
| **Röle Çıkışı** | `PB12` | Output (ULN2003A) | HIGH = bobin enerjili = NO kontak kapalı (güç yolu) |
| **SWDIO / SWCLK** | `PA13` / `PA14` | SWD | ST-Link programlama; uygulamada kullanılmaz |
| **Buzzer Çıkışı** | `PA12` | Output (ULN2003A) | HIGH = Ses Aktif |
| **Aşırı Sıcaklık** | `PA8` | Output (ULN2003A) | HIGH = Sıcaklık Alarm Çıkışı |
| **Aşırı Akım** | `PB15` | Output (ULN2003A) | HIGH = Akım Alarm Çıkışı |
| **Durum LED'i** | `PB0` | Output (2N7002) | HIGH = Normal Çalışma |

---

## 3. Saha Kullanım Kılavuzu (Operatör Rehberi)

### 3.1. Cihazın İlk Çalıştırılması ve Başlatma
1. Cihaza güç verildiğinde **3 saniye** boyunca logolu açılış ekranı (Splash Screen) görüntülenir.
2. Açılış sonrasında cihaz otomatik olarak **Ana Ekrana** geçer.
3. Bu kart motor starter değildir; ana sistemin güç yolunda **ara kesicidir**. NO röle açılışta (ACS sıfır + ~1 s) kapanır ve alarm gelene kadar kapalı kalır. Motoru ana sistemin kendi modları çalıştırır.
4. İlk **1 saniye** ACS 0A kalibrasyonu ve röle güvenli başlangıçtır (`RELAY_SAFE_STARTUP_MS`). İlk **5 saniye** boyunca alarm tetikleme pasiftir (`ALERT_UI_ARM_MS`); bu pencerede röle kapanmış olabilir.

### 3.2. Ana Ekran Kullanımı
- **Ekran Görünümü:** Sol sütunda anlık ölçülen sıcaklık (°C), sağ sütunda anlık ölçülen akım (A) görüntülenir.
- **Menüye Giriş:** `BOOT` butonuna bir kez basılarak Ayarlar Menüsüne geçilir.
- **Kaza Önleme Kilit:** Yanlışlıkla basılmaları engellemek amacıyla Ana Ekranda `OK`, `UP` ve `DOWN` tuşları pasiftir.

### 3.3. Eşik Değerlerinin Değiştirilmesi
1. Ana ekrandayken `BOOT` butonuna basarak `AYARLARI` menüsüne girin.
2. `UP` ve `DOWN` butonlarını kullanarak `Sicaklik` veya `Akim` seçeneğinin üzerine gelin ve `OK` butonuna basın.
3. `UP` / `DOWN` butonları ile hedef eşik değerini ayarlayın (Butona basılı tutulduğunda değişim hızı artar).
4. İstenen değere ulaşıldığında `OK` butonuna basarak onay adımına geçin.
5. **Çift Onay Adımları:**
   - 1. Onay: `DEGISIKLIKLERI ONAYLIYOR MUSUNUZ?` -> `UP`/`DOWN` ile `EVET` seçip `OK` butonuna basın.
   - 2. Onay: `DEGISIKLIKLERDEN EMIN MISINIZ?` -> `UP`/`DOWN` ile `EVET` seçip `OK` butonuna basın.
6. Yeni ayarlar EEPROM belleğe kaydedilir ve sistem Ayarlar menüsüne döner.
7. *İptal Etme:* Herhangi bir aşamada `BOOT` butonuna basarak işlemi kaydetmeden iptal edebilirsiniz.

### 3.4. Fabrika Ayarlarına Dönüş
- **Yöntem 1 (Menü Üzerinden):** `AYARLARI` -> `Fabrika Ayarlari` -> `OK` -> Çift Onay adımlarını uygulayın.
- **Yöntem 2 (Tuş Kısayolu):** Cihaz hangi ekranda olursa olsun `BOOT` ve `OK` butonlarına aynı anda **10 saniye** boyunca basılı tutun. Cihaz anında varsayılan değerlere (`71.0 °C`, `1550 mA`) sıfırlanır ve EEPROM’a yazılır.

### 3.5. Alarm Durumunda Yapılacaklar
- Sıcaklık veya akım eşik değeri aşıldığında ekranda ilgili uyarı ikonu yanıp söner, sesli alarm (buzzer) çalar ve motor rölesi derhal kesilir.
- **Ses Susturma:** Alarm çalarken `OK` butonuna **5 saniye** basılı tutarak sesli uyarıyı susturabilirsiniz (`ALARM_MUTE_HOLD_MS`). Röle kesik kalır; susturma alarmı iptal etmez.
- **Sistemi Yeniden Başlatma (Akım Arızası):** Akım sebebiyle güç kesildiğinde, cihaz kendini güvenli modda kilitler. Motorun tekrar çalışması için sistemi yeniden başlatmanız (gücü kesip vermeniz) veya sıfırlamanız gerekir.
- **Sistemi Yeniden Başlatma (Sıcaklık Arızası):** Sıcaklık sebebiyle güç kesildiğinde, ölçülen sıcaklık ayarlanan eşik değerinin **%20 altına düştüğünde** alarm otomatik olarak kalkar ve motor yeniden çalışmaya başlar.

### 3.6. Otomatik Ekran Koruyucu (Dimmer)
- Cihazda 5 dakika boyunca tuşa basılmazsa ekran parlaklığı otomatik olarak `%5` seviyesine düşer.
- Herhangi bir tuşa basıldığında veya yeni bir alarm durumunda ekran parlaklığı anında `%100` seviyesine döner.

---

## 4. Sinyal İşleme ve Bellenim Fonksiyonları

### 4.1. Akım Sensörü Otomatik Sıfırlama (Idle Auto-Zero)
Motor dururken (`g_motor_power_permitted == false`) ACS712 sensörünün 0A voltaj referansı arka planda bir Üstel Hareketli Ortalama (EMA) filtresi (`alpha = 0.05f`) ile takip edilir. Ortam ve pano sıcaklığından kaynaklanan 0A voltaj kaymaları (drift) dinamik olarak kompanze edilir. Motor çalıştırıldığında sıfır noktası dondurulur.

### 4.2. Boştaki Akım Bastırma (0.8A Deadband)
0.8 A (`ACS_CURRENT_DEADBAND_X100 = 80`) altındaki akım okumaları `0.0 A` olarak işlenir. Ayar tavanı 2.33 A’dır; 0.8 A ölü bant ölçüm gürültüsü içindir, eşik minimumu (780 mA) ile karıştırılmamalıdır.

### 4.3. Gösterge Histerezisi (Display Hysteresis)
Ekranda gösterilen sayısal değerlerdeki parazit kaynaklı titreşimleri engellemek amacıyla sıcaklıkta `±0.2°C` (`DISPLAY_TEMP_HYST_X10 = 2U`), akımda ise `±20 mA` (`DISPLAY_CURRENT_HYST_X100 = 2U`) gösterge histerezisi uygulanır. Güvenlik ve alarm kontrol döngüleri ham veri ile 200 ms hızında çalışmaya devam eder.

### 4.4. Ekran Parlaklığı ve Otomatik Karartma (Auto-Dimmer)
Sistemde 5 dakika boyunca tuş aktivitesi gerçekleşmezse ve aktif alarm yoksa OLED kontrastı `%5` seviyesine (`OLED_CONTRAST_DIM = 13U`) düşürülerek ekran yanmaları (burn-in) önlenir. Herhangi bir tuşa basılması veya alarm tetiklenmesi durumunda kontrast anında `%100` seviyesine (`OLED_CONTRAST_HIGH = 255U`) çıkarılır.

---

## 5. Güvenlik ve Röle Kontrol Mantığı

### 5.1. Röle Titremesi Koruması (Relay Chatter Guard)
Ölçülen değerlerin alarm sınırında dalgalanması sonucu röle kontaklarının yüksek frekansta açılıp kapanmasını önlemek amacıyla, röle durum değiştirdikten sonra yeniden kapanması (güç vermesi) için en az **3 saniye** (`RELAY_CHATTER_GUARD_MS = 3000U`) beklenmesi zorunludur. Tehlike anında rölenin kesilmesi ise **0 ms** gecikmeyle derhal gerçekleşir.

### 5.2. Ara Kesici ve Açılış
GPIO/init anında röle bobini enerjisizdir (NO kontak açık). ACS 0A kalibrasyonu bu pencerede alınır. `RELAY_SAFE_STARTUP_MS` (1 s) ve geçerli sensörler sonrası bobin enerjilenir, kontak kapanır. Alarm veya sensör hatasında kontak derhal açılır. Akım/sensör alarmı kalkınca yol kendiliğinden kapanmaz; sıcaklık alarmında ölçüm eşiğin %20 altına inince yol yeniden kapanabilir.

### 5.3. İlk Kalkış Akımı Bastırma (Startup Inrush Suppression)
Sistem açılışını takip eden ilk **5 saniye** (`ALERT_UI_ARM_MS = 5000U`) boyunca alarm tetiklenmez. Röle bu süreden önce (~1 s) kapanabilir; 1–5 s aralığında aşırı akım/sıcaklık henüz yolu kesmez.

### 5.4. Sensör Arıza Koruması ve Otomatik Kilit (Latch)
ADC okumalarının tanımlı aralık dışına çıkması (NTC kopması/kısa devresi veya ACS712 hatası) durumunda ekranda `"ERROR"` mesajı görüntülenir, röle derhal açılır (yol kesilir). Buzzer bu durumda çalmaz (`SENSOR_FAULT_USES_BOTH_PATTERN = 0`). Sensör normale dönse dahi yol kendiliğinden kapanmaz; güç kes-ver gerekir.

---

## 6. Alarm Durumları ve Sinyal Paternleri

| Alarm Tipi | Tetiklenme Şartı | Ekran Durumu | Buzzer Ses Paterni |
| :--- | :--- | :--- | :--- |
| **Aşırı Sıcaklık** | Ölçülen ≥ Sıcaklık Eşiği | Derece Sembolü Blink | Sürekli ton (`"C"`) |
| **Aşırı Akım** | Ölçülen ≥ Akım Eşiği | Akım Sembolü Blink | Kesikli 350 ms (`"____"`) |
| **İkisi birden** | Sıcaklık + akım | Her iki ikon | İlk kesen alarmın paterni (`g_first_cut_alarm`) |
| **Sensör Arızası** | ADC ölçümü sınır dışı | `"ERROR"` | Sessiz (röle kesik, her iki alarm GPIO aktif) |

*Not: Alarm tipi değişince patern sıfırlanıp yeni patern ilk sembolden başlar. Semboller: `C` = sürekli, `_` = 350 ms ses, sembol/patern aralığı 350 ms.*

---

## 7. Parametre Aralıkları

Kaynak: `include/motor_ui_config.h`. İç birim: sıcaklık ×10 (°C), akım ×100 (A).

| Parametre | İç birim | Minimum | Maksimum | Varsayılan | Adım | Alarm histerezisi |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Sıcaklık eşiği** | `0.1 °C` | `35.5 °C` | `106.5 °C` | `71.0 °C` | `1.0 °C` | `%20` altına inince temizlenir |
| **Akım eşiği** | `10 mA` | `780 mA` | `2330 mA` | `1550 mA` | `10 mA` | `100 mA` |

---

## 8. Derleme, Yükleme ve Testler

### 8.1. Derleme ve Flaşlama
```bash
# PlatformIO Derleme
pio run

# Takılı programcıya göre otomatik yükleme
#   Nucleo/onboard ST-Link -> pio upload
#   Bağımsız ST-Link V2 USB -> OpenOCD / st-flash
./flash.sh
# İki programcı takılıysa: STLINK_SERIAL=<seri> ./flash.sh
```

SWD: `PA13` SWDIO, `PA14` SWCLK, GND, 3.3V. NRST opsiyonel. Programcı USB’de görünüyor ama MCU yanıt vermiyorsa bu dört teli kontrol edin.

### 8.2. Otomatik Doğrulama Betikleri
```bash
# Stage 1-4 Koşullu Derleme Kontrolü
bash tools/check_all_stages.sh

# OLED Grafik ve Koordinat Kilit Kontrolü
python3 tools/check_ui_lock.py
```

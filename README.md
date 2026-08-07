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
| **Röle Çıkışı** | `PB12` | Output (ULN2003A) | HIGH = Motor Güç İzni |
| **Buzzer Çıkışı** | `PA12` | Output (ULN2003A) | HIGH = Ses Aktif |
| **Aşırı Sıcaklık** | `PA8` | Output (ULN2003A) | HIGH = Sıcaklık Alarm Çıkışı |
| **Aşırı Akım** | `PB15` | Output (ULN2003A) | HIGH = Akım Alarm Çıkışı |
| **Durum LED'i** | `PB0` | Output (2N7002) | HIGH = Normal Çalışma |

---

## 3. Saha Kullanım Kılavuzu (Operatör Rehberi)

### 3.1. Cihazın İlk Çalıştırılması ve Başlatma
1. Cihaza güç verildiğinde **3 saniye** boyunca logolu açılış ekranı (Splash Screen) görüntülenir.
2. Açılış sonrasında cihaz otomatik olarak **Ana Ekrana** geçer.
3. İlk 5 saniye boyunca elektrik motorunun demeraj akımına karşı koruma süresi aktif kalır.

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
- **Yöntem 2 (Tuş Kısayolu):** Cihaz hangi ekranda olursa olsun `BOOT` ve `OK` butonlarına aynı anda **10 saniye** boyunca basılı tutun. Cihaz anında varsayılan değerlere (`40.0 °C`, `1500 mA`) sıfırlanır ve belleğe işlenir.

### 3.5. Alarm Durumunda Yapılacaklar
- Sıcaklık veya akım eşik değeri aşıldığında ekranda ilgili uyarı ikonu yanıp söner, sesli alarm (buzzer) çalar ve motor rölesi derhal kesilir.
- **Ses Susturma:** Alarm çalarken `OK` butonuna **10 saniye** basılı tutarak sesli uyarıyı susturabilirsiniz.
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
Motor dururken veya hafif yükte oluşan hat gürültülerinin ölçümü etkilemesini önlemek amacıyla 0.8 A (`800 mA`) altındaki akım okumaları bellenim tarafından `0.0 A` olarak işlenir.

### 4.3. Gösterge Histerezisi (Display Hysteresis)
Ekranda gösterilen sayısal değerlerdeki parazit kaynaklı titreşimleri engellemek amacıyla sıcaklıkta `±0.2°C` (`DISPLAY_TEMP_HYST_X10 = 2U`), akımda ise `±20 mA` (`DISPLAY_CURRENT_HYST_X100 = 2U`) gösterge histerezisi uygulanır. Güvenlik ve alarm kontrol döngüleri ham veri ile 200 ms hızında çalışmaya devam eder.

### 4.4. Ekran Parlaklığı ve Otomatik Karartma (Auto-Dimmer)
Sistemde 5 dakika boyunca tuş aktivitesi gerçekleşmezse ve aktif alarm yoksa OLED kontrastı `%5` seviyesine (`OLED_CONTRAST_DIM = 13U`) düşürülerek ekran yanmaları (burn-in) önlenir. Herhangi bir tuşa basılması veya alarm tetiklenmesi durumunda kontrast anında `%100` seviyesine (`OLED_CONTRAST_HIGH = 255U`) çıkarılır.

---

## 5. Güvenlik ve Röle Kontrol Mantığı

### 5.1. Röle Titremesi Koruması (Relay Chatter Guard)
Ölçülen değerlerin alarm sınırında dalgalanması sonucu röle kontaklarının yüksek frekansta açılıp kapanmasını önlemek amacıyla, röle durum değiştirdikten sonra yeniden kapanması (güç vermesi) için en az **3 saniye** (`RELAY_CHATTER_GUARD_MS = 3000U`) beklenmesi zorunludur. Tehlike anında rölenin kesilmesi ise **0 ms** gecikmeyle derhal gerçekleşir.

### 5.2. İlk Kalkış Akımı Bastırma (Startup Inrush Suppression)
Elektrik motoru veya pompa ilk çalıştırıldığında çekilen yüksek demeraj (inrush) akımının hatalı alarmlara yol açmaması için sistem açılışını takip eden ilk **5 saniye** (`ALERT_UI_ARM_MS = 5000U`) boyunca alarm tetikleme ve güç kesme mekanizmaları pasif tutulur.

### 5.3. Sensör Arıza Koruması ve Otomatik Kilit (Latch)
ADC okumalarının tanımlı aralık dışına çıkması (NTC kopması/kısa devresi veya ACS712 hatası) durumunda ekranda `"ERROR"` mesajı görüntülenir, röle gücü derhal kesilir. Sensör değerleri normale dönse dahi motor otomatik olarak başlamaz; tekrar başlatma onayı gereklidir.

---

## 6. Alarm Durumları ve Sinyal Paternleri

| Alarm Tipi | Tetiklenme Şartı | Ekran Durumu | Buzzer Ses Paterni |
| :--- | :--- | :--- | :--- |
| **Aşırı Sıcaklık** | Ölçülen ≥ Sıcaklık Eşiği | Derece Sembolü Blink | Sürekli Tek Ton (`"C"`) |
| **Aşırı Akım** | Ölçülen ≥ Akım Eşiği | Akım Sembolü Blink | Kesikli Ritmik Ton (`"____"`) |
| **Sensör Arızası** | ADC Ölçümü Sınır Dışı | `"ERROR"` Görünümü | Kesikli Ritmik Ton |

*Not: Birden fazla alarm aynı anda oluştuğunda sesli uyarı ilk tetiklenen alarmın tonunda kilitlenir (`g_first_cut_alarm`).*

---

## 7. Parametre Aralıkları

| Parametre | Birim | Minimum | Maksimum | Varsayılan | Adım | Histerezis |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Sıcaklık Eşiği** | `0.1 °C` | `10.0 °C` | `70.0 °C` | `40.0 °C` | `1.0 °C` | `1.0 °C` |
| **Akım Eşiği** | `10 mA` | `1000 mA` | `5000 mA` | `1500 mA` | `10 mA` | `100 mA` |

---

## 8. Derleme, Yükleme ve Testler

### 8.1. Derleme ve Flaşlama
```bash
# PlatformIO Derleme
pio run

# ST-Link V2 Yükleme
./flash.sh
```

### 8.2. Otomatik Doğrulama Betikleri
```bash
# Stage 1-4 Koşullu Derleme Kontrolü
bash tools/check_all_stages.sh

# OLED Grafik ve Koordinat Kilit Kontrolü
python3 tools/check_ui_lock.py
```

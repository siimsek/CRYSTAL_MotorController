# AGENTS.md — AI Kodlama Araçları İçin Zorunlu Kurallar

Bu dosya Cursor, OpenCode, Codex ve benzeri ajanlar tarafından proje üzerinde değişiklik yapılmadan önce okunmalıdır.

## 1. Değişmez arayüz kuralı

`reference/OLED_Projesi.oled.json` nihai arayüz kaynağıdır.

Aşağıdakiler kullanıcı açıkça yeni bir arayüz vermedikçe değiştirilemez:

- Ekran sayısı
- Ekran adları
- X/Y koordinatları
- Font seçimleri
- Font boyutları
- Çizgi başlangıç ve bitiş koordinatları
- Bitmap verileri
- Bitmap genişlik ve yükseklikleri
- Bitmap X/Y koordinatları
- Menü satırlarının yerleri
- Onay ekranlarının yerleri

İzin verilenler:

- Dinamik metin içeriğini değiştirmek
- `current` yerine `set_current` gibi yanlış değişken bağlantılarını düzeltmek
- Seçili okun görünürlüğünü durum değişkeniyle kontrol etmek
- Alarm bitmapini aynı yerde blink ettirmek
- Ayar değerini aynı metin alanında blink ettirmek

## 2. Güvenlik kuralı

- Alarm aktifken röle motor izni veremez.
- Sensörler geçerli değilken Stage 4 röle motor izni veremez.
- `MotorUI_SetMotorRunRequest(true)` alarm güvenliğini bypass edemez.
- Alarm varken `g_motor_run_requested` düşürülür; alarm kalkınca motor kendiliğinden başlamaz.
- Açılışta motor kesik (`MOTOR_POWER_CUT_RELAY_LEVEL`); NO/NC `MOTOR_POWER_*` ile ayarlanır, hardcode edilmez.
- Röle çıkışını test amacıyla sürekli aktif yapan geçici kod commit edilmez.
- `MOTOR_UI_STAGE` kendiliğinden 4 yapılmaz.
- Motor ve röle pinleri tahmin edilmez.
- STM32 pini röle bobinine doğrudan bağlanmış gibi dokümantasyon yazılmaz.
- OLED: `u8g2` page buffer (`_1` + FirstPage/NextPage); full-frame `_f` kullanılmaz (F030 4KB SRAM).
- EXTI: projede tek `HAL_GPIO_EXTI_Callback` → `MotorUI_ButtonIRQ` yalnız.

## 3. Kesme kuralı

ISR içinde şunlar yapılmaz:

- OLED çizimi
- I2C transferi
- EEPROM yazımı
- ADC ortalaması
- `HAL_Delay()`
- Buzzer paterni döngüsü
- Röle durum makinesi

EXTI callback yalnız `MotorUI_ButtonIRQ()` çağırır. Debounce ve hold işlemi `MotorUI_Task()` içinde yapılır.

## 4. Zamanlama kuralı

- Buzzer ve blink için bloklayıcı gecikme kullanılmaz.
- `HAL_GetTick()` tabanlı non-blocking durum makinesi korunur.
- Uzun süren yeni işler küçük adımlara bölünür.

## 5. Birim kuralı

- Sıcaklık iç birimi `x10`'dur: `400 = 40.0C`.
- Akım iç birimi `x100 A`'dır: `150 = 1.50A = 1500mA`.
- Akım ayar ekranında mA gösterilir.
- Ana ekranda ölçülen akım A gösterilir.
- EEPROM sürümü değiştirilmeden birim anlamı değiştirilemez.

## 6. Ayar aralıkları

- Sıcaklık: 10.0C–70.0C, varsayılan 40.0C.
- Akım: 1000mA–5000mA, varsayılan 1500mA.
- Tek basış sıcaklık adımı 0.1C.
- Tek basış akım adımı 10mA.
- Hold katsayıları 1-2-5-10-20-50.

## 7. Buton davranışı

- Ana ekran + BOOT -> Ayarlar.
- Ana ekran + OK -> işlem yok.
- Ayarlar + OK -> seç.
- Ayarlar + BOOT -> Ana ekran.
- Set ekranı + ilk OK -> düzenleme.
- Set ekranı + ikinci OK -> onay1.
- Set/onay ekranı + BOOT -> kaydetmeden Ayarlar.
- Alarm ekranında butonlar alarmı bypass edemez.

## 8. Alarm paternleri

- Sıcaklık: `._._`
- Akım: `..__..`
- Birleşik: `______.......`
- Alarm tipi değişince patern sıfırlanıp yeni patern ilk sembolden başlamalıdır.

## 9. Donanım bilinmeyenleri / ölçüm gerektirenler

Şemadan pin eşlemesi `motor_ui_config.h` ve `PROJECT_STATUS.yaml` içine işlenmiştir.
Aşağıdakiler ölçülmeden veya parça işaretinden okunmadan kesin kabul edilemez / doldurulamaz:

- ACS bölücü veya filtre oranı (`ACS_SENSOR_MV_PER_ADC_MV_NUM/DEN`)
- NTC bağlantı yönü (ADC ölçümü ile; kör ters çevrilmez)
- Röle NO/NC motor güç yolu (`MOTOR_POWER_ALLOW/CUT_RELAY_LEVEL`)
- EEPROM modeli, I2C adresi, page size (`EEPROM_ENABLE` kapalı kalsın)
- Pompa nominal / kalkış / sıkışma akımı

Bu bilgiler için `PROJECT_STATUS.yaml` güncellenmelidir.

## 10. Kabul testleri

Her işten sonra en az:

1. Stage 1 koşullu derleme
2. Stage 2 koşullu derleme
3. Stage 3 koşullu derleme
4. Stage 4 koşullu derleme
5. Arayüz koordinat karşılaştırması
6. Alarm geçiş testi
7. BOOT iptal testi
8. Hold sınır testi
9. Röle alarm override testi

uygulanmalıdır.

## 11. Dosya sorumlulukları

- Donanım eşleme: `motor_ui_config.h`
- Kamu API: `motor_ui.h`
- İş mantığı ve Çizimler: `motor_ui.c` (Single-File MVC yapısında, tüm bitmapler dahil)
- I2C portu: `u8g2_stm32_port.c`
- Kullanıcı entegrasyonu: `main_kullanim_ornegi.c`
- Güncel bilinmeyenler: `PROJECT_STATUS.yaml`
- Arayüz kaynak doğrusu: `reference/OLED_Projesi.oled.json`

## 12. Yasaklanan otomatik değişiklikler

- OLED tasarımını “daha güzel” yapmak
- Metinleri yeniden hizalamak
- Bitmapleri küçültmek veya taşımak
- Çift onayı tek onaya düşürmek
- BOOT iptalini kaldırmak
- Alarmda röleyi yeniden açmak
- Sensör hatasını yok saymak
- Stage 4'ü varsayılan yapmak
- Bilinmeyen pinleri varsaymak

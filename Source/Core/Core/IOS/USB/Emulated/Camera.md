#### Support:
- [x] Yu-Gi-Oh! 5D's: Duel Transer  / 640x480
- [x] Your Shape / 320x240
- [x] Fit in Six / 320x240
- [?] Racket Sports Party / 160x120

#### QTMultimedia:
```
git clone https://github.com/dolphin-emu/qsc.git _qsc
cd _qsc
py -m pip install -r requirements.txt
$env:Path += ';C:\Program Files\Git\usr\bin\'
```
```diff
diff --git a/examples/dolphin-x64.yml b/examples/dolphin-x64.yml
index 5486190..a895eaf 100644
--- a/examples/dolphin-x64.yml
+++ b/examples/dolphin-x64.yml
@@ -29,7 +29,6 @@ configure:
         - qtlocation
         - qtlottie
         - qtmqtt
-        - qtmultimedia
         - qtnetworkauth
         - qtopcua
         - qtpositioning
@@ -42,7 +41,6 @@ configure:
         - qtsensors
         - qtserialbus
         - qtserialport
-        - qtshadertools
         - qtspeech
         - qttools
         - qttranslations
@@ -53,13 +51,13 @@ configure:
         - qtwebsockets
         - qtwebview
     feature:
-        concurrent: false
+        concurrent: true
         dbus: false
         gif: false
         ico: false
         imageformat_bmp: false
         jpeg: false
-        network: false
+        network: true
         printsupport: false
         qmake: false
         sql: false
```
```
py -m qsc examples\dolphin-x64.yml
```
